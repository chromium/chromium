// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_CACHE_BODY_DECOMPRESSOR_H_
#define NET_HTTP_CACHE_BODY_DECOMPRESSOR_H_

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

#if !defined(NET_DISABLE_ZSTD)
#include "third_party/zstd/src/lib/zstd.h"
#endif

namespace net {

class IOBuffer;

// Encapsulates zstd streaming decompression for reading compressed cache
// entry bodies.
//
// This class owns all zstd decompression state (ZSTD_DCtx, intermediate
// input buffer, leftover tracking, byte counters, frame-complete flag) and
// exposes a minimal interface for HttpCache::Transaction to call at
// well-defined points in its state machine. All zstd-specific logic is
// confined here.
//
// Usage (see HttpCache::Transaction::DoCacheReadData /
// DoCacheReadDataComplete for the real wiring):
//   decompressor_ = std::make_unique<CacheBodyDecompressor>();
//   if (!decompressor_->Init()) { /* error */ }
//   ...
//   // In DoCacheReadData(), before each disk read:
//   if (decompressor_->has_leftover()) {
//     // Drain bytes zstd already holds from a prior disk read. Skip disk.
//   } else {
//     decompressor_->EnsureInputBuffer(read_buf_len_);
//     entry->ReadData(..., decompressor_->input_buffer(), ..., callback);
//   }
//   ...
//   // In DoCacheReadDataComplete():
//   int consumed = 0;
//   int out = decompressor_->has_leftover()
//       ? decompressor_->DecompressLeftover(read_buf, len, &consumed)
//       : decompressor_->Decompress(bytes_read, read_buf, len, &consumed);
//   read_offset_ += consumed;
//
//   // On EOF (disk read returned 0):
//   if (!decompressor_->frame_complete()) { /* truncated, fail */ }
//
// Thread-safety: Not thread-safe. Must be used on a single sequence.
class NET_EXPORT_PRIVATE CacheBodyDecompressor {
 public:
  CacheBodyDecompressor();
  ~CacheBodyDecompressor();

  CacheBodyDecompressor(const CacheBodyDecompressor&) = delete;
  CacheBodyDecompressor& operator=(const CacheBodyDecompressor&) = delete;

  // Initializes the zstd decompression context.
  // Returns true on success, false if initialization fails.
  bool Init();

  // Returns the input buffer that callers should read compressed data into.
  // The buffer is allocated by EnsureInputBuffer() and reused across calls.
  IOBuffer* input_buffer() const { return input_buffer_.get(); }

  // Ensures the input buffer is at least `min_size` bytes, allocating or
  // growing as needed.
  //
  // Precondition: has_leftover() must be false. Reallocating a buffer that
  // still holds unconsumed bytes would silently drop them.
  void EnsureInputBuffer(size_t min_size);

  // Returns true if there are unconsumed compressed bytes from a previous
  // Decompress()/DecompressLeftover() call. When true, the caller should
  // call DecompressLeftover() (no disk read) instead of reading new bytes
  // from disk.
  bool has_leftover() const { return leftover_len_ > 0; }

  // Returns the number of unconsumed compressed bytes in input_buffer().
  size_t leftover_len() const { return leftover_len_; }

  // Decompresses the first `bytes_read` bytes of input_buffer() into
  // `output_buf` (up to `output_buf_len` bytes). Intended to be called after
  // a fresh disk read filled input_buffer() with `bytes_read` compressed
  // bytes.
  //
  // Returns:
  //   > 0: number of decompressed bytes written to `output_buf`
  //     0: zstd consumed input but produced no output (e.g., while parsing
  //        a frame header). The caller should read more compressed data
  //        from disk and call again. The decompressor enforces
  //        kMaxConsecutiveZeroOutputCount internally, so the caller does
  //        not need a separate guard against zero-output infinite loops.
  //   < 0: decompression error (ERR_CACHE_READ_FAILURE). Includes zstd
  //        errors, the case where zstd neither consumed input nor produced
  //        output (would otherwise infinite-loop), and exceeding the
  //        consecutive zero-output limit.
  //
  // `bytes_consumed_out` is set to the number of compressed input bytes
  // consumed by zstd. The caller uses this to advance its disk read offset.
  // Any remaining bytes are tracked internally and reported via
  // has_leftover()/leftover_len() for the next DecompressLeftover() call.
  int Decompress(size_t bytes_read,
                 IOBuffer* output_buf,
                 size_t output_buf_len,
                 size_t* bytes_consumed_out);

