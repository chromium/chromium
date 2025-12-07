// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PARTIAL_DECODER_H_
#define SERVICES_NETWORK_PARTIAL_DECODER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream.h"
#include "net/filter/source_stream_type.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace network {

// Represents the result of a partial decoding operation. It contains the raw
// (encoded) data read so far and the completion status of the underlying
// read operation.
class COMPONENT_EXPORT(NETWORK_SERVICE) PartialDecoderResult {
 public:
  // `raw_buffers` is a queue of buffers containing the raw, encoded data
  // received from the network.
  // `completion_status` indicates the final status of the network read
  // operation (e.g., success, error, or end-of-file).
  PartialDecoderResult(
      base::queue<scoped_refptr<net::IOBufferWithSize>> raw_buffers,
      const std::optional<net::Error>& completion_status);

  PartialDecoderResult(const PartialDecoderResult&) = delete;
  PartialDecoderResult& operator=(const PartialDecoderResult&) = delete;

  PartialDecoderResult(PartialDecoderResult&& other);
  PartialDecoderResult& operator=(PartialDecoderResult&&);

  ~PartialDecoderResult();

  // Returns true if there is raw (encoded) data available to consume.
  bool HasRawData() const;

  // Copies raw (encoded) data into `out`. Returns the number of bytes copied.
  // This can be called multiple times to consume the raw data in chunks.
  // The `out` span should be a buffer where the raw data will be copied.
  size_t ConsumeRawData(base::span<uint8_t> out);

  // The completion status of the request. This is set only after receiving a
  // 0 or negative value (other than IO_PENDING) from the underlying data
  // source.
  const std::optional<net::Error>& completion_status() const {
    return completion_status_;
  }

 private:
  // A queue of buffers containing the raw, encoded data. These are wrapped in
  // `DrainableIOBuffer` to allow for partial consumption during the
  // `ConsumeRawData` calls.
  base::queue<scoped_refptr<net::DrainableIOBuffer>> raw_buffers_;

  // The final status of the underlying network read operation.
  std::optional<net::Error> completion_status_;
};

// `PartialDecoder` decodes the first `decoded_buffer_size` bytes of a response
// body, making both the decoded data and the raw (encoded) data available. This
// is useful for situations where you need to inspect a small portion of the
// decoded data (e.g., for MIME sniffing) but still want access to the original
// encoded data for the initial part of the stream.
class COMPONENT_EXPORT(NETWORK_SERVICE) PartialDecoder {
 public:
  // Constructs a `PartialDecoder`.
  // `read_raw_data_callback` is called to read raw data from the underlying
  // data source (e.g., net::URLRequest). It takes a buffer and size, and
  // returns the number of bytes read or a net error.
  // `types` is the ordered list of `SourceStreamType` values, indicating which
  // decoding filters (like gzip, brotli) to attempt to apply. An empty list
  // means no decoding will be performed by the `decoding_stream_`.
  // `decoding_buffer_size` is the maximum number of decoded bytes to buffer
  // internally.
  PartialDecoder(
      base::RepeatingCallback<int(net::IOBuffer*, int)> read_raw_data_callback,
      const std::vector<net::SourceStreamType>& types,
      size_t decoding_buffer_size);
  ~PartialDecoder();

  // Attempts to read more decoded data into the internal buffer.
  // When the read operation finishes synchronously, it returns the number of
  // newly read bytes or a net error. Otherwise returns net::ERR_IO_PENDING, and
  // `callback` will be invoked with the result of the read operation (number of
  // bytes read or a net error).
  // This must not be called when `HasRemainingBuffer()` is false.
  // This may call `read_raw_data_callback` multiple times until a decoded chunk
  // becomes available for reading.
  int ReadDecodedDataMore(base::OnceCallback<void(int)> callback);

  // Indicates asynchronous completion of a read started by
  // `read_raw_data_callback`. The creator of `read_raw_data_callback`
  // should arrange for `OnReadRawDataCompleted()` to be called later in
  // the case when `read_raw_data_callback` returns net::ERR_IO_PENDING.
  // `result` is the number of bytes read or a net error code.
  void OnReadRawDataCompleted(int result);

