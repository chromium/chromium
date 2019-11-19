// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflate_stream.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_inflater.h"
#include "net/websockets/websocket_stream.h"

class GURL;

namespace net {

namespace {

const int kWindowBits = 15;
const size_t kChunkSize = 4 * 1024;

}  // namespace

WebSocketDeflateStream::WebSocketDeflateStream(
    std::unique_ptr<WebSocketStream> stream,
    const WebSocketDeflateParameters& params,
    std::unique_ptr<WebSocketDeflatePredictor> predictor)
    : stream_(std::move(stream)),
      deflater_(params.client_context_take_over_mode()),
      inflater_(kChunkSize, kChunkSize),
      reading_state_(NOT_READING),
      writing_state_(NOT_WRITING),
      current_reading_opcode_(WebSocketFrameHeader::kOpCodeText),
      current_writing_opcode_(WebSocketFrameHeader::kOpCodeText),
      predictor_(std::move(predictor)) {
  DCHECK(stream_);
  DCHECK(params.IsValidAsResponse());
  int client_max_window_bits = 15;
  if (params.is_client_max_window_bits_specified()) {
    DCHECK(params.has_client_max_window_bits_value());
    client_max_window_bits = params.client_max_window_bits();
  }
  deflater_.Initialize(client_max_window_bits);
  inflater_.Initialize(kWindowBits);
}

WebSocketDeflateStream::~WebSocketDeflateStream() = default;

int WebSocketDeflateStream::ReadFrames(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    CompletionOnceCallback callback) {
  read_callback_ = std::move(callback);
  inflater_outputs_.clear();
  int result = stream_->ReadFrames(
      frames, base::BindOnce(&WebSocketDeflateStream::OnReadComplete,
                             base::Unretained(this), base::Unretained(frames)));
  if (result < 0)
    return result;
  DCHECK_EQ(OK, result);
  DCHECK(!frames->empty());

  return InflateAndReadIfNecessary(frames);
}

int WebSocketDeflateStream::WriteFrames(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    CompletionOnceCallback callback) {
  deflater_outputs_.clear();
  int result = Deflate(frames);
  if (result != OK)
    return result;
  if (frames->empty())
    return OK;
  return stream_->WriteFrames(frames, std::move(callback));
}

void WebSocketDeflateStream::Close() { stream_->Close(); }

std::string WebSocketDeflateStream::GetSubProtocol() const {
  return stream_->GetSubProtocol();
}

std::string WebSocketDeflateStream::GetExtensions() const {
  return stream_->GetExtensions();
}

void WebSocketDeflateStream::OnReadComplete(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    int result) {
  if (result != OK) {
    frames->clear();
    std::move(read_callback_).Run(result);
    return;
  }

  int r = InflateAndReadIfNecessary(frames);
  if (r != ERR_IO_PENDING)
    std::move(read_callback_).Run(r);
}

int WebSocketDeflateStream::Deflate(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_write;
  // Store frames of the currently processed message if writing_state_ equals to
  // WRITING_POSSIBLY_COMPRESSED_MESSAGE.
  std::vector<std::unique_ptr<WebSocketFrame>> frames_of_message;
  for (size_t i = 0; i < frames->size(); ++i) {
    DCHECK(!(*frames)[i]->header.reserved1);
    if (!WebSocketFrameHeader::IsKnownDataOpCode((*frames)[i]->header.opcode)) {
      frames_to_write.push_back(std::move((*frames)[i]));
      continue;
    }
    if (writing_state_ == NOT_WRITING)
      OnMessageStart(*frames, i);

    std::unique_ptr<WebSocketFrame> frame(std::move((*frames)[i]));
    predictor_->RecordInputDataFrame(frame.get());

    if (writing_state_ == WRITING_UNCOMPRESSED_MESSAGE) {
      if (frame->header.final)
        writing_state_ = NOT_WRITING;
      predictor_->RecordWrittenDataFrame(frame.get());
      frames_to_write.push_back(std::move(frame));
      current_writing_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
    } else {
      if (frame->payload &&
          !deflater_.AddBytes(
              frame->payload,
              static_cast<size_t>(frame->header.payload_length))) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "deflater_.AddBytes() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }
      if (frame->header.final && !deflater_.Finish()) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "deflater_.Finish() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }

      if (writing_state_ == WRITING_COMPRESSED_MESSAGE) {
        if (deflater_.CurrentOutputSize() >= kChunkSize ||
            frame->header.final) {
          int result = AppendCompressedFrame(frame->header, &frames_to_write);
          if (result != OK)
            return result;
        }
        if (frame->header.final)
          writing_state_ = NOT_WRITING;
      } else {
        DCHECK_EQ(WRITING_POSSIBLY_COMPRESSED_MESSAGE, writing_state_);
        bool final = frame->header.final;
        frames_of_message.push_back(std::move(frame));
        if (final) {
          int result = AppendPossiblyCompressedMessage(&frames_of_message,
                                                       &frames_to_write);
          if (result != OK)
            return result;
          frames_of_message.clear();
          writing_state_ = NOT_WRITING;
        }
      }
    }
  }
  DCHECK_NE(WRITING_POSSIBLY_COMPRESSED_MESSAGE, writing_state_);
  frames->swap(frames_to_write);
  return OK;
}

void WebSocketDeflateStream::OnMessageStart(
    const std::vector<std::unique_ptr<WebSocketFrame>>& frames,
    size_t index) {
  WebSocketFrame* frame = frames[index].get();
  current_writing_opcode_ = frame->header.opcode;
  DCHECK(current_writing_opcode_ == WebSocketFrameHeader::kOpCodeText ||
         current_writing_opcode_ == WebSocketFrameHeader::kOpCodeBinary);
  WebSocketDeflatePredictor::Result prediction =
      predictor_->Predict(frames, index);

  switch (prediction) {
    case WebSocketDeflatePredictor::DEFLATE:
      writing_state_ = WRITING_COMPRESSED_MESSAGE;
      return;
    case WebSocketDeflatePredictor::DO_NOT_DEFLATE:
      writing_state_ = WRITING_UNCOMPRESSED_MESSAGE;
      return;
    case WebSocketDeflatePredictor::TRY_DEFLATE:
      writing_state_ = WRITING_POSSIBLY_COMPRESSED_MESSAGE;
      return;
  }
  NOTREACHED();
}

int WebSocketDeflateStream::AppendCompressedFrame(
    const WebSocketFrameHeader& header,
    std::vector<std::unique_ptr<WebSocketFrame>>* frames_to_write) {
  const WebSocketFrameHeader::OpCode opcode = current_writing_opcode_;
  scoped_refptr<IOBufferWithSize> compressed_payload =
      deflater_.GetOutput(deflater_.CurrentOutputSize());
  if (!compressed_payload.get()) {
    DVLOG(1) << "WebSocket protocol error. "
             << "deflater_.GetOutput() returns an error.";
    return ERR_WS_PROTOCOL_ERROR;
  }
  deflater_outputs_.push_back(compressed_payload);
  auto compressed = std::make_unique<WebSocketFrame>(opcode);
  compressed->header.CopyFrom(header);
  compressed->header.opcode = opcode;
  compressed->header.final = header.final;
  compressed->header.reserved1 =
      (opcode != WebSocketFrameHeader::kOpCodeContinuation);
  compressed->payload = compressed_payload->data();
  compressed->header.payload_length = compressed_payload->size();

  current_writing_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
  predictor_->RecordWrittenDataFrame(compressed.get());
  frames_to_write->push_back(std::move(compressed));
  return OK;
}

int WebSocketDeflateStream::AppendPossiblyCompressedMessage(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    std::vector<std::unique_ptr<WebSocketFrame>>* frames_to_write) {
  DCHECK(!frames->empty());

  const WebSocketFrameHeader::OpCode opcode = current_writing_opcode_;
  scoped_refptr<IOBufferWithSize> compressed_payload =
      deflater_.GetOutput(deflater_.CurrentOutputSize());
  if (!compressed_payload.get()) {
    DVLOG(1) << "WebSocket protocol error. "
             << "deflater_.GetOutput() returns an error.";
    return ERR_WS_PROTOCOL_ERROR;
  }
  deflater_outputs_.push_back(compressed_payload);

  uint64_t original_payload_length = 0;
  for (size_t i = 0; i < frames->size(); ++i) {
    WebSocketFrame* frame = (*frames)[i].get();
    // Asserts checking that frames represent one whole data message.
    DCHECK(WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode));
    DCHECK_EQ(i == 0,
              WebSocketFrameHeader::kOpCodeContinuation !=
              frame->header.opcode);
    DCHECK_EQ(i == frames->size() - 1, frame->header.final);
    original_payload_length += frame->header.payload_length;
  }
  if (original_payload_length <=
      static_cast<uint64_t>(compressed_payload->size())) {
    // Compression is not effective. Use the original frames.
    for (size_t i = 0; i < frames->size(); ++i) {
      std::unique_ptr<WebSocketFrame> frame = std::move((*frames)[i]);
      predictor_->RecordWrittenDataFrame(frame.get());
      frames_to_write->push_back(std::move(frame));
    }
    frames->clear();
    return OK;
  }
  auto compressed = std::make_unique<WebSocketFrame>(opcode);
  compressed->header.CopyFrom((*frames)[0]->header);
  compressed->header.opcode = opcode;
  compressed->header.final = true;
  compressed->header.reserved1 = true;
  compressed->payload = compressed_payload->data();
  compressed->header.payload_length = compressed_payload->size();