  // Decompresses the leftover bytes tracked from a previous call into
  // `output_buf`. Precondition: has_leftover() is true.
  //
  // Returns the same values as Decompress(). `bytes_consumed_out` is set
  // to the number of additional leftover bytes consumed on this call.
  int DecompressLeftover(IOBuffer* output_buf,
                         size_t output_buf_len,
                         size_t* bytes_consumed_out);

  // Returns the number of consecutive Decompress()/DecompressLeftover() calls
  // that produced 0 output bytes. Exposed for diagnostics / tests only — the
  // limit is enforced internally by DoDecompress() which returns an error as
  // soon as the count exceeds kMaxConsecutiveZeroOutputCount. Callers do not
  // need to check this themselves.
  int consecutive_zero_output_count() const {
    return consecutive_zero_output_count_;
  }

  // Upper bound on consecutive zero-output decompression calls before we
  // declare the stream corrupt. Zstd frame headers and metadata blocks can
  // legitimately consume input without producing output for one or two calls,
  // but a sustained run indicates a malformed stream or an infinite loop. A
  // well-formed frame should never need more than a handful of zero-output
  // iterations before producing data, so this bound is intentionally tight.
  static constexpr int kMaxConsecutiveZeroOutputCount = 20;

  // Hard cap on decompressed output, applied even when Content-Length is
  // missing or lies. Disk backends already reject entries larger than 2 GB,
  // so this only fires on zstd-bomb input.
  static constexpr int64_t kMaxDecompressedBodySize = 2LL * 1024 * 1024 * 1024;

  // Returns true if zstd signaled that the compressed frame has been fully
  // decoded (i.e., ZSTD_decompressStream returned 0). The caller should check
  // this at EOF — if the disk read returns 0 but the frame is not complete,
  // the cache entry is truncated.
  bool frame_complete() const { return frame_complete_; }

  // Sets an upper bound on decompressed output. When set, DoDecompress()
  // returns ERR_CACHE_READ_FAILURE as soon as total_output_bytes_ would
  // exceed this value, catching over-decompression mid-stream rather than
  // only at EOF.
  // Call after Init() and before the first Decompress()/DecompressLeftover().
  //
  // `len` must be non-negative. A negative sentinel would silently invert
  // the over-decompression check (every output byte would exceed the bound),
  // so we CHECK rather than tolerate it.
  void set_expected_content_length(int64_t len) {
    CHECK_GE(len, 0);
    expected_content_length_ = len;
  }

  // Total decompressed output bytes produced across all calls.
  int64_t total_output_bytes() const { return total_output_bytes_; }

  // Resets all state. Safe to call even if Init() was never called.
  void Reset();

 private:
  // Core decompression logic shared by Decompress() and DecompressLeftover().
  // `input_offset` is the absolute offset into input_buffer_ where `input`
  // starts, used to compute leftover offsets without pointer arithmetic.
  int DoDecompress(base::span<const uint8_t> input,
                   size_t input_offset,
                   IOBuffer* output_buf,
                   size_t output_buf_len,
                   size_t* bytes_consumed_out);

#if !defined(NET_DISABLE_ZSTD)
  struct ZstdDCtxDeleter {
    void operator()(ZSTD_DCtx* ctx) const { ZSTD_freeDCtx(ctx); }
  };
  std::unique_ptr<ZSTD_DCtx, ZstdDCtxDeleter> dctx_;
#endif

  scoped_refptr<IOBuffer> input_buffer_;

  // Leftover tracking: unconsumed compressed bytes from previous disk read.
  size_t leftover_offset_ = 0;
  size_t leftover_len_ = 0;

  int consecutive_zero_output_count_ = 0;
  int64_t total_output_bytes_ = 0;
  std::optional<int64_t> expected_content_length_;
  bool frame_complete_ = false;
};

}  // namespace net

#endif  // NET_HTTP_CACHE_BODY_DECOMPRESSOR_H_
