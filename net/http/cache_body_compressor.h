// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_CACHE_BODY_COMPRESSOR_H_
#define NET_HTTP_CACHE_BODY_COMPRESSOR_H_

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

#if !defined(NET_DISABLE_ZSTD_COMPRESS)
// Forward-declare the zstd compression context to avoid pulling zstd.h into
// this header. The full definition is only needed in the .cc file.
struct ZSTD_CCtx_s;
using ZSTD_CCtx = ZSTD_CCtx_s;
#endif

namespace net {

class IOBuffer;

// Encapsulates zstd streaming compression for writing cache entry bodies.
//
// This class owns all zstd compression state (ZSTD_CCtx, output buffer, byte
// counters) and exposes a minimal interface for HttpCache::Writers to call at
// well-defined points in its state machine. All zstd-specific logic is
// confined here, keeping the Writers state machine clean.
//
// Usage (see HttpCache::Writers::DoCacheWriteData /
// DoCacheWriteDataComplete for the real wiring):
//   compressor_ = std::make_unique<CacheBodyCompressor>();
//   if (!compressor_->Init()) { /* fall back to uncompressed */ }
//   ...
//   int out = compressor_->Compress(input, /*is_last_chunk=*/false);
//   if (out > 0)
//     entry->WriteData(..., compressor_->output_buffer(), out, ...);
//   ...
//   // At EOF:
//   int remaining = compressor_->Finalize();
//   // Write compressor_->output_buffer() with compressor_->output_length(),
//   // then repeat until Finalize() returns 0.
//
// Thread-safety: Not thread-safe. Must be used on a single sequence.
class NET_EXPORT_PRIVATE CacheBodyCompressor {
 public:
  CacheBodyCompressor();
  ~CacheBodyCompressor();

  CacheBodyCompressor(const CacheBodyCompressor&) = delete;
  CacheBodyCompressor& operator=(const CacheBodyCompressor&) = delete;

  // Initializes the zstd compression context with level 1 (fastest setting
  // that still gives useful compression). Returns true on success, false if
  // initialization fails. Must be called before Compress() or Finalize().
  bool Init();

  // Compresses `input` bytes into the internal output buffer. If
  // `is_last_chunk` is true, the zstd frame is also finalized in the same
  // call (equivalent to calling Finalize() inline after the last Compress()).
  //
  // Returns:
  //   > 0: number of compressed bytes available in output_buffer()
  //     0: zstd buffered the input internally and produced no output yet
  //   < 0: compression error (ERR_CACHE_COMPRESSION_FAILURE)
  //
  // After a successful call with return > 0, use output_buffer() and
  // output_length() to retrieve the bytes to write to disk.
  int Compress(base::span<const uint8_t> input, bool is_last_chunk);

  // Flushes any remaining compressed data by ending the zstd frame. May need
  // to be called multiple times if the output buffer fills before the frame
  // epilogue is fully emitted.
  //
  // Returns:
  //   >= 0: number of bytes that zstd still needs to flush. When 0,
  //         finalization is complete.
  //   < 0:  compression error (ERR_CACHE_COMPRESSION_FAILURE).
  //
  // After each successful call, output_length() contains the bytes the
  // caller should write to disk.
  int Finalize();

  // Returns the output buffer containing compressed data from the most recent
  // Compress() or Finalize() call.
  //
  // The returned pointer is valid until the next call to Compress(),
  // Finalize(), or Reset(). The caller must copy or write the data before
  // making another call.
  //
  // Truncation contract: if the network stream is interrupted mid-body, the
  // caller MUST doom the cache entry — a partial zstd frame is undecodable.
  // Do NOT store a truncated compressed body.
  IOBuffer* output_buffer() const { return output_buffer_.get(); }

  // Returns the number of compressed bytes produced by the most recent
  // Compress() or Finalize() call.
  size_t output_length() const { return output_length_; }

  // Returns total uncompressed input bytes processed across all Compress()
  // calls. Used by the caller to populate
  // HttpResponseInfo::zstd_uncompressed_body_size, which the read path uses
  // as the over-decompression bound. (Note: this is intentionally the count
  // of plaintext bytes streamed into zstd, NOT the wire Content-Length —
  // see HttpResponseInfo for why those are not the same thing.)
  int64_t total_input_bytes() const { return total_input_bytes_; }

  // Resets per-stream state so the same instance can be reused. The zstd
  // context is reset (not freed) to avoid re-allocation on the next Init().
  // Safe to call even if Init() was never called.
  void Reset();

 private:
#if !defined(NET_DISABLE_ZSTD_COMPRESS)
  struct ZstdCCtxDeleter {
    void operator()(ZSTD_CCtx* ctx) const;
  };

  // Grows output_buffer_ so it has room for at least `needed` more bytes.
  // Preserves the first `bytes_already_written` bytes. Growth is rounded up
  // to at least buffer_size/16 to prevent slow linear resizing.
  void GrowOutputBufferSizeByAtLeast(size_t needed,
                                     size_t bytes_already_written);

  std::unique_ptr<ZSTD_CCtx, ZstdCCtxDeleter> cctx_;
#endif

  scoped_refptr<IOBuffer> output_buffer_;
  size_t output_length_ = 0;
  int64_t total_input_bytes_ = 0;
};

}  // namespace net

#endif  // NET_HTTP_CACHE_BODY_COMPRESSOR_H_
