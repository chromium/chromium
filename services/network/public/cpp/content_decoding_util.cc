// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_decoding_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/source_stream.h"
#include "services/network/public/cpp/data_buffer_factory.h"

namespace network {

namespace {

// An in-memory, synchronous net::SourceStream that reads from a base::span.
class SpanSourceStream : public net::SourceStream {
 public:
  explicit SpanSourceStream(base::span<const uint8_t> data)
      : net::SourceStream(net::SourceStreamType::kNone), data_(data) {}
  ~SpanSourceStream() override = default;

  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override {
    CHECK(dest_buffer);
    CHECK_GE(buffer_size, 0);
    if (data_.empty()) {
      return 0;
    }
    size_t bytes_to_copy =
        std::min(data_.size(), static_cast<size_t>(buffer_size));
    dest_buffer->span()
        .first(bytes_to_copy)
        .copy_from(data_.first(bytes_to_copy));
    data_ = data_.subspan(bytes_to_copy);
    return static_cast<int>(bytes_to_copy);
  }

  std::string Description() const override { return "SpanSourceStream"; }
  bool MayHaveMoreBytes() const override { return !data_.empty(); }

 private:
  base::raw_span<const uint8_t> data_;
};

}  // namespace

// static
std::unique_ptr<network::DataBufferList> ContentDecodingUtil::Decode(
    base::span<const uint8_t> upstream_data,
    const std::vector<net::SourceStreamType>& types,
    network::DataBufferFactory& data_buffer_factory,
    uint32_t buffer_size) {
  TRACE_EVENT("net", "ContentDecodingUtil::Decode");
  CHECK_GT(buffer_size, 0u);
  if (!base::IsValueInRangeForNumericType<int>(upstream_data.size())) {
    return nullptr;
  }
  const int read_buffer_size = base::checked_cast<int>(buffer_size);

  // Wrap the input in-memory data in a synchronous SourceStream.
  auto source_stream = std::make_unique<SpanSourceStream>(upstream_data);

  // Create the chained decoding stream for the specified content encodings.
  std::unique_ptr<net::SourceStream> decoding_stream =
      net::FilterSourceStream::CreateDecodingSourceStream(
          std::move(source_stream), types);

  if (!decoding_stream) {
    return nullptr;
  }

  auto buffers = data_buffer_factory.CreateDataBufferList();

  // Read and decode data in chunks until EOF.
  while (true) {
    auto buffer = data_buffer_factory.AllocateDataBuffer(buffer_size);
    auto io_buffer = base::MakeRefCounted<net::WrappedIOBuffer>(buffer->data());

    // SpanSourceStream completes reads synchronously, so the callback is
    // never invoked.
    int result =
        decoding_stream->Read(io_buffer.get(), read_buffer_size,
                              base::BindOnce([](int) { NOTREACHED(); }));
    if (result < 0) {
      if (result == net::ERR_IO_PENDING) {
        // Since SpanSourceStream is synchronous, FilterSourceStream should also
        // return synchronously.
        NOTREACHED();
      }
      // Reset `decoding_stream` before `buffer` is destroyed to avoid a
      // dangling pointer in FilterSourceStream's internal IOBuffer reference to
      // `buffer->data()`.
      decoding_stream.reset();
      return nullptr;
    }

    if (result == 0) {
      // Reached EOF.
      // Reset `decoding_stream` before `buffer` is destroyed so that the
      // FilterSourceStream releases its reference to `io_buffer` before
      // `buffer` goes out of scope and frees its memory.
      decoding_stream.reset();
      break;
    }

    // Shrink buffer to actual decoded bytes read if necessary.
    if (buffer->data().size() > static_cast<size_t>(result)) {
      buffer->Shrink(result);
    }
    buffers->Append(std::move(buffer));
  }

  return buffers;
}

}  // namespace network
