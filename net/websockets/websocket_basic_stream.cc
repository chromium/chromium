// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"
#include "net/websockets/websocket_basic_stream_adapters.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"

namespace net {

namespace {

// Please refer to the comment in class header if the usage changes.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("websocket_basic_stream", R"(
      semantics {
        sender: "WebSocket Basic Stream"
        description:
          "Implementation of WebSocket API from web content (a page the user "
          "visits)."
        trigger: "Website calls the WebSocket API."
        data:
          "Any data provided by web content, masked and framed in accordance "
          "with RFC6455."
        destination: OTHER
        destination_other:
          "The address that the website has chosen to communicate to."
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled."
        policy_exception_justification:
          "Not implemented. WebSocket is a core web platform API."
      }
      comments:
        "The browser will never add cookies to a WebSocket message. But the "
        "handshake that was performed when the WebSocket connection was "
        "established may have contained cookies."
      )");

// This uses type uint64_t to match the definition of
// WebSocketFrameHeader::payload_length in websocket_frame.h.
const uint64_t kMaxControlFramePayload = 125;

// The number of bytes to attempt to read at a time.
// TODO(ricea): See if there is a better number or algorithm to fulfill our
// requirements:
//  1. We would like to use minimal memory on low-bandwidth or idle connections
//  2. We would like to read as close to line speed as possible on
//     high-bandwidth connections
//  3. We can't afford to cause jank on the IO thread by copying large buffers
//     around
//  4. We would like to hit any sweet-spots that might exist in terms of network
//     packet sizes / encryption block sizes / IPC alignment issues, etc.
#if defined(OS_ANDROID)
const int kReadBufferSize = 32 * 1024;
#else
// |2^n - delta| is better than 2^n on Linux. See crrev.com/c/1792208.
const int kReadBufferSize = 131000;
#endif

// Returns the total serialized size of |frames|. This function assumes that
// |frames| will be serialized with mask field. This function forces the
// masked bit of the frames on.
int CalculateSerializedSizeAndTurnOnMaskBit(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  const uint64_t kMaximumTotalSize = std::numeric_limits<int>::max();

  uint64_t total_size = 0;
  for (const auto& frame : *frames) {
    // Force the masked bit on.
    frame->header.masked = true;
    // We enforce flow control so the renderer should never be able to force us
    // to cache anywhere near 2GB of frames.
    uint64_t frame_size = frame->header.payload_length +
                          GetWebSocketFrameHeaderSize(frame->header);
    CHECK_LE(frame_size, kMaximumTotalSize - total_size)
        << "Aborting to prevent overflow";
    total_size += frame_size;
  }
  return static_cast<int>(total_size);
}

}  // namespace

// Overrides default read buffer size for WebSocket. This flag will be used to
// investigate the performance issue of crbug.com/865001 and be deleted later
// on.
const char kWebSocketReadBufferSize[] = "websocket-read-buffer-size";

WebSocketBasicStream::WebSocketBasicStream(
    std::unique_ptr<Adapter> connection,
    const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
    const std::string& sub_protocol,
    const std::string& extensions)
    : connection_(std::move(connection)),
      http_read_buffer_(http_read_buffer),
      sub_protocol_(sub_protocol),
      extensions_(extensions),
      generate_websocket_masking_key_(&GenerateWebSocketMaskingKey) {
  // http_read_buffer_ should not be set if it contains no data.
  if (http_read_buffer_.get() && http_read_buffer_->offset() == 0)
    http_read_buffer_ = nullptr;
  DCHECK(connection_->is_initialized());
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);
  int websocket_buffer_size = kReadBufferSize;
  if (command_line->HasSwitch(kWebSocketReadBufferSize)) {
    std::string size_string =
        command_line->GetSwitchValueASCII(kWebSocketReadBufferSize);
    if (!base::StringToInt(size_string, &websocket_buffer_size) ||
        websocket_buffer_size <= 0) {
      websocket_buffer_size = kReadBufferSize;
    }
  }
  DVLOG(1) << "WebSocketReadBufferSize is " << websocket_buffer_size;
  read_buffer_ =
      (base::MakeRefCounted<IOBufferWithSize>(websocket_buffer_size));
}

WebSocketBasicStream::~WebSocketBasicStream() { Close(); }

