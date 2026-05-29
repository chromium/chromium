// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/cache_body_compressor.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if !defined(NET_DISABLE_ZSTD_COMPRESS)
#include "third_party/zstd/src/lib/zstd.h"
#endif

namespace net {

#if !defined(NET_DISABLE_ZSTD_COMPRESS)
void CacheBodyCompressor::ZstdCCtxDeleter::operator()(ZSTD_CCtx* ctx) const {
  ZSTD_freeCCtx(ctx);
}
#endif

CacheBodyCompressor::CacheBodyCompressor() = default;

CacheBodyCompressor::~CacheBodyCompressor() {
  Reset();
}

bool CacheBodyCompressor::Init() {
#if !defined(NET_DISABLE_ZSTD_COMPRESS)
  // TODO(crbug.com/406988080): Add UmaHistogramEnumeration for zstd error
  // telemetry.
  if (!cctx_) {
    cctx_.reset(ZSTD_createCCtx());
    if (!cctx_) {
      DVLOG(1) << "Failed to create zstd compression context";
      return false;
    }

    // Level 1 minimizes CPU overhead; cache writes happen inline on the network
    // IO path so latency matters more than squeezing out the last few percent
    // of ratio. Level 1's default windowLog=19 (512 KB) is well within the
    // decompressor's ZSTD_d_windowLogMax=23 limit.
    size_t result =
        ZSTD_CCtx_setParameter(cctx_.get(), ZSTD_c_compressionLevel, 1);
    if (ZSTD_isError(result)) {
      DVLOG(1) << "Failed to set zstd compression level: "
               << ZSTD_getErrorName(result);
      cctx_.reset();
      return false;
    }
  }

  // Pre-allocate the output buffer. 32 KB matches the default read buffer
  // size used by HttpCache::Transaction (kDefaultCacheReadBufLen), so in the
  // common case the buffer is allocated exactly once.
  //
  // ZSTD_compressBound() is documented for single-shot compression; for
  // streaming, the actual output per call can vary unpredictably. We use it
  // as a reasonable initial estimate and rely on the retry loop in Compress()
  // to grow the buffer if needed. This keeps all zstd complexity inside this
  // class so HttpCache::Writers (which is already complex) stays clean.
  constexpr size_t kDefaultCompressInputSize = 32 * 1024;
  output_buffer_ = base::MakeRefCounted<IOBufferWithSize>(
      base::checked_cast<int>(ZSTD_compressBound(kDefaultCompressInputSize)));
  output_length_ = 0;
  total_input_bytes_ = 0;
  return true;
#else
  return false;
#endif
}

int CacheBodyCompressor::Compress(base::span<const uint8_t> input,
                                  bool is_last_chunk) {
#if !defined(NET_DISABLE_ZSTD_COMPRESS)
  CHECK(cctx_);

  ZSTD_inBuffer zstd_input = {input.data(), input.size(), 0};
  ZSTD_outBuffer zstd_output = {output_buffer_->data(),
                                static_cast<size_t>(output_buffer_->size()), 0};

  ZSTD_EndDirective mode = is_last_chunk ? ZSTD_e_end : ZSTD_e_continue;

  // Loop until all input is consumed AND (for is_last_chunk) the frame
  // epilogue is fully flushed. In the common case this completes in one
  // iteration; the loop handles the rare case where the output buffer is too
  // small for streaming flush of internally-buffered data.
  while (true) {
    size_t remaining =
        ZSTD_compressStream2(cctx_.get(), &zstd_output, &zstd_input, mode);
    if (ZSTD_isError(remaining)) {
      DVLOG(1) << "zstd compression error: " << ZSTD_getErrorName(remaining);
      output_length_ = 0;
      return ERR_CACHE_COMPRESSION_FAILURE;
    }

    bool input_done = (zstd_input.pos == zstd_input.size);
    bool frame_done = (is_last_chunk && remaining == 0) || !is_last_chunk;

    if (input_done && frame_done) {
      break;
    }

    // Output buffer was too small — grow it and retry.
    GrowOutputBufferSizeByAtLeast(remaining, zstd_output.pos);
    zstd_output.dst = output_buffer_->data();
    zstd_output.size = static_cast<size_t>(output_buffer_->size());
  }

  total_input_bytes_ += static_cast<int64_t>(input.size());
  output_length_ = zstd_output.pos;
  return base::checked_cast<int>(output_length_);
#else
  return ERR_CACHE_COMPRESSION_FAILURE;
#endif
}

int CacheBodyCompressor::Finalize() {
#if !defined(NET_DISABLE_ZSTD_COMPRESS)
  CHECK(cctx_);
  CHECK(output_buffer_);

  ZSTD_inBuffer zstd_input = {nullptr, 0, 0};
  ZSTD_outBuffer zstd_output = {output_buffer_->data(),
                                static_cast<size_t>(output_buffer_->size()), 0};

  size_t remaining =
      ZSTD_compressStream2(cctx_.get(), &zstd_output, &zstd_input, ZSTD_e_end);
  if (ZSTD_isError(remaining)) {
    DVLOG(1) << "zstd finalization error: " << ZSTD_getErrorName(remaining);
    output_length_ = 0;
    return ERR_CACHE_COMPRESSION_FAILURE;
  }

  output_length_ = zstd_output.pos;
  // `remaining` > 0 means zstd still has more output pending — the caller
  // needs to write the current output_buffer() and call Finalize() again.
  // `remaining` == 0 means the frame is complete.
  return base::checked_cast<int>(remaining);
#else
  return ERR_CACHE_COMPRESSION_FAILURE;
#endif
}

void CacheBodyCompressor::Reset() {
#if !defined(NET_DISABLE_ZSTD_COMPRESS)
  if (cctx_) {
    ZSTD_CCtx_reset(cctx_.get(), ZSTD_reset_session_only);
  }
#endif
  output_buffer_ = nullptr;
  output_length_ = 0;
  total_input_bytes_ = 0;
}

#if !defined(NET_DISABLE_ZSTD_COMPRESS)
void CacheBodyCompressor::GrowOutputBufferSizeByAtLeast(
    size_t needed,
    size_t bytes_already_written) {
  size_t old_size = static_cast<size_t>(output_buffer_->size());
  CHECK_GE(old_size, 16u);
  size_t min_growth = std::max(needed, old_size / 16);
  size_t new_size = old_size + std::max(min_growth, size_t{1});

  auto new_buffer =
      base::MakeRefCounted<IOBufferWithSize>(base::checked_cast<int>(new_size));
  if (bytes_already_written > 0) {
    new_buffer->span()
        .first(bytes_already_written)
        .copy_from(output_buffer_->span().first(bytes_already_written));
  }
  output_buffer_ = std::move(new_buffer);
}
#endif

}  // namespace net
