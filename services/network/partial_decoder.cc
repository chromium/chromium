// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/partial_decoder.h"

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "net/filter/filter_source_stream.h"

namespace network {

PartialDecoderResult::PartialDecoderResult(
    base::queue<scoped_refptr<net::IOBufferWithSize>> raw_buffers,
    const std::optional<net::Error>& completion_status)
    : completion_status_(completion_status) {
  // Wrap each raw buffer in a `DrainableIOBuffer` to allow for partial
  // consumption in `ConsumeRawData`.
  while (!raw_buffers.empty()) {
    auto buffer = raw_buffers.front();
    CHECK_NE(buffer->size(), 0);
    raw_buffers_.push(
        base::MakeRefCounted<net::DrainableIOBuffer>(buffer, buffer->size()));
    raw_buffers.pop();
  }
}

PartialDecoderResult::PartialDecoderResult(PartialDecoderResult&& other) =
    default;

PartialDecoderResult& PartialDecoderResult::operator=(PartialDecoderResult&&) =
    default;

PartialDecoderResult::~PartialDecoderResult() = default;

bool PartialDecoderResult::HasRawData() const {
  return !raw_buffers_.empty();
}

size_t PartialDecoderResult::ConsumeRawData(base::span<uint8_t> out) {
  base::span<uint8_t> remaining = out;
  // Copy data from the raw buffers into `out` until either all raw data is
  // consumed or `out` is full.
  while (!raw_buffers_.empty() && !remaining.empty()) {
    // Get the next raw buffer.
    auto buf = raw_buffers_.front();
    // Calculate the number of bytes to write, which is the minimum of the
    // remaining space in `out` and the remaining bytes in the current buffer.
    size_t write_size = std::min(
        remaining.size(), base::checked_cast<size_t>(buf->BytesRemaining()));
    auto [destination, rest] = remaining.split_at(write_size);
    // Copy the data from the raw buffer to the output span.
    destination.copy_from_nonoverlapping(buf->first(write_size));
    // Mark the bytes as consumed in the DrainableIOBuffer.
    buf->DidConsume(write_size);
    // If the current raw buffer is fully consumed, remove it.
    if (buf->BytesRemaining() == 0) {
      raw_buffers_.pop();
    }
    // Update the remaining span.
    remaining = rest;
  }
  return out.size() - remaining.size();
}

PartialDecoder::RecordingStream::RecordingStream(
    base::RepeatingCallback<int(net::IOBuffer*, int)> read_callback)
    : SourceStream(net::SourceStreamType::kNone),
      read_callback_(std::move(read_callback)) {}

PartialDecoder::RecordingStream::~RecordingStream() = default;

int PartialDecoder::RecordingStream::Read(
    net::IOBuffer* dest_buffer,
    int buffer_size,
    net::CompletionOnceCallback callback) {
  // Call the underlying read callback to fetch more data.
  int result = read_callback_.Run(dest_buffer, buffer_size);
  if (result == net::ERR_IO_PENDING) {
    // If the read is pending, store the destination buffer and callback for
    // later use in `OnReadCompleted`.
    pending_dest_buffer_ = dest_buffer;
    pending_callback_ = std::move(callback);
    return result;
  }
  HandleReadCompleted(result, dest_buffer);
  return result;
}

std::string PartialDecoder::RecordingStream::Description() const {
  return std::string();
}

bool PartialDecoder::RecordingStream::MayHaveMoreBytes() const {
  return true;
}

void PartialDecoder::RecordingStream::OnReadCompleted(int result) {
  CHECK_NE(result, net::ERR_IO_PENDING);
  CHECK(pending_dest_buffer_);
  HandleReadCompleted(result, pending_dest_buffer_.get());
  // Clear the pending buffer and callback, then invoke the callback to signal
  // completion to the `decoding_stream_`.
  pending_dest_buffer_ = nullptr;
  std::move(pending_callback_).Run(result);
}

base::queue<scoped_refptr<net::IOBufferWithSize>>
PartialDecoder::RecordingStream::TakeRawBuffers() {
  return std::move(raw_buffers_);
}

void PartialDecoder::RecordingStream::HandleReadCompleted(
    int result,
    net::IOBuffer* dest_buffer) {
  CHECK_NE(result, net::ERR_IO_PENDING);
  if (result > 0) {
    // Record the raw data read result.
    auto new_buffer = base::MakeRefCounted<net::IOBufferWithSize>(result);
    new_buffer->span().copy_from(
        dest_buffer->first(base::checked_cast<size_t>(result)));
    raw_buffers_.push(std::move(new_buffer));
  } else {
    // If the read completed, store the completion status.
    completion_status_ = static_cast<net::Error>(result);
  }
}

PartialDecoder::PartialDecoder(
    base::RepeatingCallback<int(net::IOBuffer*, int)> read_raw_data_callback,
    const std::vector<net::SourceStreamType>& types,
    size_t decoded_buffer_size)
    : decoded_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()) {
  decoded_buffer_->SetCapacity(base::checked_cast<int>(decoded_buffer_size));
  // Create a `RecordingStream` to intercept and record the raw data from the
  // underlying read callback.
  auto recording_stream =
      std::make_unique<RecordingStream>(std::move(read_raw_data_callback));
  recording_stream_ = recording_stream.get();
  // Create a decoding stream that uses the `RecordingStream` as its input.
  // This stream will apply the specified decoders (if any) to the recorded
  // raw data.
  decoding_stream_ = net::FilterSourceStream::CreateDecodingSourceStream(
      std::move(recording_stream), types);
}

