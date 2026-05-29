// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/cache_body_compressor.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace net {

namespace {

// Returns a deterministic, compressible plaintext of `size` bytes.
std::string MakeCompressiblePlaintext(size_t size) {
  std::string s;
  s.reserve(size);
  // Repeating brief pattern — zstd compresses this very well, useful for
  // verifying compressed_size < plaintext_size.
  static constexpr std::string_view kPattern =
      "The quick brown fox jumps over the lazy dog. ";
  while (s.size() < size) {
    s.append(kPattern.substr(0, std::min(kPattern.size(), size - s.size())));
  }
  return s;
}

// Decompresses `compressed` via zstd streaming (NOT one-shot), asserting
// success, and returns the plaintext.
//
// We use the streaming decoder rather than ZSTD_decompress() because the
// production code does NOT pre-declare the decompressed size via
// ZSTD_CCtx_setPledgedSrcSize() — so the frame header has
// ZSTD_CONTENTSIZE_UNKNOWN, which the one-shot decoder cannot handle. The
// streaming decoder just consumes the frame and tells us how many bytes
// it produced.
std::string DecompressZstdFrame(const std::vector<uint8_t>& compressed) {
  std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx(ZSTD_createDCtx(),
                                                            &ZSTD_freeDCtx);
  if (!dctx) {
    ADD_FAILURE() << "ZSTD_createDCtx failed";
    return std::string();
  }

  // 256 KB output staging covers our test fixtures (up to 200 KB plaintext).
  std::string out;
  std::vector<uint8_t> staging(256 * 1024);
  ZSTD_inBuffer zstd_input = {compressed.data(), compressed.size(), 0};
  while (zstd_input.pos < zstd_input.size) {
    ZSTD_outBuffer zstd_output = {staging.data(), staging.size(), 0};
    size_t result =
        ZSTD_decompressStream(dctx.get(), &zstd_output, &zstd_input);
    if (ZSTD_isError(result)) {
      ADD_FAILURE() << "ZSTD_decompressStream: " << ZSTD_getErrorName(result);
      return std::string();
    }
    out.append(
        base::as_string_view(base::span(staging).first(zstd_output.pos)));
    // result == 0 means the frame is complete. If there's still more input
    // after that, the test would be feeding multiple frames — not a case
    // our compressor produces, so treat extra input as a fixture bug.
    if (result == 0 && zstd_input.pos < zstd_input.size) {
      ADD_FAILURE() << "unexpected trailing bytes after zstd frame";
      return std::string();
    }
  }
  return out;
}

// Appends `length` bytes from `buf` into `sink`.
void AppendToVec(std::vector<uint8_t>* sink, IOBuffer* buf, size_t length) {
  sink->append_range(buf->span().first(length));
}

}  // namespace

class CacheBodyCompressorTest : public testing::Test {};

// Init() must succeed when zstd is compiled in.
TEST_F(CacheBodyCompressorTest, InitSucceeds) {
  CacheBodyCompressor compressor;
  EXPECT_TRUE(compressor.Init());
  EXPECT_EQ(0u, compressor.output_length());
  EXPECT_EQ(0, compressor.total_input_bytes());
}

// Single-shot compression of a small payload with is_last_chunk=true must
// produce a complete frame that round-trips to the original bytes.
TEST_F(CacheBodyCompressorTest, CompressSingleShotRoundTrip) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string plaintext = MakeCompressiblePlaintext(2048);
  int out = compressor.Compress(base::as_byte_span(plaintext),
                                /*is_last_chunk=*/true);
  ASSERT_GT(out, 0);
  EXPECT_EQ(out, compressor.output_length());
  EXPECT_EQ(static_cast<int64_t>(plaintext.size()),
            compressor.total_input_bytes());

  std::vector<uint8_t> compressed;
  AppendToVec(&compressed, compressor.output_buffer(), out);
  EXPECT_EQ(plaintext, DecompressZstdFrame(compressed));
}

// Streaming compression mirroring production usage: split the plaintext
// across N Compress() calls, with `is_last_chunk=true` on the final one
// (no separate Finalize() needed). The concatenated output must round-trip.
TEST_F(CacheBodyCompressorTest, CompressStreamingRoundTrip) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string plaintext = MakeCompressiblePlaintext(8192);
  constexpr size_t kChunk = 1500;

  std::vector<uint8_t> compressed;
  auto remaining = base::as_byte_span(plaintext);
  while (!remaining.empty()) {
    auto chunk = remaining.first(std::min(kChunk, remaining.size()));
    remaining = remaining.subspan(chunk.size());
    int out = compressor.Compress(chunk, remaining.empty());
    ASSERT_GE(out, 0) << "Unexpected error mid-stream";
    if (out > 0) {
      AppendToVec(&compressed, compressor.output_buffer(), out);
    }
  }

  EXPECT_EQ(static_cast<int64_t>(plaintext.size()),
            compressor.total_input_bytes());
  EXPECT_EQ(plaintext, DecompressZstdFrame(compressed));
}

