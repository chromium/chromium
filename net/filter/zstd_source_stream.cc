// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/zstd_source_stream.h"

#include "net/base/io_buffer.h"

namespace net {

namespace {

const char kZstd[] = "ZSTD";

// ZstdSourceStream applies Zstd content decoding to a data stream.
// Zstd format speciication: https://datatracker.ietf.org/doc/html/rfc8878
class ZstdSourceStream : public FilterSourceStream {
 public:
  explicit ZstdSourceStream(std::unique_ptr<SourceStream> upstream)
      : FilterSourceStream(SourceStream::TYPE_ZSTD, std::move(upstream)) {}

  ZstdSourceStream(const ZstdSourceStream&) = delete;
  ZstdSourceStream& operator=(const ZstdSourceStream&) = delete;

  ~ZstdSourceStream() override = default;

 private:
  // SourceStream implementation
  std::string GetTypeAsString() const override { return kZstd; }

  base::expected<size_t, Error> FilterData(IOBuffer* output_bufer,
                                           size_t output_buffer_size,
                                           IOBuffer* input_buffer,
                                           size_t input_buffer_size,
                                           size_t* consumed_bytes,
                                           bool upstream_end_reached) override {
    return 0;
  }
};

}  // namespace

std::unique_ptr<FilterSourceStream> CreateZstdSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return std::make_unique<ZstdSourceStream>(std::move(previous));
}

}  // namespace net