int WebSocketBasicStream::ReadFrames(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    CompletionOnceCallback callback) {
  read_callback_ = std::move(callback);
  complete_control_frame_body_.clear();
  if (http_read_buffer_ && is_http_read_buffer_decoded_) {
    http_read_buffer_.reset();
  }
  return ReadEverything(frames);
}

int WebSocketBasicStream::WriteFrames(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    CompletionOnceCallback callback) {
  // This function always concatenates all frames into a single buffer.
  // TODO(ricea): Investigate whether it would be better in some cases to
  // perform multiple writes with smaller buffers.

  write_callback_ = std::move(callback);

  // First calculate the size of the buffer we need to allocate.
  int total_size = CalculateSerializedSizeAndTurnOnMaskBit(frames);
  auto combined_buffer = base::MakeRefCounted<IOBufferWithSize>(total_size);

  char* dest = combined_buffer->data();
  int remaining_size = total_size;
  for (const auto& frame : *frames) {
    WebSocketMaskingKey mask = generate_websocket_masking_key_();
    int result =
        WriteWebSocketFrameHeader(frame->header, &mask, dest, remaining_size);
    DCHECK_NE(ERR_INVALID_ARGUMENT, result)
        << "WriteWebSocketFrameHeader() says that " << remaining_size
        << " is not enough to write the header in. This should not happen.";
    CHECK_GE(result, 0) << "Potentially security-critical check failed";
    dest += result;
    remaining_size -= result;

    CHECK_LE(frame->header.payload_length,
             static_cast<uint64_t>(remaining_size));
    const int frame_size = static_cast<int>(frame->header.payload_length);
    if (frame_size > 0) {
      const char* const frame_data = frame->payload;
      std::copy(frame_data, frame_data + frame_size, dest);
      MaskWebSocketFramePayload(mask, 0, dest, frame_size);
      dest += frame_size;
      remaining_size -= frame_size;
    }
  }
  DCHECK_EQ(0, remaining_size) << "Buffer size calculation was wrong; "
                               << remaining_size << " bytes left over.";
  auto drainable_buffer = base::MakeRefCounted<DrainableIOBuffer>(
      combined_buffer.get(), total_size);
  return WriteEverything(drainable_buffer);
}

void WebSocketBasicStream::Close() {
  connection_->Disconnect();
}

std::string WebSocketBasicStream::GetSubProtocol() const {
  return sub_protocol_;
}

std::string WebSocketBasicStream::GetExtensions() const { return extensions_; }

/*static*/
std::unique_ptr<WebSocketBasicStream>
WebSocketBasicStream::CreateWebSocketBasicStreamForTesting(
    std::unique_ptr<ClientSocketHandle> connection,
    const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
    const std::string& sub_protocol,
    const std::string& extensions,
    WebSocketMaskingKeyGeneratorFunction key_generator_function) {
  auto stream = std::make_unique<WebSocketBasicStream>(
      std::make_unique<WebSocketClientSocketHandleAdapter>(
          std::move(connection)),
      http_read_buffer, sub_protocol, extensions);
  stream->generate_websocket_masking_key_ = key_generator_function;
  return stream;
}

int WebSocketBasicStream::ReadEverything(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  DCHECK(frames->empty());

  // If there is data left over after parsing the HTTP headers, attempt to parse
  // it as WebSocket frames.
  if (http_read_buffer_.get() && !is_http_read_buffer_decoded_) {
    DCHECK_GE(http_read_buffer_->offset(), 0);
    is_http_read_buffer_decoded_ = true;
    std::vector<std::unique_ptr<WebSocketFrameChunk>> frame_chunks;
    if (!parser_.Decode(http_read_buffer_->StartOfBuffer(),
                        http_read_buffer_->offset(), &frame_chunks))
      return WebSocketErrorToNetError(parser_.websocket_error());
    if (!frame_chunks.empty()) {
      int result = ConvertChunksToFrames(&frame_chunks, frames);
      if (result != ERR_IO_PENDING)
        return result;
    }
  }

  // Run until socket stops giving us data or we get some frames.
  while (true) {
    // base::Unretained(this) here is safe because net::Socket guarantees not to
    // call any callbacks after Disconnect(), which we call from the destructor.
    // The caller of ReadEverything() is required to keep |frames| valid.
    int result = connection_->Read(
        read_buffer_.get(), read_buffer_->size(),
        base::BindOnce(&WebSocketBasicStream::OnReadComplete,
                       base::Unretained(this), base::Unretained(frames)));
    if (result == ERR_IO_PENDING)
      return result;
    result = HandleReadResult(result, frames);
    if (result != ERR_IO_PENDING)
      return result;
    DCHECK(frames->empty());
  }
}

