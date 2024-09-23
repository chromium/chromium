// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/zstd_source_stream.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#define ZSTD_STATIC_LINKING_ONLY

#include "base/bits.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "third_party/zstd/src/lib/zstd_errors.h"

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
  explicit ZstdSourceStream(std::unique_ptr<SourceStream> upstream,
                            scoped_refptr<IOBuffer> dictionary = nullptr,
                            size_t dictionary_size = 0u)
      : FilterSourceStream(SourceStream::TYPE_ZSTD, std::move(upstream)),
        dictionary_(std::move(dictionary)),
        dictionary_size_(dictionary_size) {
    ZSTD_customMem custom_mem = {&customMalloc, &customFree, this};
    dctx_.reset(ZSTD_createDCtx_advanced(custom_mem));
    CHECK(dctx_);

    // Following RFC 8878 recommendation (see section 3.1.1.1.2 Window
    // Descriptor) of using a maximum 8MB memory buffer to decompress frames
    // to '... protect decoders from unreasonable memory requirements'.
    int window_log_max = 23;
    if (dictionary_) {
      // For shared dictionary case, allow using larger window size:
      //   clamp(dictionary size * 1.25, 8MB, 128MB)
      // See https://github.com/httpwg/http-extensions/issues/2754 for more
      // details. To avoid floating point calculations, using `* 5 / 4` for
      // `* 1.25` specified by the standard.
      // Note: `base::checked_cast<uint32_t>` is safe because we have the size
      // limit per shared dictionary and the total dictionary size limit.
      window_log_max = std::clamp(
          base::bits::Log2Ceiling(
              base::checked_cast<uint32_t>(dictionary_size_ * 5 / 4)),
          23,   // 8MB
          27);  // 128MB
    }
    ZSTD_DCtx_setParameter(dctx_.get(), ZSTD_d_windowLogMax, window_log_max);
    if (dictionary_) {
      size_t result = ZSTD_DCtx_loadDictionary_advanced(
          dctx_.get(), reinterpret_cast<const void*>(dictionary_->data()),
          dictionary_size_, ZSTD_dlm_byRef, ZSTD_dct_rawContent);
      DCHECK(!ZSTD_isError(result));
    }
  }

  ZstdSourceStream(const ZstdSourceStream&) = delete;
  ZstdSourceStream& operator=(const ZstdSourceStream&) = delete;

  ~ZstdSourceStream() override {
    if (ZSTD_isError(decoding_result_)) {
      ZSTD_ErrorCode error_code = ZSTD_getErrorCode(decoding_result_);
      UMA_HISTOGRAM_ENUMERATION(
          "Net.ZstdFilter.ErrorCode", static_cast<int>(error_code),
          static_cast<int>(ZSTD_ErrorCode::ZSTD_error_maxCode));
    }

    UMA_HISTOGRAM_ENUMERATION("Net.ZstdFilter.Status", decoding_status_);

    if (decoding_status_ == ZstdDecodingStatus::kEndOfFrame) {
      // CompressionRatio is undefined when there is no output produced.
      if (produced_bytes_ != 0) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Net.ZstdFilter.CompressionRatio",
            static_cast<int>((consumed_bytes_ * 100) / produced_bytes_));
      }
    }

    UMA_HISTOGRAM_MEMORY_KB("Net.ZstdFilter.MaxMemoryUsage",
                            (max_allocated_ / 1024));
  }

 private:
  static void* customMalloc(void* opaque, size_t size) {
    return reinterpret_cast<ZstdSourceStream*>(opaque)->customMalloc(size);
  }

  void* customMalloc(size_t size) {
    void* address = malloc(size);
    CHECK(address);
    malloc_sizes_.emplace(address, size);
    total_allocated_ += size;
    if (total_allocated_ > max_allocated_) {
      max_allocated_ = total_allocated_;
    }
    return address;
  }

  static void customFree(void* opaque, void* address) {
    return reinterpret_cast<ZstdSourceStream*>(opaque)->customFree(address);
  }

  void customFree(void* address) {
    free(address);
    auto it = malloc_sizes_.find(address);
    CHECK(it != malloc_sizes_.end());
    const size_t size = it->second;
    total_allocated_ -= size;
    malloc_sizes_.erase(it);
  }

  // SourceStream implementation
  std::string GetTypeAsString() const override { return kZstd; }

  base::expected<size_t, Error> FilterData(IOBuffer* output_buffer,
                                           size_t output_buffer_size,
                                           IOBuffer* input_buffer,
                                           size_t input_buffer_size,
                                           size_t* consumed_bytes,
                                           bool upstream_end_reached) override {
    CHECK(dctx_);
    ZSTD_inBuffer input = {input_buffer->data(), input_buffer_size, 0};
    ZSTD_outBuffer output = {output_buffer->data(), output_buffer_size, 0};

    const size_t result = ZSTD_decompressStream(dctx_.get(), &output, &input);

    decoding_result_ = result;

    produced_bytes_ += output.pos;
    consumed_bytes_ += input.pos;

    *consumed_bytes = input.pos;

    if (ZSTD_isError(result)) {
      decoding_status_ = ZstdDecodingStatus::kDecodingError;
      if (ZSTD_getErrorCode(result) ==
          ZSTD_error_frameParameter_windowTooLarge) {
        return base::unexpected(ERR_ZSTD_WINDOW_SIZE_TOO_BIG);
      }
      return base::unexpected(ERR_CONTENT_DECODING_FAILED);
    } else if (input.pos < input.size) {
      // Given a valid frame, zstd won't consume the last byte of the frame
      // until it has flushed all of the decompressed data of the frame.
      // Therefore, instead of checking if the return code is 0, we can
      // just check if input.pos < input.size.
      return output.pos;
    } else {
      CHECK_EQ(input.pos, input.size);
      if (result != 0u) {
        // The return value from ZSTD_decompressStream did not end on a frame,
        // but we reached the end of the file. We assume this is an error, and
        // the input was truncated.
        if (upstream_end_reached) {
          decoding_status_ = ZstdDecodingStatus::kDecodingError;
        }
      } else {
        CHECK_EQ(result, 0u);
        CHECK_LE(output.pos, output.size);
        // Finished decoding a frame.
        decoding_status_ = ZstdDecodingStatus::kEndOfFrame;
      }
      return output.pos;
    }
  }

  size_t total_allocated_ = 0;
  size_t max_allocated_ = 0;
  std::unordered_map<void*, size_t> malloc_sizes_;

  const scoped_refptr<IOBuffer> dictionary_;
  const size_t dictionary_size_;

  std::unique_ptr<ZSTD_DCtx, FreeContextDeleter> dctx_;

  ZstdDecodingStatus decoding_status_ = ZstdDecodingStatus::kDecodingInProgress;

  size_t decoding_result_ = 0;
  size_t consumed_bytes_ = 0;
  size_t produced_bytes_ = 0;
};

}  // namespace

std::unique_ptr<FilterSourceStream> CreateZstdSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return std::make_unique<ZstdSourceStream>(std::move(previous));
}

std::unique_ptr<FilterSourceStream> CreateZstdSourceStreamWithDictionary(
    std::unique_ptr<SourceStream> previous,
    scoped_refptr<IOBuffer> dictionary,
    size_t dictionary_size) {
  return std::make_unique<ZstdSourceStream>(
      std::move(previous), std::move(dictionary), dictionary_size);
}

}  // namespace net
