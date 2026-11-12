// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/cert_compression.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/brotli/include/brotli/encode.h"

namespace net {
namespace {

// How the test server tampers with the compressed certificate.
enum class Tamper {
  kNone,
  // Compresses one byte more than the certificate message.
  kAppendByte,
  // Compresses one byte less than the certificate message.
  kDropByte,
  // Sends only the first half of the compressed data.
  kTruncate,
  // Compresses with the large-window extension of brotli, which isn't part of
  // brotli as RFC 7932 defines it.
  kLargeWindow,
};

struct ServerState {
  Tamper tamper = Tamper::kNone;
  int compress_calls = 0;
};

int CompressBrotliCert(SSL* ssl, CBB* out, const uint8_t* in, size_t in_len) {
  auto* state = static_cast<ServerState*>(SSL_get_app_data(ssl));
  ++state->compress_calls;

  // SAFETY: BoringSSL's C callback gives the message only as a pointer and a
  // length, and `in` points to `in_len` valid bytes.
  std::vector<uint8_t> message =
      base::ToVector(UNSAFE_BUFFERS(base::span(in, in_len)));
  if (state->tamper == Tamper::kAppendByte) {
    message.push_back(0);
  } else if (state->tamper == Tamper::kDropByte) {
    message.pop_back();
  }

  std::vector<uint8_t> compressed(
      BrotliEncoderMaxCompressedSize(message.size()));
  size_t compressed_size = compressed.size();
  // A window larger than `BROTLI_MAX_WINDOW_BITS` turns on large windows.
  const int window_bits = state->tamper == Tamper::kLargeWindow
                              ? BROTLI_LARGE_MAX_WINDOW_BITS
                              : BROTLI_DEFAULT_WINDOW;
  if (!BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, window_bits,
                             BROTLI_MODE_GENERIC, message.size(),
                             message.data(), &compressed_size,
                             compressed.data())) {
    return 0;
  }
  compressed.resize(compressed_size);
  if (state->tamper == Tamper::kTruncate) {
    compressed.resize(compressed.size() / 2);
  }
  return CBB_add_bytes(out, compressed.data(), compressed.size());
}

struct HandshakeResult {
  // The first error that the client reported, or 0 if the handshake worked.
  uint32_t client_error = 0;
  // The certificate that the client received.
  std::vector<uint8_t> peer_cert;
};

// The parameter is whether `features::kRustBrotliInNet` is enabled.
class CertCompressionTest : public testing::TestWithParam<bool> {
 protected:
  CertCompressionTest() {
    feature_list_.InitWithFeatureState(features::kRustBrotliInNet, GetParam());
  }

  CertBuilder& leaf() { return *chain_[0]; }