// Explicit Finalize() path: every Compress() call uses is_last_chunk=false,
// then Finalize() flushes any buffered data and emits the frame epilogue.
// May need multiple Finalize() iterations if the output buffer is small.
TEST_F(CacheBodyCompressorTest, CompressFinalizeRoundTrip) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string plaintext = MakeCompressiblePlaintext(8192);
  constexpr size_t kChunk = 1500;

  std::vector<uint8_t> compressed;
  auto remaining = base::as_byte_span(plaintext);
  while (!remaining.empty()) {
    auto chunk = remaining.first(std::min(kChunk, remaining.size()));
    remaining = remaining.subspan(chunk.size());
    int out = compressor.Compress(chunk, /*is_last_chunk=*/false);
    ASSERT_GE(out, 0) << "Unexpected error mid-stream";
    if (out > 0) {
      AppendToVec(&compressed, compressor.output_buffer(), out);
    }
  }

  // Finalize: may need multiple calls if the buffer fills.
  while (true) {
    int fin_remaining = compressor.Finalize();
    ASSERT_GE(fin_remaining, 0);
    if (compressor.output_length() > 0) {
      AppendToVec(&compressed, compressor.output_buffer(),
                  compressor.output_length());
    }
    if (fin_remaining == 0) {
      break;
    }
  }

  EXPECT_EQ(static_cast<int64_t>(plaintext.size()),
            compressor.total_input_bytes());
  EXPECT_EQ(plaintext, DecompressZstdFrame(compressed));
}

// A compressible payload must produce a smaller-or-equal compressed stream.
// The frame epilogue makes very small inputs occasionally grow, so we use a
// payload large enough that zstd-level-1 reliably shrinks it.
TEST_F(CacheBodyCompressorTest, CompressibleInputShrinks) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string plaintext = MakeCompressiblePlaintext(8192);
  int out = compressor.Compress(base::as_byte_span(plaintext),
                                /*is_last_chunk=*/true);
  ASSERT_GT(out, 0);
  EXPECT_LT(out, static_cast<int>(plaintext.size()))
      << "Expected highly-redundant 8KB input to compress smaller than "
      << "plaintext at level 1";
}

// Reset() must clear all per-stream state so the same instance can be reused.
// Re-initializing after Reset and running another round trip must succeed.
TEST_F(CacheBodyCompressorTest, ResetAllowsReuse) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string first = MakeCompressiblePlaintext(1024);
  int out1 = compressor.Compress(base::as_byte_span(first),
                                 /*is_last_chunk=*/true);
  ASSERT_GT(out1, 0);
  EXPECT_EQ(static_cast<int64_t>(first.size()), compressor.total_input_bytes());

  // Reset clears per-stream state; the zstd context is reset, not freed.
  compressor.Reset();
  EXPECT_EQ(0u, compressor.output_length());
  EXPECT_EQ(0, compressor.total_input_bytes());

  // Re-init and round-trip a different payload.
  ASSERT_TRUE(compressor.Init());
  const std::string second = MakeCompressiblePlaintext(2048);
  int out2 = compressor.Compress(base::as_byte_span(second),
                                 /*is_last_chunk=*/true);
  ASSERT_GT(out2, 0);
  EXPECT_EQ(static_cast<int64_t>(second.size()),
            compressor.total_input_bytes());

  std::vector<uint8_t> compressed;
  AppendToVec(&compressed, compressor.output_buffer(), out2);
  EXPECT_EQ(second, DecompressZstdFrame(compressed));
}

// Verify total_input_bytes() accumulates across multiple Compress() calls
// in a streaming session.
TEST_F(CacheBodyCompressorTest, TotalInputBytesAccumulates) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  const std::string a(1000, 'a');
  const std::string b(2000, 'b');
  const std::string c(500, 'c');

  ASSERT_GE(compressor.Compress(base::as_byte_span(a), /*is_last_chunk=*/false),
            0);
  ASSERT_GE(compressor.Compress(base::as_byte_span(b), /*is_last_chunk=*/false),
            0);
  ASSERT_GE(compressor.Compress(base::as_byte_span(c), /*is_last_chunk=*/true),
            0);

  EXPECT_EQ(static_cast<int64_t>(a.size() + b.size() + c.size()),
            compressor.total_input_bytes());
}

// Compressing an empty payload with is_last_chunk=true must produce a valid
// zstd frame that decodes to empty output.
TEST_F(CacheBodyCompressorTest, CompressEmptyInputProducesValidFrame) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  base::span<const uint8_t> empty;
  int out = compressor.Compress(empty, /*is_last_chunk=*/true);
  ASSERT_GT(out, 0) << "Even an empty input must emit a frame epilogue";
  EXPECT_EQ(0, compressor.total_input_bytes());

  std::vector<uint8_t> compressed;
  AppendToVec(&compressed, compressor.output_buffer(), out);
  EXPECT_EQ(std::string(), DecompressZstdFrame(compressed));
}

// Regression test: streaming 200 KB of incompressible data in small 10 KB
// chunks must not fail when zstd flushes a full block (128 KB). Before the
// retry-loop buffer growth fix, this triggered ERR_FAILED at ~120 KB.
TEST_F(CacheBodyCompressorTest, UncompressibleStreamingLargePayload) {
  CacheBodyCompressor compressor;
  ASSERT_TRUE(compressor.Init());

  // Deterministic pseudo-random data that zstd cannot compress.
  std::vector<uint8_t> plaintext(200 * 1024);
  uint32_t seed = 42;
  for (auto& byte : plaintext) {
    seed = seed * 1103515245 + 12345;
    byte = static_cast<uint8_t>((seed / 65536) % 256);
  }

  constexpr size_t kChunk = 10 * 1024;
  std::vector<uint8_t> compressed;

  auto remaining = base::span(plaintext);
  while (!remaining.empty()) {
    auto chunk = remaining.first(std::min(kChunk, remaining.size()));
    remaining = remaining.subspan(chunk.size());

    int out = compressor.Compress(chunk, remaining.empty());
    ASSERT_GE(out, 0) << "Failed to compress chunk";
    if (out > 0) {
      AppendToVec(&compressed, compressor.output_buffer(), out);
    }
  }

  std::string decompressed = DecompressZstdFrame(compressed);
  ASSERT_EQ(plaintext.size(), decompressed.size());
  EXPECT_EQ(base::span(plaintext), base::as_byte_span(decompressed));
}

}  // namespace net