  // Returns true if an asynchronous read operation for decoded data is
  // currently in progress.
  bool read_in_progress() const;

  // Returns true if there is space remaining in the internal buffer to store
  // more decoded data.
  bool HasRemainingBuffer() const;

  // Returns the decoded data that has been buffered so far.
  base::span<const uint8_t> decoded_data() const;

  // Takes ownership of the accumulated raw data and the completion status.
  // This should be called once all necessary decoded data has been read.
  PartialDecoderResult TakeResult() &&;

 private:
  // A custom SourceStream that records the raw data passed to its `Read()`
  // method, while making it available for subsequent decoding by the
  // `decoding_stream_`.
  class RecordingStream : public net::SourceStream {
   public:
    // `read_callback` will be called to fetch more encoded data from
    // the underlying data source.
    explicit RecordingStream(
        base::RepeatingCallback<int(net::IOBuffer*, int)> read_callback);

    RecordingStream(const RecordingStream&) = delete;
    RecordingStream& operator=(const RecordingStream&) = delete;

    ~RecordingStream() override;

    // net::SourceStream implementation:
    // Reads data from the underlying source stream. The read data is also
    // recorded internally.
    int Read(net::IOBuffer* dest_buffer,
             int buffer_size,
             net::CompletionOnceCallback callback) override;
    // Returns a description of the source stream.
    std::string Description() const override;
    // Indicates whether the source stream might have more bytes.
    bool MayHaveMoreBytes() const override;

    // Called when the read operation initiated by `Read()` completes. `result`
    // is the number of read bytes or a net error code.
    void OnReadCompleted(int result);

    // Returns the recorded raw buffers and clears the internal buffer.
    base::queue<scoped_refptr<net::IOBufferWithSize>> TakeRawBuffers();

    // Returns the completion status of the underlying read operations.
    const std::optional<net::Error>& completion_status() const {
      return completion_status_;
    }

   private:
    // Handles the result of `read_callback_`.
    void HandleReadCompleted(int result, net::IOBuffer* dest_buffer);

    // The callback to read more data from the underlying source.
    base::RepeatingCallback<int(net::IOBuffer*, int)> read_callback_;

    // Buffers to store the raw, encoded data received from the underlying
    // source.
    base::queue<scoped_refptr<net::IOBufferWithSize>> raw_buffers_;

    // If `Read()` returns `net::ERR_IO_PENDING`, these store a pointer of the
    // destination buffer and the `callback` passed to the `Read()` method, so
    // they can be used when the read operation completes.
    raw_ptr<net::IOBuffer> pending_dest_buffer_ = nullptr;
    net::CompletionOnceCallback pending_callback_;

    // The completion status of the request. This is set only after receiving a
    // 0 or negative value (other than IO_PENDING) from `read_callback_` or via
    // `OnReadCompleted::OnReadCompleted()`.
    std::optional<net::Error> completion_status_;
  };

  // Called when a read operation on `decoding_stream_` completes
  // asynchronously. `result` is the number of bytes read or a net error.
  void OnReadDecodedDataAsyncComplete(int result);

  // The SourceStream that performs the actual content decoding (e.g., gzip).
  // It takes the `RecordingStream` as its input, so the raw data is
  // recorded before being decoded.
  std::unique_ptr<net::SourceStream> decoding_stream_;

  // The `RecordingStream` instance used to record raw data. This is a raw
  // pointer because the `decoding_stream_` owns the recording stream.
  raw_ptr<RecordingStream> recording_stream_;

  // Buffer to hold the decoded data. The capacity is set during construction.
  scoped_refptr<net::GrowableIOBuffer> decoded_buffer_;

  // Callback to be invoked when an asynchronous read of decoded data
  // completes.
  base::OnceCallback<void(int)> pending_read_decoded_data_more_callback_;
};
}  // namespace network

#endif  // SERVICES_NETWORK_PARTIAL_DECODER_H_
