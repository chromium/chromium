// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/cert_compression.h"

#include <cstdint>

#include "third_party/boringssl/src/include/openssl/ssl.h"

#if !defined(NET_DISABLE_BROTLI)
#include "third_party/brotli/include/brotli/decode.h"
#endif

namespace net {
namespace {

#if !defined(NET_DISABLE_BROTLI)
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

  size_t output_size = uncompressed_len;
  if (BrotliDecoderDecompress(in_len, in, &output_size, data) !=
          BROTLI_DECODER_RESULT_SUCCESS ||
      output_size != uncompressed_len) {
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
