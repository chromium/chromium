// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/cache_body_decompressor.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if !defined(NET_DISABLE_ZSTD)
#include "third_party/zstd/src/lib/zstd.h"
#endif

namespace net {

CacheBodyDecompressor::CacheBodyDecompressor() = default;

CacheBodyDecompressor::~CacheBodyDecompressor() {
  Reset();
}

bool CacheBodyDecompressor::Init() {
#if !defined(NET_DISABLE_ZSTD)
  CHECK(!dctx_);
  dctx_.reset(ZSTD_createDCtx());
  if (!dctx_) {
    DVLOG(1) << "Failed to create zstd decompression context";
    return false;
  }
  // Cap the maximum window size to prevent a malicious or corrupt cache entry
  // from requesting a huge window (up to 2GB by default), which would cause
  // excessive memory allocation. 23 = log2(8MB).
  //
  // RFC 9659 Section 6.1 permits dictionary-compressed responses to use windows
  // up to 2^27 (128 MB), so this cap is intentionally tighter than the spec
  // ceiling: an 8 MB working set is sufficient for the cache-storage
  // workload (where the writer controls compression parameters and there is
  // no need to honor servers' larger windows). CacheBodyCompressor uses zstd
  // level 1 whose default windowLog=19 (512 KB), well within this limit.
  // Entries written with a larger window will be silently rejected
  // here, dooming the entry and triggering a network
  // refetch (cache thrash).
  //
  // This cap is a security boundary: if the parameter cannot be set, fail
  // Init() rather than run with the default unbounded window.
  size_t param_result =
      ZSTD_DCtx_setParameter(dctx_.get(), ZSTD_d_windowLogMax, 23);
  if (ZSTD_isError(param_result)) {
    DVLOG(1) << "Failed to set zstd window limit: "
             << ZSTD_getErrorName(param_result);
    dctx_.reset();
    return false;
  }
  return true;
#else
  return false;
#endif
}

void CacheBodyDecompressor::EnsureInputBuffer(size_t min_size) {
  // Reallocating would silently drop any leftover bytes still held in the
  // buffer. Callers must drain leftovers before requesting a new disk read.
  CHECK_EQ(leftover_len_, 0u);
  if (!input_buffer_ || static_cast<size_t>(input_buffer_->size()) < min_size) {
    input_buffer_ = base::MakeRefCounted<IOBufferWithSize>(min_size);
  }
}

int CacheBodyDecompressor::Decompress(size_t bytes_read,
                                      IOBuffer* output_buf,
                                      size_t output_buf_len,
                                      size_t* bytes_consumed_out) {
  CHECK(input_buffer_);
  CHECK_GT(bytes_read, 0u);
  auto input_span = input_buffer_->span().first(bytes_read);
  return DoDecompress(input_span, /*input_offset=*/0, output_buf,
                      output_buf_len, bytes_consumed_out);
}

int CacheBodyDecompressor::DecompressLeftover(IOBuffer* output_buf,
                                              size_t output_buf_len,
                                              size_t* bytes_consumed_out) {
  CHECK(input_buffer_);
  CHECK_GT(leftover_len_, 0u);
  auto input_span =
      input_buffer_->span().subspan(leftover_offset_, leftover_len_);
  return DoDecompress(input_span, leftover_offset_, output_buf, output_buf_len,
                      bytes_consumed_out);
}

int CacheBodyDecompressor::DoDecompress(base::span<const uint8_t> input,
                                        size_t input_offset,
                                        IOBuffer* output_buf,
                                        size_t output_buf_len,
                                        size_t* bytes_consumed_out) {
#if !defined(NET_DISABLE_ZSTD)
  CHECK(dctx_);

  ZSTD_inBuffer zstd_input = {input.data(), input.size(), 0};
  ZSTD_outBuffer zstd_output = {output_buf->data(), output_buf_len, 0};

  size_t decompress_result =
      ZSTD_decompressStream(dctx_.get(), &zstd_output, &zstd_input);
  if (ZSTD_isError(decompress_result)) {
    DVLOG(1) << "zstd decompression error: "
             << ZSTD_getErrorName(decompress_result);
    *bytes_consumed_out = 0;
    return ERR_CACHE_READ_FAILURE;
  }

  size_t consumed = zstd_input.pos;
  *bytes_consumed_out = consumed;

  size_t remaining = input.size() - consumed;
  if (remaining > 0) {
    leftover_offset_ = input_offset + consumed;
    leftover_len_ = remaining;
  } else {
    leftover_offset_ = 0;
    leftover_len_ = 0;
  }

  size_t decompressed_bytes = zstd_output.pos;

  // Track whether the frame has been fully decoded. A return value of 0 from
  // ZSTD_decompressStream means the frame is complete; non-zero means "more
  // input expected" (hint, not size).
  frame_complete_ = (decompress_result == 0);

  if (decompressed_bytes == 0) {
    // No output produced. This is legal only if zstd consumed input (e.g.,
    // parsing frame headers) — otherwise we'd loop forever feeding the same
    // bytes back in.
    if (consumed == 0) {
      DVLOG(1) << "zstd decompression produced no output and consumed no "
               << "input; treating as malformed stream";
      return ERR_CACHE_READ_FAILURE;
    }
    // Enforce the zero-output limit here so callers cannot forget the check.
    // Frame headers and skippable blocks can legitimately produce no output
    // for a call or two, but a sustained run indicates a malformed stream or
    // an infinite loop.
    if (++consecutive_zero_output_count_ > kMaxConsecutiveZeroOutputCount) {
      DVLOG(1) << "zstd decompression zero-output limit exceeded ("
               << consecutive_zero_output_count_ << ")";
      return ERR_CACHE_READ_FAILURE;
    }
    return 0;
  }

  // Reset on successful output production.
  consecutive_zero_output_count_ = 0;
  total_output_bytes_ += decompressed_bytes;

  // Absolute size cap: see kMaxDecompressedBodySize in the header for
  // rationale. Catches zstd-bomb entries with no/lying Content-Length
  // before unbounded output reaches the consumer.
  if (total_output_bytes_ > kMaxDecompressedBodySize) {
    DVLOG(1) << "zstd decompression exceeded absolute size cap "
             << kMaxDecompressedBodySize << " (produced " << total_output_bytes_
             << ")";
    return ERR_CACHE_READ_FAILURE;
  }

  // Streaming over-decompression guard: if the caller set an expected
  // content length and we've already produced more bytes than advertised,
  // the compressed data is corrupt or tampered. Fail now instead of
  // delivering excess bytes to the consumer and only detecting the
  // mismatch at EOF.
  if (expected_content_length_.has_value() &&
      total_output_bytes_ > *expected_content_length_) {
    DVLOG(1) << "zstd decompression produced " << total_output_bytes_
             << " bytes, exceeding advertised content length "
             << *expected_content_length_;
    return ERR_CACHE_READ_FAILURE;
  }

  return base::checked_cast<int>(decompressed_bytes);
#else
  *bytes_consumed_out = 0;
  return ERR_CACHE_READ_FAILURE;
#endif
}

void CacheBodyDecompressor::Reset() {
#if !defined(NET_DISABLE_ZSTD)
  dctx_.reset();
#endif
  input_buffer_ = nullptr;
  leftover_offset_ = 0;
  leftover_len_ = 0;
  consecutive_zero_output_count_ = 0;
  total_output_bytes_ = 0;
  expected_content_length_ = std::nullopt;
  frame_complete_ = false;
}

}  // namespace net