void WebSocketBasicStream::OnReadComplete(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    int result) {
  result = HandleReadResult(result, frames);
  if (result == ERR_IO_PENDING)
    result = ReadEverything(frames);
  if (result != ERR_IO_PENDING)
    std::move(read_callback_).Run(result);
}

int WebSocketBasicStream::WriteEverything(
    const scoped_refptr<DrainableIOBuffer>& buffer) {
  while (buffer->BytesRemaining() > 0) {
    // The use of base::Unretained() here is safe because on destruction we
    // disconnect the socket, preventing any further callbacks.
    int result = connection_->Write(
        buffer.get(), buffer->BytesRemaining(),
        base::BindOnce(&WebSocketBasicStream::OnWriteComplete,
                       base::Unretained(this), buffer),
        kTrafficAnnotation);
    if (result > 0) {
      UMA_HISTOGRAM_COUNTS_100000("Net.WebSocket.DataUse.Upstream", result);
      buffer->DidConsume(result);
    } else {
      return result;
    }
  }
  return OK;
}

void WebSocketBasicStream::OnWriteComplete(
    const scoped_refptr<DrainableIOBuffer>& buffer,
    int result) {
  if (result < 0) {
    DCHECK_NE(ERR_IO_PENDING, result);
    std::move(write_callback_).Run(result);
    return;
  }

  DCHECK_NE(0, result);
  UMA_HISTOGRAM_COUNTS_100000("Net.WebSocket.DataUse.Upstream", result);

  buffer->DidConsume(result);
  result = WriteEverything(buffer);
  if (result != ERR_IO_PENDING)
    std::move(write_callback_).Run(result);
}

int WebSocketBasicStream::HandleReadResult(
    int result,
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(frames->empty());
  if (result < 0)
    return result;
  if (result == 0)
    return ERR_CONNECTION_CLOSED;

  UMA_HISTOGRAM_COUNTS_100000("Net.WebSocket.DataUse.Downstream", result);

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frame_chunks;
  if (!parser_.Decode(read_buffer_->data(), result, &frame_chunks))
    return WebSocketErrorToNetError(parser_.websocket_error());
  if (frame_chunks.empty())
    return ERR_IO_PENDING;
  return ConvertChunksToFrames(&frame_chunks, frames);
}

int WebSocketBasicStream::ConvertChunksToFrames(
    std::vector<std::unique_ptr<WebSocketFrameChunk>>* frame_chunks,
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  for (size_t i = 0; i < frame_chunks->size(); ++i) {
    auto& chunk = (*frame_chunks)[i];
    DCHECK(chunk == frame_chunks->back() || chunk->final_chunk)
        << "Only last chunk can have |final_chunk| set to be false.";
    std::unique_ptr<WebSocketFrame> frame;
    int result = ConvertChunkToFrame(std::move(chunk), &frame);
    if (result != OK)
      return result;
    if (frame)
      frames->push_back(std::move(frame));
  }
  frame_chunks->clear();
  if (frames->empty())
    return ERR_IO_PENDING;
  return OK;
}