  // Runs a TLS 1.3 handshake in memory, between a client that is set up with
  // `ConfigureCertificateCompression()` and a server that compresses its
  // certificate with brotli as `server_state` says.
  HandshakeResult Handshake(ServerState& server_state) {
    bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_with_buffers_method()));
    bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_with_buffers_method()));
    CHECK(client_ctx);
    CHECK(server_ctx);
    // Certificate compression only exists in TLS 1.3.
    CHECK(SSL_CTX_set_min_proto_version(client_ctx.get(), TLS1_3_VERSION));
    CHECK(SSL_CTX_set_min_proto_version(server_ctx.get(), TLS1_3_VERSION));

    ConfigureCertificateCompression(client_ctx.get());
    SSL_CTX_set_custom_verify(client_ctx.get(), SSL_VERIFY_PEER,
                              [](SSL*, uint8_t*) { return ssl_verify_ok; });

    std::array<CRYPTO_BUFFER*, 1> chain = {leaf().GetCertBuffer()};
    CHECK(SSL_CTX_set_chain_and_key(server_ctx.get(), chain.data(),
                                    chain.size(), leaf().GetKey(), nullptr));
    CHECK(SSL_CTX_add_cert_compression_alg(server_ctx.get(),
                                           TLSEXT_cert_compression_brotli,
                                           CompressBrotliCert, nullptr));

    bssl::UniquePtr<SSL> client(SSL_new(client_ctx.get()));
    bssl::UniquePtr<SSL> server(SSL_new(server_ctx.get()));
    CHECK(client);
    CHECK(server);
    SSL_set_connect_state(client.get());
    SSL_set_accept_state(server.get());
    SSL_set_app_data(server.get(), &server_state);

    BIO* client_bio = nullptr;
    BIO* server_bio = nullptr;
    CHECK(BIO_new_bio_pair(&client_bio, 0, &server_bio, 0));
    // Each `SSL` takes the only reference to its end of the pair.
    SSL_set_bio(client.get(), client_bio, client_bio);
    SSL_set_bio(server.get(), server_bio, server_bio);

    HandshakeResult result;
    ERR_clear_error();
    bool client_done = false;
    bool server_done = false;
    // The handshake needs two round trips, so a few iterations are enough.
    for (int i = 0; i < 10 && !(client_done && server_done); ++i) {
      if (!client_done) {
        int ret = SSL_do_handshake(client.get());
        if (ret == 1) {
          client_done = true;
        } else if (SSL_get_error(client.get(), ret) != SSL_ERROR_WANT_READ) {
          result.client_error = ERR_peek_error();
          CHECK(result.client_error);
          ERR_clear_error();
          return result;
        }
      }
      if (!server_done) {
        int ret = SSL_do_handshake(server.get());
        if (ret == 1) {
          server_done = true;
        } else {
          CHECK_EQ(SSL_get_error(server.get(), ret), SSL_ERROR_WANT_READ);
        }
      }
    }
    CHECK(client_done);
    CHECK(server_done);

    const STACK_OF(CRYPTO_BUFFER)* peer_certs =
        SSL_get0_peer_certificates(client.get());
    CHECK(peer_certs);
    CHECK_EQ(sk_CRYPTO_BUFFER_num(peer_certs), 1u);
    result.peer_cert = base::ToVector(
        x509_util::CryptoBufferAsSpan(sk_CRYPTO_BUFFER_value(peer_certs, 0)));
    return result;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // The leaf certificate and its issuer, which the leaf needs to sign itself.
  std::array<std::unique_ptr<CertBuilder>, 2> chain_ =
      CertBuilder::CreateSimpleChain2();
};

bool IsDecompressionError(uint32_t error) {
  return ERR_GET_LIB(error) == ERR_LIB_SSL &&
         ERR_GET_REASON(error) == SSL_R_CERT_DECOMPRESSION_FAILED;
}

// The values of the parameter. The Rust decoder is only tested where //net
// builds it (see `use_rust_brotli_decoder` in //net/BUILD.gn).
constexpr bool kRustBrotliInNetValues[] = {
    false,
#if BUILDFLAG(USE_RUST_BROTLI_DECODER)
    true,
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertCompressionTest,
                         testing::ValuesIn(kRustBrotliInNetValues),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RustBrotli" : "CBrotli";
                         });

TEST_P(CertCompressionTest, DecompressesCertificate) {
  ServerState server_state;
  HandshakeResult result = Handshake(server_state);
  EXPECT_EQ(result.client_error, 0u);
  EXPECT_EQ(server_state.compress_calls, 1);
  EXPECT_EQ(
      result.peer_cert,
      base::ToVector(x509_util::CryptoBufferAsSpan(leaf().GetCertBuffer())));
}

TEST_P(CertCompressionTest, RejectsTruncatedData) {
  ServerState server_state{.tamper = Tamper::kTruncate};
  EXPECT_TRUE(IsDecompressionError(Handshake(server_state).client_error));
}

TEST_P(CertCompressionTest, RejectsCertificateLongerThanAnnounced) {
  ServerState server_state{.tamper = Tamper::kAppendByte};
  EXPECT_TRUE(IsDecompressionError(Handshake(server_state).client_error));
}

TEST_P(CertCompressionTest, RejectsCertificateShorterThanAnnounced) {
  ServerState server_state{.tamper = Tamper::kDropByte};
  EXPECT_TRUE(IsDecompressionError(Handshake(server_state).client_error));
}

// RFC 8879 only allows brotli as RFC 7932 defines it.
TEST_P(CertCompressionTest, RejectsLargeWindow) {
  ServerState server_state{.tamper = Tamper::kLargeWindow};
  EXPECT_TRUE(IsDecompressionError(Handshake(server_state).client_error));
}

}  // namespace
}  // namespace net
