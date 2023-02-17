// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_private_key_test_util.h"

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using net::test::IsOk;

namespace net {

namespace {

bool VerifyWithOpenSSL(uint16_t algorithm,
                       base::span<const uint8_t> input,
                       EVP_PKEY* key,
                       base::span<const uint8_t> signature) {
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pctx;
  if (!EVP_DigestVerifyInit(ctx.get(), &pctx,
                            SSL_get_signature_algorithm_digest(algorithm),
                            nullptr, key)) {
    return false;
  }
  if (SSL_is_signature_algorithm_rsa_pss(algorithm)) {
    if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1 /* hash length */)) {
      return false;
    }
  }
  return EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                          input.data(), input.size());
}

void OnSignComplete(base::RunLoop* loop,
                    Error* out_error,
                    std::vector<uint8_t>* out_signature,
                    Error error,
                    const std::vector<uint8_t>& signature) {
  *out_error = error;
  *out_signature = signature;
  loop->Quit();
}

Error DoKeySigningWithWrapper(SSLPrivateKey* key,
                              uint16_t algorithm,
                              base::span<const uint8_t> input,
                              std::vector<uint8_t>* result) {
  Error error;
  base::RunLoop loop;
  key->Sign(algorithm, input,
            base::BindOnce(OnSignComplete, base::Unretained(&loop),
                           base::Unretained(&error), base::Unretained(result)));
  loop.Run();
  return error;
}

}  // namespace

void TestSSLPrivateKeyMatches(SSLPrivateKey* key, const std::string& pkcs8) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Create the equivalent OpenSSL key.
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> openssl_key(EVP_parse_private_key(&cbs));
  ASSERT_TRUE(openssl_key);
  EXPECT_EQ(0u, CBS_len(&cbs));

  // Test all supported algorithms.
  std::vector<uint16_t> preferences = key->GetAlgorithmPreferences();

  for (uint16_t algorithm : preferences) {
    SCOPED_TRACE(
        SSL_get_signature_algorithm_name(algorithm, 0 /* exclude curve */));
    // BoringSSL will skip signatures algorithms that don't match the key type.
    if (EVP_PKEY_id(openssl_key.get()) !=
        SSL_get_signature_algorithm_key_type(algorithm)) {
      continue;
    }
    // If the RSA key is too small for the hash, skip the algorithm. BoringSSL
    // will filter this algorithm out and decline using it. In particular,
    // 1024-bit RSA keys cannot sign RSA-PSS with SHA-512 and test keys are
    // often 1024 bits.
    if (SSL_is_signature_algorithm_rsa_pss(algorithm) &&
        static_cast<size_t>(EVP_PKEY_size(openssl_key.get())) <
            2 * EVP_MD_size(SSL_get_signature_algorithm_digest(algorithm)) +
                2) {
      continue;
    }

    // Test the key generates valid signatures.
    std::vector<uint8_t> input(100, 'a');
    std::vector<uint8_t> signature;
    Error error = DoKeySigningWithWrapper(key, algorithm, input, &signature);
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(
        VerifyWithOpenSSL(algorithm, input, openssl_key.get(), signature));
  }
}

}  // namespace net