int WebSocketBasicStream::ConvertChunkToFrame(
    std::unique_ptr<WebSocketFrameChunk> chunk,
    std::unique_ptr<WebSocketFrame>* frame) {
  DCHECK(frame->get() == nullptr);
  bool is_first_chunk = false;
  if (chunk->header) {
    DCHECK(current_frame_header_ == nullptr)
        << "Received the header for a new frame without notification that "
        << "the previous frame was complete (bug in WebSocketFrameParser?)";
    is_first_chunk = true;
    current_frame_header_.swap(chunk->header);
  }
  DCHECK(current_frame_header_) << "Unexpected header-less chunk received "
                                << "(final_chunk = " << chunk->final_chunk
                                << ", payload size = " << chunk->payload.size()
                                << ") (bug in WebSocketFrameParser?)";
  const bool is_final_chunk = chunk->final_chunk;
  const WebSocketFrameHeader::OpCode opcode = current_frame_header_->opcode;
  if (WebSocketFrameHeader::IsKnownControlOpCode(opcode)) {
    bool protocol_error = false;
    if (!current_frame_header_->final) {
      DVLOG(1) << "WebSocket protocol error. Control frame, opcode=" << opcode
               << " received with FIN bit unset.";
      protocol_error = true;
    }
    if (current_frame_header_->payload_length > kMaxControlFramePayload) {
      DVLOG(1) << "WebSocket protocol error. Control frame, opcode=" << opcode
               << ", payload_length=" << current_frame_header_->payload_length
               << " exceeds maximum payload length for a control message.";
      protocol_error = true;
    }
    if (protocol_error) {
      current_frame_header_.reset();
      return ERR_WS_PROTOCOL_ERROR;
    }

    if (!is_final_chunk) {
      DVLOG(2) << "Encountered a split control frame, opcode " << opcode;
      AddToIncompleteControlFrameBody(chunk->payload);
      return OK;
    }

    if (!incomplete_control_frame_body_.empty()) {
      DVLOG(2) << "Rejoining a split control frame, opcode " << opcode;
      AddToIncompleteControlFrameBody(chunk->payload);
      DCHECK(is_final_chunk);
      DCHECK(complete_control_frame_body_.empty());
      complete_control_frame_body_ = std::move(incomplete_control_frame_body_);
      *frame = CreateFrame(is_final_chunk, complete_control_frame_body_);
      return OK;
    }
  }

  // Apply basic sanity checks to the |payload_length| field from the frame
  // header. A check for exact equality can only be used when the whole frame
  // arrives in one chunk.
  DCHECK_GE(current_frame_header_->payload_length,
            base::checked_cast<uint64_t>(chunk->payload.size()));
  DCHECK(!is_first_chunk || !is_final_chunk ||
         current_frame_header_->payload_length ==
             base::checked_cast<uint64_t>(chunk->payload.size()));

  // Convert the chunk to a complete frame.
  *frame = CreateFrame(is_final_chunk, chunk->payload);
  return OK;
}

std::unique_ptr<WebSocketFrame> WebSocketBasicStream::CreateFrame(
    bool is_final_chunk,
    base::span<const char> data) {
  std::unique_ptr<WebSocketFrame> result_frame;
  const bool is_final_chunk_in_message =
      is_final_chunk && current_frame_header_->final;
  const WebSocketFrameHeader::OpCode opcode = current_frame_header_->opcode;
  // Empty frames convey no useful information unless they are the first frame
  // (containing the type and flags) or have the "final" bit set.
  if (is_final_chunk_in_message || data.size() > 0 ||
      current_frame_header_->opcode !=
          WebSocketFrameHeader::kOpCodeContinuation) {
    result_frame = std::make_unique<WebSocketFrame>(opcode);
    result_frame->header.CopyFrom(*current_frame_header_);
    result_frame->header.final = is_final_chunk_in_message;
    result_frame->header.payload_length = data.size();
    result_frame->payload = data.data();
    // Ensure that opcodes Text and Binary are only used for the first frame in
    // the message. Also clear the reserved bits.
    // TODO(ricea): If a future extension requires the reserved bits to be
    // retained on continuation frames, make this behaviour conditional on a
    // flag set at construction time.
    if (!is_final_chunk && WebSocketFrameHeader::IsKnownDataOpCode(opcode)) {
      current_frame_header_->opcode = WebSocketFrameHeader::kOpCodeContinuation;
      current_frame_header_->reserved1 = false;
      current_frame_header_->reserved2 = false;
      current_frame_header_->reserved3 = false;
    }
  }
  // Make sure that a frame header is not applied to any chunks that do not
  // belong to it.
  if (is_final_chunk)
    current_frame_header_.reset();
  return result_frame;
}

void WebSocketBasicStream::AddToIncompleteControlFrameBody(
    base::span<const char> data) {
  if (data.empty()) {
    return;
  }
  incomplete_control_frame_body_.insert(incomplete_control_frame_body_.end(),
                                        data.begin(), data.end());
  // This method checks for oversize control frames above, so as long as
  // the frame parser is working correctly, this won't overflow. If a bug
  // does cause it to overflow, it will CHECK() in
  // AddToIncompleteControlFrameBody() without writing outside the buffer.
  CHECK_LE(incomplete_control_frame_body_.size(), kMaxControlFramePayload)
      << "Control frame body larger than frame header indicates; frame parser "
         "bug?";
}

}  // namespace net
