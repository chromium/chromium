// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/cert_compression.h"

#include <cstdint>

#include "third_party/boringssl/src/include/openssl/ssl.h"

#if !defined(NET_DISABLE_BROTLI)
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "third_party/brotli/include/brotli/decode.h"

#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
#include "net/filter/rust_brotli_decoder.h"
#endif
#endif

namespace net {
namespace {

#if !defined(NET_DISABLE_BROTLI)
// Decompresses `encoded` into `decoded`, with the Rust decoder if
// `features::kRustBrotliInNet` is enabled. Returns whether `encoded` starts
// with a complete brotli stream that decompresses to exactly `decoded.size()`
// bytes. Like `BrotliDecoderDecompress()`, it ignores bytes after the stream.
bool DecompressBrotli(base::span<const uint8_t> encoded,
                      base::span<uint8_t> decoded) {
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
  if (base::FeatureList::IsEnabled(features::kRustBrotliInNet)) {
    // RFC 8879 only allows brotli as RFC 7932 defines it, so like
    // `BrotliDecoderDecompress()`, the decoder rejects the large-window
    // extension.
    rust_brotli::Decoder decoder;
    const rust_brotli::DecodeResult result =
        decoder.decompress_stream(/*dictionary=*/{}, encoded, decoded);
    return result.result == BROTLI_DECODER_RESULT_SUCCESS &&
           result.bytes_written == decoded.size();
  }
#endif
  size_t decoded_size = decoded.size();
  return BrotliDecoderDecompress(encoded.size(), encoded.data(), &decoded_size,
                                 decoded.data()) ==
             BROTLI_DECODER_RESULT_SUCCESS &&
         decoded_size == decoded.size();
}

int DecompressBrotliCert(SSL* ssl,
                         CRYPTO_BUFFER** out,
                         size_t uncompressed_len,
                         const uint8_t* in,
                         size_t in_len) {
  uint8_t* data;
  bssl::UniquePtr<CRYPTO_BUFFER> decompressed(
      CRYPTO_BUFFER_alloc(&data, uncompressed_len));
  if (!decompressed) {
    return 0;
  }

  // SAFETY: BoringSSL passes `in_len` bytes at `in`, and
  // `CRYPTO_BUFFER_alloc()` allocated `uncompressed_len` bytes at `data`.
  const base::span<const uint8_t> encoded =
      UNSAFE_BUFFERS(base::span(in, in_len));
  const base::span<uint8_t> decoded =
      UNSAFE_BUFFERS(base::span(data, uncompressed_len));
  if (!DecompressBrotli(encoded, decoded)) {
    return 0;
  }

  *out = decompressed.release();
  return 1;
}
#endif

}  // namespace

void ConfigureCertificateCompression(SSL_CTX* ctx) {
#if !defined(NET_DISABLE_BROTLI)
  SSL_CTX_add_cert_compression_alg(ctx, TLSEXT_cert_compression_brotli,
                                   nullptr /* compression not supported */,
                                   DecompressBrotliCert);
#endif

  // Avoid "unused argument" errors in case no algorithms are supported.
  (void)(ctx);
}

}  // namespace net
