// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/ed25519_public_key.h"

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
std::unique_ptr<PublicKey> Ed25519PublicKey::ExtractFromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  // See https://tools.ietf.org/html/rfc8152#section-13.2
  struct COSEKey {
    // All the fields below are not a raw_ptr<,,,>, because ELEMENT() treats the
    // raw_ptr<T> as a void*, skipping AddRef() call and causing a ref-counting
    // mismatch.
    RAW_PTR_EXCLUSION const int64_t* kty;
    RAW_PTR_EXCLUSION const int64_t* crv;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* key;
  } cose_key;

  static constexpr cbor_extract::StepOrByte<COSEKey> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, COSEKey, kty),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kKty)),

      ELEMENT(Is::kRequired, COSEKey, crv),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kEllipticCurve)),

      ELEMENT(Is::kRequired, COSEKey, key),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kEllipticX)),

      Stop<COSEKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<COSEKey>(&cose_key, kSteps, map) ||
      *cose_key.kty != static_cast<int64_t>(CoseKeyTypes::kOKP) ||
      *cose_key.crv != static_cast<int64_t>(CoseCurves::kEd25519) ||
      cose_key.key->size() != 32) {
    return nullptr;
  }

  // The COSE RFC says that "This contains the x-coordinate for the EC point".
  // The RFC authors do not appear to understand what's going on because it
  // actually just contains the Ed25519 public key, which you would expect, and
  // which also encodes the y-coordinate as a sign bit.
  //
  // We could attempt to check whether |key| contains a quadratic residue, as it
  // should. But that would involve diving into the guts of Ed25519 too much.

  bssl::UniquePtr<EVP_PKEY> pkey(
      EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, /*engine=*/nullptr,
                                  cose_key.key->data(), cose_key.key->size()));
  if (!pkey) {
    return nullptr;
  }

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
