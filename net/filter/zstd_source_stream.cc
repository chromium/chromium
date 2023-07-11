// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/zstd_source_stream.h"

#include <utility>

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace net {

namespace {

const char kZstd[] = "ZSTD";

struct FreeContextDeleter {
  inline void operator()(ZSTD_DCtx* ptr) const { ZSTD_freeDCtx(ptr); }
};

// ZstdSourceStream applies Zstd content decoding to a data stream.
// Zstd format speciication: https://datatracker.ietf.org/doc/html/rfc8878
class ZstdSourceStream : public FilterSourceStream {
 public:
  explicit ZstdSourceStream(std::unique_ptr<SourceStream> upstream)
      : FilterSourceStream(SourceStream::TYPE_ZSTD, std::move(upstream)) {
    dctx_.reset(ZSTD_createDCtx());
  }

  ZstdSourceStream(const ZstdSourceStream&) = delete;
  ZstdSourceStream& operator=(const ZstdSourceStream&) = delete;

  ~ZstdSourceStream() override = default;

 private:
  enum class DecodingStatus : int {
    DECODING_IN_PROGRESS = 0,
    DECODING_DONE,
    DECODING_ERROR,

    DECODING_STATUS_COUNT
    // DECODING_STATUS_COUNT must always be the last element in this enum.
  };

  // SourceStream implementation
  std::string GetTypeAsString() const override { return kZstd; }

  base::expected<size_t, Error> FilterData(IOBuffer* output_buffer,
                                           size_t output_buffer_size,
                                           IOBuffer* input_buffer,
                                           size_t input_buffer_size,
                                           size_t* consumed_bytes,
                                           bool upstream_end_reached) override {
    if (decoding_status_ == DecodingStatus::DECODING_DONE) {
      *consumed_bytes = input_buffer_size;
      return 0;
    }

    if (decoding_status_ != DecodingStatus::DECODING_IN_PROGRESS) {
      return base::unexpected(ERR_CONTENT_DECODING_FAILED);
    }

    CHECK(dctx_);
    ZSTD_inBuffer input = {input_buffer->data(), input_buffer_size, 0};
    ZSTD_outBuffer output = {output_buffer->data(), output_buffer_size, 0};

    const size_t result = ZSTD_decompressStream(dctx_.get(), &output, &input);

    if (result > 0u) {
      if (upstream_end_reached) {
        decoding_status_ = DecodingStatus::DECODING_ERROR;
      }
      // There is some input remaining and caller should provide remaining input
      // on next call OR there is potentially unflushed data present in the
      // internal buffers.
      *consumed_bytes = input.pos;
      return output.pos;
    } else if (result == 0u) {
      CHECK_LT(output.pos, output.size);
      // Decoder finished and flushed all remaining buffers
      decoding_status_ = DecodingStatus::DECODING_DONE;
      *consumed_bytes = input.pos;
      return output.pos;
    } else {
      DCHECK(ZSTD_isError(result));
      decoding_status_ = DecodingStatus::DECODING_ERROR;
      return base::unexpected(ERR_CONTENT_DECODING_FAILED);
    }
  }

  std::unique_ptr<ZSTD_DCtx, FreeContextDeleter> dctx_;

  DecodingStatus decoding_status_ = DecodingStatus::DECODING_IN_PROGRESS;
};

}  // namespace

std::unique_ptr<FilterSourceStream> CreateZstdSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return std::make_unique<ZstdSourceStream>(std::move(previous));
}

}  // namespace net
