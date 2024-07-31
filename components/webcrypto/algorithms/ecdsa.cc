// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/ec.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace webcrypto {

namespace {

// Extracts the OpenSSL key and digest from a WebCrypto key + algorithm. The
// returned pkey pointer will remain valid as long as |key| is alive.
Status GetPKeyAndDigest(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        EVP_PKEY** pkey,
                        const EVP_MD** digest) {
  *pkey = GetEVP_PKEY(key);
  *digest = GetDigest(algorithm.EcdsaParams()->GetHash());
  if (!*digest)
    return Status::ErrorUnsupported();
  return Status::Success();
}

// Gets the EC key's order size in bytes.
Status GetEcGroupOrderSize(EVP_PKEY* pkey, size_t* order_size_bytes) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey);
  if (!ec)
    return Status::ErrorUnexpected();

  const EC_GROUP* group = EC_KEY_get0_group(ec);

  bssl::UniquePtr<BIGNUM> order(BN_new());
  if (!EC_GROUP_get_order(group, order.get(), nullptr))
    return Status::OperationError();

  *order_size_bytes = BN_num_bytes(order.get());
  return Status::Success();
}

// Formats a DER-encoded signature (ECDSA-Sig-Value as specified in RFC 3279) to
// the signature format expected by WebCrypto (raw concatenated "r" and "s").
//
// TODO(eroman): Where is the specification for WebCrypto's signature format?
Status ConvertDerSignatureToWebCryptoSignature(
    EVP_PKEY* key,
    std::vector<uint8_t>* signature) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(signature->data(), signature->size()));
  if (!ecdsa_sig.get())
    return Status::ErrorUnexpected();

  // Determine the maximum length of r and s.
  size_t order_size_bytes;
  Status status = GetEcGroupOrderSize(key, &order_size_bytes);
  if (status.IsError())
    return status;

  signature->resize(order_size_bytes * 2);

  if (!BN_bn2bin_padded(signature->data(), order_size_bytes,
                        ecdsa_sig.get()->r)) {
    return Status::ErrorUnexpected();
  }

  if (!BN_bn2bin_padded(&(*signature)[order_size_bytes], order_size_bytes,
                        ecdsa_sig.get()->s)) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

// Formats a WebCrypto ECDSA signature to a DER-encoded signature
// (ECDSA-Sig-Value as specified in RFC 3279).
//
// TODO(eroman): What is the specification for WebCrypto's signature format?
//
// If the signature length is incorrect (not 2 * order_size), then
// Status::Success() is returned and |*incorrect_length| is set to true;
//
// Otherwise on success, der_signature is filled with a ASN.1 encoded
// ECDSA-Sig-Value.
Status ConvertWebCryptoSignatureToDerSignature(
    EVP_PKEY* key,
    base::span<const uint8_t> signature,
    std::vector<uint8_t>* der_signature,
    bool* incorrect_length) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Determine the length of r and s.
  size_t order_size_bytes;
  Status status = GetEcGroupOrderSize(key, &order_size_bytes);
  if (status.IsError())
    return status;

  // If the size of the signature is incorrect, verification must fail. Success
  // is returned here rather than an error, so that the caller can fail
  // verification with a boolean, rather than reject the promise with an
  // exception.
  if (signature.size() != 2 * order_size_bytes) {
    *incorrect_length = true;
    return Status::Success();
  }
  base::span<const uint8_t> r_bytes = signature.first(order_size_bytes);
  base::span<const uint8_t> s_bytes = signature.subspan(order_size_bytes);

  *incorrect_length = false;

  // Construct an ECDSA_SIG from |signature|.
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig)
    return Status::OperationError();

  if (!BN_bin2bn(r_bytes.data(), r_bytes.size(), ecdsa_sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), ecdsa_sig->s)) {
    return Status::ErrorUnexpected();
  }

  // Encode the signature.
  uint8_t* der;
  size_t der_len;
  if (!ECDSA_SIG_to_bytes(&der, &der_len, ecdsa_sig.get()))
    return Status::OperationError();
  der_signature->assign(der, der + der_len);
  OPENSSL_free(der);

  return Status::Success();
}

class EcdsaImplementation : public EcAlgorithm {
 public:
  EcdsaImplementation()
      : EcAlgorithm(blink::kWebCryptoKeyUsageVerify,
                    blink::kWebCryptoKeyUsageSign) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoNamedCurve curve) const override {
    switch (curve) {
      case blink::kWebCryptoNamedCurveP256:
        return "ES256";
      case blink::kWebCryptoNamedCurveP384:
        return "ES384";
      case blink::kWebCryptoNamedCurveP521:
        // This is not a typo! ES512 means P-521 with SHA-512.
        return "ES512";
      default:
        return nullptr;
    }
  }

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              base::span<const uint8_t> data,
              std::vector<uint8_t>* buffer) const override {
    if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

    EVP_PKEY* private_key = nullptr;
    const EVP_MD* digest = nullptr;
    Status status = GetPKeyAndDigest(algorithm, key, &private_key, &digest);
    if (status.IsError())
      return status;

    // NOTE: A call to EVP_DigestSign() with a NULL second parameter returns a
    // maximum allocation size, while the call without a NULL returns the real
    // one, which may be smaller.
    bssl::ScopedEVP_MD_CTX ctx;
    size_t sig_len = 0;
    if (!EVP_DigestSignInit(ctx.get(), nullptr, digest, nullptr, private_key) ||
        !EVP_DigestSign(ctx.get(), nullptr, &sig_len, data.data(),
                        data.size())) {
      return Status::OperationError();
    }

    buffer->resize(sig_len);
    if (!EVP_DigestSign(ctx.get(), buffer->data(), &sig_len, data.data(),
                        data.size())) {
      return Status::OperationError();
    }
    buffer->resize(sig_len);

    // ECDSA signing in BoringSSL outputs a DER-encoded (r,s). WebCrypto however
    // expects a padded bitstring that is r concatenated to s. Convert to the
    // expected format.
    return ConvertDerSignatureToWebCryptoSignature(private_key, buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                base::span<const uint8_t> signature,
                base::span<const uint8_t> data,
                bool* signature_match) const override {
    if (key.GetType() != blink::kWebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

    EVP_PKEY* public_key = nullptr;
    const EVP_MD* digest = nullptr;
    Status status = GetPKeyAndDigest(algorithm, key, &public_key, &digest);
    if (status.IsError())
      return status;

    std::vector<uint8_t> der_signature;
    bool incorrect_length_signature = false;
    status = ConvertWebCryptoSignatureToDerSignature(
        public_key, signature, &der_signature, &incorrect_length_signature);
    if (status.IsError())
      return status;

    if (incorrect_length_signature) {
      *signature_match = false;
      return Status::Success();
    }

    bssl::ScopedEVP_MD_CTX ctx;
    if (!EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr,
                              public_key)) {
      return Status::OperationError();
    }

    *signature_match =
        1 == EVP_DigestVerify(ctx.get(), der_signature.data(),
                              der_signature.size(), data.data(), data.size());
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateEcdsaImplementation() {
  return std::make_unique<EcdsaImplementation>();
}

}  // namespace webcrypto
