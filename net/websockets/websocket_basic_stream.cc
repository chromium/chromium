// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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

// The number of bytes to attempt to read at a time. It's used only for high
// throughput connections.
// TODO(ricea): See if there is a better number or algorithm to fulfill our
// requirements:
//  1. We would like to use minimal memory on low-bandwidth or idle connections
//  2. We would like to read as close to line speed as possible on
//     high-bandwidth connections
//  3. We can't afford to cause jank on the IO thread by copying large buffers
//     around
//  4. We would like to hit any sweet-spots that might exist in terms of network
//     packet sizes / encryption block sizes / IPC alignment issues, etc.
#if BUILDFLAG(IS_ANDROID)
constexpr size_t kLargeReadBufferSize = 32 * 1024;
#else
// |2^n - delta| is better than 2^n on Linux. See crrev.com/c/1792208.
constexpr size_t kLargeReadBufferSize = 131000;
#endif

// The number of bytes to attempt to read at a time. It's set as an initial read
// buffer size and used for low throughput connections.
constexpr size_t kSmallReadBufferSize = 1000;

// The threshold to decide whether to switch the read buffer size.
constexpr double kThresholdInBytesPerSecond = 1200 * 1000;

// Returns the total serialized size of |frames|. This function assumes that
// |frames| will be serialized with mask field. This function forces the
// masked bit of the frames on.
int CalculateSerializedSizeAndTurnOnMaskBit(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  constexpr uint64_t kMaximumTotalSize = std::numeric_limits<int>::max();

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

base::Value::Dict NetLogBufferSizeParam(int buffer_size) {
  base::Value::Dict dict;
  dict.Set("read_buffer_size_in_bytes", buffer_size);
  return dict;
}

base::Value::Dict NetLogFrameHeaderParam(const WebSocketFrameHeader* header) {
  base::Value::Dict dict;
  dict.Set("final", header->final);
  dict.Set("reserved1", header->reserved1);
  dict.Set("reserved2", header->reserved2);
  dict.Set("reserved3", header->reserved3);
  dict.Set("opcode", header->opcode);
  dict.Set("masked", header->masked);
  dict.Set("payload_length", static_cast<double>(header->payload_length));
  return dict;
}

}  // namespace

WebSocketBasicStream::BufferSizeManager::BufferSizeManager() = default;

WebSocketBasicStream::BufferSizeManager::~BufferSizeManager() = default;

void WebSocketBasicStream::BufferSizeManager::OnRead(base::TimeTicks now) {
  read_start_timestamps_.push(now);
}

void WebSocketBasicStream::BufferSizeManager::OnReadComplete(
    base::TimeTicks now,
    int size) {
  DCHECK_GT(size, 0);
  // This cannot overflow because the result is at most
  // kLargeReadBufferSize*rolling_average_window_.
  rolling_byte_total_ += size;
  recent_read_sizes_.push(size);
  DCHECK_LE(read_start_timestamps_.size(), rolling_average_window_);
  if (read_start_timestamps_.size() == rolling_average_window_) {
    DCHECK_EQ(read_start_timestamps_.size(), recent_read_sizes_.size());
    base::TimeDelta duration = now - read_start_timestamps_.front();
    base::TimeDelta threshold_duration =
        base::Seconds(rolling_byte_total_ / kThresholdInBytesPerSecond);
    read_start_timestamps_.pop();
    rolling_byte_total_ -= recent_read_sizes_.front();
    recent_read_sizes_.pop();
    if (threshold_duration < duration) {
      buffer_size_ = BufferSize::kSmall;
    } else {
      buffer_size_ = BufferSize::kLarge;
    }
  }
}

WebSocketBasicStream::WebSocketBasicStream(
    std::unique_ptr<Adapter> connection,
    const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
    const std::string& sub_protocol,
    const std::string& extensions,
    const NetLogWithSource& net_log)
    : read_buffer_(
          base::MakeRefCounted<IOBufferWithSize>(kSmallReadBufferSize)),
      target_read_buffer_size_(read_buffer_->size()),
      connection_(std::move(connection)),
      http_read_buffer_(http_read_buffer),
      sub_protocol_(sub_protocol),
      extensions_(extensions),
      net_log_(net_log),
      generate_websocket_masking_key_(&GenerateWebSocketMaskingKey) {
  // http_read_buffer_ should not be set if it contains no data.
  if (http_read_buffer_.get() && http_read_buffer_->offset() == 0)
    http_read_buffer_ = nullptr;
  DCHECK(connection_->is_initialized());
}

WebSocketBasicStream::~WebSocketBasicStream() { Close(); }

