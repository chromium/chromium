// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/rsa_public_key.h"

#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "components/cbor/writer.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device {

// static
std::unique_ptr<PublicKey> RSAPublicKey::ExtractFromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  // See https://tools.ietf.org/html/rfc8230#section-4
  struct COSEKey {
    // All the fields below are not a raw_ptr<,,,>, because ELEMENT() treats the
    // raw_ptr<T> as a void*, skipping AddRef() call and causing a ref-counting
    // mismatch.
    RAW_PTR_EXCLUSION const int64_t* kty;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* n;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* e;
  } cose_key;

  static constexpr cbor_extract::StepOrByte<COSEKey> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, COSEKey, kty),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kKty)),

      ELEMENT(Is::kRequired, COSEKey, n),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kRSAModulus)),

      ELEMENT(Is::kRequired, COSEKey, e),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kRSAPublicExponent)),

      Stop<COSEKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<COSEKey>(&cose_key, kSteps, map) ||
      *cose_key.kty != static_cast<int64_t>(CoseKeyTypes::kRSA)) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> n_bn(BN_new());
  bssl::UniquePtr<BIGNUM> e_bn(BN_new());
  if (!BN_bin2bn(cose_key.n->data(), cose_key.n->size(), n_bn.get()) ||
      !BN_bin2bn(cose_key.e->data(), cose_key.e->size(), e_bn.get())) {
    return nullptr;
  }

  bssl::UniquePtr<RSA> rsa(RSA_new());
  if (!RSA_set0_key(rsa.get(), n_bn.release(), e_bn.release(), /*d=*/nullptr)) {
    return nullptr;
  }

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_assign_RSA(pkey.get(), rsa.release()));

  bssl::ScopedCBB cbb;
  uint8_t* der_bytes = nullptr;
  size_t der_bytes_len = 0;
  CHECK(CBB_init(cbb.get(), /* initial size */ 128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()) &&
        CBB_finish(cbb.get(), &der_bytes, &der_bytes_len));

  std::vector<uint8_t> der_bytes_vec(der_bytes, der_bytes + der_bytes_len);
  OPENSSL_free(der_bytes);

  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     std::move(der_bytes_vec));
}

}  // namespace device