  predictor_->RecordWrittenDataFrame(compressed.get());
  frames_to_write->push_back(std::move(compressed));
  return OK;
}

int WebSocketDeflateStream::Inflate(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  std::vector<std::unique_ptr<WebSocketFrame>> frames_passed;
  frames->swap(frames_passed);
  for (size_t i = 0; i < frames_passed.size(); ++i) {
    std::unique_ptr<WebSocketFrame> frame(std::move(frames_passed[i]));
    frames_passed[i] = nullptr;
    DVLOG(3) << "Input frame: opcode=" << frame->header.opcode
             << " final=" << frame->header.final
             << " reserved1=" << frame->header.reserved1
             << " payload_length=" << frame->header.payload_length;

    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode)) {
      frames_to_output.push_back(std::move(frame));
      continue;
    }

    if (reading_state_ == NOT_READING) {
      if (frame->header.reserved1)
        reading_state_ = READING_COMPRESSED_MESSAGE;
      else
        reading_state_ = READING_UNCOMPRESSED_MESSAGE;
      current_reading_opcode_ = frame->header.opcode;
    } else {
      if (frame->header.reserved1) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "Receiving a non-first frame with RSV1 flag set.";
        return ERR_WS_PROTOCOL_ERROR;
      }
    }

    if (reading_state_ == READING_UNCOMPRESSED_MESSAGE) {
      if (frame->header.final)
        reading_state_ = NOT_READING;
      current_reading_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
      frames_to_output.push_back(std::move(frame));
    } else {
      DCHECK_EQ(reading_state_, READING_COMPRESSED_MESSAGE);
      if (frame->payload &&
          !inflater_.AddBytes(
              frame->payload,
              static_cast<size_t>(frame->header.payload_length))) {
        DVLOG(1) << "WebSocket protocol error. "
                 << "inflater_.AddBytes() returns an error.";
        return ERR_WS_PROTOCOL_ERROR;
      }
      if (frame->header.final) {
        if (!inflater_.Finish()) {
          DVLOG(1) << "WebSocket protocol error. "
                   << "inflater_.Finish() returns an error.";
          return ERR_WS_PROTOCOL_ERROR;
        }
      }
      // TODO(yhirano): Many frames can be generated by the inflater and
      // memory consumption can grow.
      // We could avoid it, but avoiding it makes this class much more
      // complicated.
      while (inflater_.CurrentOutputSize() >= kChunkSize ||
             frame->header.final) {
        size_t size = std::min(kChunkSize, inflater_.CurrentOutputSize());
        auto inflated =
            std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodeText);
        scoped_refptr<IOBufferWithSize> data = inflater_.GetOutput(size);
        inflater_outputs_.push_back(data);
        bool is_final = !inflater_.CurrentOutputSize() && frame->header.final;
        if (!data.get()) {
          DVLOG(1) << "WebSocket protocol error. "
                   << "inflater_.GetOutput() returns an error.";
          return ERR_WS_PROTOCOL_ERROR;
        }
        inflated->header.CopyFrom(frame->header);
        inflated->header.opcode = current_reading_opcode_;
        inflated->header.final = is_final;
        inflated->header.reserved1 = false;
        inflated->payload = data->data();
        inflated->header.payload_length = data->size();
        DVLOG(3) << "Inflated frame: opcode=" << inflated->header.opcode
                 << " final=" << inflated->header.final
                 << " reserved1=" << inflated->header.reserved1
                 << " payload_length=" << inflated->header.payload_length;
        frames_to_output.push_back(std::move(inflated));
        current_reading_opcode_ = WebSocketFrameHeader::kOpCodeContinuation;
        if (is_final)
          break;
      }
      if (frame->header.final)
        reading_state_ = NOT_READING;
    }
  }
  frames->swap(frames_to_output);
  return frames->empty() ? ERR_IO_PENDING : OK;
}

int WebSocketDeflateStream::InflateAndReadIfNecessary(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  int result = Inflate(frames);
  while (result == ERR_IO_PENDING) {
    DCHECK(frames->empty());

    result = stream_->ReadFrames(
        frames,
        base::BindOnce(&WebSocketDeflateStream::OnReadComplete,
                       base::Unretained(this), base::Unretained(frames)));
    if (result < 0)
      break;
    DCHECK_EQ(OK, result);
    DCHECK(!frames->empty());

    result = Inflate(frames);
  }
  if (result < 0)
    frames->clear();
  return result;
}

}  // namespace net