PartialDecoder::~PartialDecoder() = default;

int PartialDecoder::ReadDecodedDataMore(
    base::OnceCallback<void(int)> callback) {
  CHECK(HasRemainingBuffer());
  // Attempt to read more decoded data from the `decoding_stream_` into the
  // remaining capacity of the `decoded_buffer_`.
  int result = decoding_stream_->Read(
      decoded_buffer_.get(), decoded_buffer_->RemainingCapacity(),
      base::BindOnce(&PartialDecoder::OnReadDecodedDataAsyncComplete,
                     base::Unretained(this)));
  if (result == net::ERR_IO_PENDING) {
    // If the read is pending, store the callback.
    pending_read_decoded_data_more_callback_ = std::move(callback);
  } else if (result > 0) {
    // If data was read synchronously, update the offset of the
    // `decoded_buffer_` to reflect the new data.
    decoded_buffer_->set_offset(decoded_buffer_->offset() + result);
  }
  return result;
}

void PartialDecoder::OnReadDecodedDataAsyncComplete(int result) {
  if (result > 0) {
    // If data was read, update the offset of the `decoded_buffer_`.
    decoded_buffer_->set_offset(decoded_buffer_->offset() + result);
  }
  CHECK(pending_read_decoded_data_more_callback_);
  // Run the stored callback to notify the caller that the read is complete.
  std::move(pending_read_decoded_data_more_callback_).Run(result);
}

// Forwards the completion of a raw data read to the recording stream.
void PartialDecoder::OnReadRawDataCompleted(int bytes_read) {
  CHECK(read_in_progress());
  CHECK(recording_stream_);
  // The `recording_stream_` will handle recording the raw data and invoking
  // the original read callback.
  recording_stream_->OnReadCompleted(bytes_read);
}

bool PartialDecoder::read_in_progress() const {
  return !!pending_read_decoded_data_more_callback_;
}

bool PartialDecoder::HasRemainingBuffer() const {
  return decoded_buffer_->RemainingCapacity() > 0;
}

base::span<const uint8_t> PartialDecoder::decoded_data() const {
  return decoded_buffer_->span_before_offset();
}

PartialDecoderResult PartialDecoder::TakeResult() && {
  return PartialDecoderResult(recording_stream_->TakeRawBuffers(),
                              recording_stream_->completion_status());
}
}  // namespace network