int WebSocketBasicStream::ReadFrames(
    std::vector<std::unique_ptr<WebSocketFrame>>* frames,
    CompletionOnceCallback callback) {
  read_callback_ = std::move(callback);
  control_frame_payloads_.clear();
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

  base::span<uint8_t> dest = combined_buffer->span();
  for (const auto& frame : *frames) {
    net_log_.AddEvent(net::NetLogEventType::WEBSOCKET_SENT_FRAME_HEADER,
                      [&] { return NetLogFrameHeaderParam(&frame->header); });
    WebSocketMaskingKey mask = generate_websocket_masking_key_();
    int result = WriteWebSocketFrameHeader(frame->header, &mask, dest);
    DCHECK_NE(ERR_INVALID_ARGUMENT, result)
        << "WriteWebSocketFrameHeader() says that " << dest.size()
        << " is not enough to write the header in. This should not happen.";
    CHECK_GE(result, 0) << "Potentially security-critical check failed";
    dest = dest.subspan(result);

    CHECK_LE(frame->header.payload_length,
             base::checked_cast<uint64_t>(dest.size()));
    const size_t frame_size = frame->header.payload_length;
    if (frame_size > 0) {
      dest.copy_prefix_from(frame->payload);
      MaskWebSocketFramePayload(mask, 0, dest.first(frame_size));
      dest = dest.subspan(frame_size);
    }
  }
  DCHECK(dest.empty()) << "Buffer size calculation was wrong; " << dest.size()
                       << " bytes left over.";
  auto drainable_buffer = base::MakeRefCounted<DrainableIOBuffer>(
      std::move(combined_buffer), total_size);
  return WriteEverything(drainable_buffer);
}

void WebSocketBasicStream::Close() {
  connection_->Disconnect();
}

std::string WebSocketBasicStream::GetSubProtocol() const {
  return sub_protocol_;
}

std::string WebSocketBasicStream::GetExtensions() const { return extensions_; }

const NetLogWithSource& WebSocketBasicStream::GetNetLogWithSource() const {
  return net_log_;
}

/*static*/
std::unique_ptr<WebSocketBasicStream>
WebSocketBasicStream::CreateWebSocketBasicStreamForTesting(
    std::unique_ptr<ClientSocketHandle> connection,
    const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
    const std::string& sub_protocol,
    const std::string& extensions,
    const NetLogWithSource& net_log,
    WebSocketMaskingKeyGeneratorFunction key_generator_function) {
  auto stream = std::make_unique<WebSocketBasicStream>(
      std::make_unique<WebSocketClientSocketHandleAdapter>(
          std::move(connection)),
      http_read_buffer, sub_protocol, extensions, net_log);
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
    if (!parser_.Decode(http_read_buffer_->span_before_offset(),
                        &frame_chunks)) {
      return WebSocketErrorToNetError(parser_.websocket_error());
    }
    if (!frame_chunks.empty()) {
      int result = ConvertChunksToFrames(&frame_chunks, frames);
      if (result != ERR_IO_PENDING)
        return result;
    }
  }

  // Run until socket stops giving us data or we get some frames.
  while (true) {
    if (buffer_size_manager_.buffer_size() != buffer_size_) {
      read_buffer_ = base::MakeRefCounted<IOBufferWithSize>(
          buffer_size_manager_.buffer_size() == BufferSize::kSmall
              ? kSmallReadBufferSize
              : kLargeReadBufferSize);
      buffer_size_ = buffer_size_manager_.buffer_size();
      net_log_.AddEvent(
          net::NetLogEventType::WEBSOCKET_READ_BUFFER_SIZE_CHANGED,
          [&] { return NetLogBufferSizeParam(read_buffer_->size()); });
    }
    buffer_size_manager_.OnRead(base::TimeTicks::Now());

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

  buffer_size_manager_.OnReadComplete(base::TimeTicks::Now(), result);

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frame_chunks;
  if (!parser_.Decode(
          read_buffer_->span().first(base::checked_cast<size_t>(result)),
          &frame_chunks)) {
    return WebSocketErrorToNetError(parser_.websocket_error());
  }
  if (frame_chunks.empty())
    return ERR_IO_PENDING;
  return ConvertChunksToFrames(&frame_chunks, frames);
}

int WebSocketBasicStream::ConvertChunksToFrames(
    std::vector<std::unique_ptr<WebSocketFrameChunk>>* frame_chunks,
    std::vector<std::unique_ptr<WebSocketFrame>>* frames) {
  for (auto& chunk : *frame_chunks) {
    DCHECK(chunk == frame_chunks->back() || chunk->final_chunk)
        << "Only last chunk can have |final_chunk| set to be false.";

    if (chunk->header) {
      net_log_.AddEvent(net::NetLogEventType::WEBSOCKET_RECV_FRAME_HEADER, [&] {
        return NetLogFrameHeaderParam(chunk->header.get());
      });
    }

    auto frame_result = chunk_assembler_.HandleChunk(std::move(chunk));

    if (!frame_result.has_value()) {
      return frame_result.error();
    }

    auto frame = std::move(frame_result.value());
    bool is_control_opcode =
        WebSocketFrameHeader::IsKnownControlOpCode(frame->header.opcode) ||
        WebSocketFrameHeader::IsReservedControlOpCode(frame->header.opcode);
    if (is_control_opcode) {
      const size_t length =
          base::checked_cast<size_t>(frame->header.payload_length);
      if (length > 0) {
        auto copied_payload =
            base::HeapArray<uint8_t>::CopiedFrom(frame->payload);
        frame->payload = copied_payload.as_span();
        control_frame_payloads_.emplace_back(std::move(copied_payload));
      }
    }

    frames->emplace_back(std::move(frame));
  }

  frame_chunks->clear();

  return frames->empty() ? ERR_IO_PENDING : OK;
}

}  // namespace net
