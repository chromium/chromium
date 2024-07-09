/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/platform/web_crypto_algorithm.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

// A mapping from the algorithm ID to information about the algorithm.
constexpr WebCryptoAlgorithmInfo kAlgorithmIdToInfo[] = {
    {// Index 0
     "AES-CBC",
     {
         kWebCryptoAlgorithmParamsTypeAesCbcParams,         // Encrypt
         kWebCryptoAlgorithmParamsTypeAesCbcParams,         // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeAesKeyGenParams,      // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,                 // ImportKey
         kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                // DeriveBits
         kWebCryptoAlgorithmParamsTypeAesCbcParams,         // WrapKey
         kWebCryptoAlgorithmParamsTypeAesCbcParams          // UnwrapKey
     }},
    {// Index 1
     "HMAC",
     {
         WebCryptoAlgorithmInfo::kUndefined,             // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,             // Decrypt
         kWebCryptoAlgorithmParamsTypeNone,              // Sign
         kWebCryptoAlgorithmParamsTypeNone,              // Verify
         WebCryptoAlgorithmInfo::kUndefined,             // Digest
         kWebCryptoAlgorithmParamsTypeHmacKeyGenParams,  // GenerateKey
         kWebCryptoAlgorithmParamsTypeHmacImportParams,  // ImportKey
         kWebCryptoAlgorithmParamsTypeHmacImportParams,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,             // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,             // WrapKey
         WebCryptoAlgorithmInfo::kUndefined              // UnwrapKey
     }},
    {// Index 2
     "RSASSA-PKCS1-v1_5",
     {
         WebCryptoAlgorithmInfo::kUndefined,                  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,                  // Decrypt
         kWebCryptoAlgorithmParamsTypeNone,                   // Sign
         kWebCryptoAlgorithmParamsTypeNone,                   // Verify
         WebCryptoAlgorithmInfo::kUndefined,                  // Digest
         kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams,  // GenerateKey
         kWebCryptoAlgorithmParamsTypeRsaHashedImportParams,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,                  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,                  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined                   // UnwrapKey
     }},
    {// Index 3
     "SHA-1",
     {
         WebCryptoAlgorithmInfo::kUndefined,  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Sign
         WebCryptoAlgorithmInfo::kUndefined,  // Verify
         kWebCryptoAlgorithmParamsTypeNone,   // Digest
         WebCryptoAlgorithmInfo::kUndefined,  // GenerateKey
         WebCryptoAlgorithmInfo::kUndefined,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined   // UnwrapKey
     }},
    {// Index 4
     "SHA-256",
     {
         WebCryptoAlgorithmInfo::kUndefined,  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Sign
         WebCryptoAlgorithmInfo::kUndefined,  // Verify
         kWebCryptoAlgorithmParamsTypeNone,   // Digest
         WebCryptoAlgorithmInfo::kUndefined,  // GenerateKey
         WebCryptoAlgorithmInfo::kUndefined,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined   // UnwrapKey
     }},
    {// Index 5
     "SHA-384",
     {
         WebCryptoAlgorithmInfo::kUndefined,  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Sign
         WebCryptoAlgorithmInfo::kUndefined,  // Verify
         kWebCryptoAlgorithmParamsTypeNone,   // Digest
         WebCryptoAlgorithmInfo::kUndefined,  // GenerateKey
         WebCryptoAlgorithmInfo::kUndefined,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined   // UnwrapKey
     }},
    {// Index 6
     "SHA-512",
     {
         WebCryptoAlgorithmInfo::kUndefined,  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Sign
         WebCryptoAlgorithmInfo::kUndefined,  // Verify
         kWebCryptoAlgorithmParamsTypeNone,   // Digest
         WebCryptoAlgorithmInfo::kUndefined,  // GenerateKey
         WebCryptoAlgorithmInfo::kUndefined,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined   // UnwrapKey
     }},
    {// Index 7
     "AES-GCM",
     {
         kWebCryptoAlgorithmParamsTypeAesGcmParams,         // Encrypt
         kWebCryptoAlgorithmParamsTypeAesGcmParams,         // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeAesKeyGenParams,      // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,                 // ImportKey
         kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                // DeriveBits
         kWebCryptoAlgorithmParamsTypeAesGcmParams,         // WrapKey
         kWebCryptoAlgorithmParamsTypeAesGcmParams          // UnwrapKey
     }},
    {// Index 8
     "RSA-OAEP",
     {
         kWebCryptoAlgorithmParamsTypeRsaOaepParams,          // Encrypt
         kWebCryptoAlgorithmParamsTypeRsaOaepParams,          // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                  // Sign
         WebCryptoAlgorithmInfo::kUndefined,                  // Verify
         WebCryptoAlgorithmInfo::kUndefined,                  // Digest
         kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams,  // GenerateKey
         kWebCryptoAlgorithmParamsTypeRsaHashedImportParams,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,                  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                  // DeriveBits
         kWebCryptoAlgorithmParamsTypeRsaOaepParams,          // WrapKey
         kWebCryptoAlgorithmParamsTypeRsaOaepParams           // UnwrapKey
     }},
    {// Index 9
     "AES-CTR",
     {
         kWebCryptoAlgorithmParamsTypeAesCtrParams,         // Encrypt
         kWebCryptoAlgorithmParamsTypeAesCtrParams,         // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeAesKeyGenParams,      // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,                 // ImportKey
         kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                // DeriveBits
         kWebCryptoAlgorithmParamsTypeAesCtrParams,         // WrapKey
         kWebCryptoAlgorithmParamsTypeAesCtrParams          // UnwrapKey
     }},
    {// Index 10
     "AES-KW",
     {
         WebCryptoAlgorithmInfo::kUndefined,                // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeAesKeyGenParams,      // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,                 // ImportKey
         kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                // DeriveBits
         kWebCryptoAlgorithmParamsTypeNone,                 // WrapKey
         kWebCryptoAlgorithmParamsTypeNone                  // UnwrapKey
     }},
    {// Index 11
     "RSA-PSS",
     {
         WebCryptoAlgorithmInfo::kUndefined,                  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,                  // Decrypt
         kWebCryptoAlgorithmParamsTypeRsaPssParams,           // Sign
         kWebCryptoAlgorithmParamsTypeRsaPssParams,           // Verify
         WebCryptoAlgorithmInfo::kUndefined,                  // Digest
         kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams,  // GenerateKey
         kWebCryptoAlgorithmParamsTypeRsaHashedImportParams,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,                  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,                  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,                  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined                   // UnwrapKey
     }},
    {// Index 12
     "ECDSA",
     {
         WebCryptoAlgorithmInfo::kUndefined,              // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,              // Decrypt
         kWebCryptoAlgorithmParamsTypeEcdsaParams,        // Sign
         kWebCryptoAlgorithmParamsTypeEcdsaParams,        // Verify
         WebCryptoAlgorithmInfo::kUndefined,              // Digest
         kWebCryptoAlgorithmParamsTypeEcKeyGenParams,     // GenerateKey
         kWebCryptoAlgorithmParamsTypeEcKeyImportParams,  // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,              // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,              // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,              // WrapKey
         WebCryptoAlgorithmInfo::kUndefined               // UnwrapKey
     }},
    {// Index 13
     "ECDH",
     {
         WebCryptoAlgorithmInfo::kUndefined,                // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeEcKeyGenParams,       // GenerateKey
         kWebCryptoAlgorithmParamsTypeEcKeyImportParams,    // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,                // GetKeyLength
         kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,                // WrapKey
         WebCryptoAlgorithmInfo::kUndefined                 // UnwrapKey
     }},
    {// Index 14
     "HKDF",
     {
         WebCryptoAlgorithmInfo::kUndefined,       // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,       // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,       // Sign
         WebCryptoAlgorithmInfo::kUndefined,       // Verify
         WebCryptoAlgorithmInfo::kUndefined,       // Digest
         WebCryptoAlgorithmInfo::kUndefined,       // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,        // ImportKey
         kWebCryptoAlgorithmParamsTypeNone,        // GetKeyLength
         kWebCryptoAlgorithmParamsTypeHkdfParams,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,       // WrapKey
         WebCryptoAlgorithmInfo::kUndefined        // UnwrapKey
     }},
    {// Index 15
     "PBKDF2",
     {
         WebCryptoAlgorithmInfo::kUndefined,         // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,         // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,         // Sign
         WebCryptoAlgorithmInfo::kUndefined,         // Verify
         WebCryptoAlgorithmInfo::kUndefined,         // Digest
         WebCryptoAlgorithmInfo::kUndefined,         // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,          // ImportKey
         kWebCryptoAlgorithmParamsTypeNone,          // GetKeyLength
         kWebCryptoAlgorithmParamsTypePbkdf2Params,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,         // WrapKey
         WebCryptoAlgorithmInfo::kUndefined          // UnwrapKey
     }},
    {// Index 16
     // TODO(crbug.com/1370697): Ed25519 is experimental behind a flag. See
     // https://chromestatus.com/feature/4913922408710144 for the status.
     "Ed25519",
     {
         WebCryptoAlgorithmInfo::kUndefined,  // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,  // Decrypt
         kWebCryptoAlgorithmParamsTypeNone,   // Sign
         kWebCryptoAlgorithmParamsTypeNone,   // Verify
         WebCryptoAlgorithmInfo::kUndefined,  // Digest
         kWebCryptoAlgorithmParamsTypeNone,   // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,   // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,  // GetKeyLength
         WebCryptoAlgorithmInfo::kUndefined,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,  // WrapKey
         WebCryptoAlgorithmInfo::kUndefined   // UnwrapKey
     }},
    {// Index 17
     // TODO(crbug.com/1370697): X25519 is experimental behind a flag. See
     // https://chromestatus.com/feature/4913922408710144 for the status.
     "X25519",
     {
         WebCryptoAlgorithmInfo::kUndefined,                // Encrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Decrypt
         WebCryptoAlgorithmInfo::kUndefined,                // Sign
         WebCryptoAlgorithmInfo::kUndefined,                // Verify
         WebCryptoAlgorithmInfo::kUndefined,                // Digest
         kWebCryptoAlgorithmParamsTypeNone,                 // GenerateKey
         kWebCryptoAlgorithmParamsTypeNone,                 // ImportKey
         WebCryptoAlgorithmInfo::kUndefined,                // GetKeyLength
         kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams,  // DeriveBits
         WebCryptoAlgorithmInfo::kUndefined,                // WrapKey
         WebCryptoAlgorithmInfo::kUndefined                 // UnwrapKey
     }},
};

// Initializing the algorithmIdToInfo table above depends on knowing the enum
// values for algorithm IDs. If those ever change, the table will need to be
// updated.
static_assert(kWebCryptoAlgorithmIdAesCbc == 0, "AES CBC id must match");
static_assert(kWebCryptoAlgorithmIdHmac == 1, "HMAC id must match");
static_assert(kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5 == 2,
              "RSASSA-PKCS1-v1_5 id must match");
static_assert(kWebCryptoAlgorithmIdSha1 == 3, "SHA1 id must match");
static_assert(kWebCryptoAlgorithmIdSha256 == 4, "SHA256 id must match");
static_assert(kWebCryptoAlgorithmIdSha384 == 5, "SHA384 id must match");
static_assert(kWebCryptoAlgorithmIdSha512 == 6, "SHA512 id must match");
static_assert(kWebCryptoAlgorithmIdAesGcm == 7, "AES GCM id must match");
static_assert(kWebCryptoAlgorithmIdRsaOaep == 8, "RSA OAEP id must match");
static_assert(kWebCryptoAlgorithmIdAesCtr == 9, "AES CTR id must match");
static_assert(kWebCryptoAlgorithmIdAesKw == 10, "AESKW id must match");
static_assert(kWebCryptoAlgorithmIdRsaPss == 11, "RSA-PSS id must match");
static_assert(kWebCryptoAlgorithmIdEcdsa == 12, "ECDSA id must match");
static_assert(kWebCryptoAlgorithmIdEcdh == 13, "ECDH id must match");
static_assert(kWebCryptoAlgorithmIdHkdf == 14, "HKDF id must match");
static_assert(kWebCryptoAlgorithmIdPbkdf2 == 15, "Pbkdf2 id must match");
static_assert(kWebCryptoAlgorithmIdEd25519 == 16, "Ed25519 id must match");
static_assert(kWebCryptoAlgorithmIdX25519 == 17, "X25519 id must match");
static_assert(kWebCryptoAlgorithmIdLast == 17, "last id must match");
static_assert(10 == kWebCryptoOperationLast,
              "the parameter mapping needs to be updated");

}  // namespace

class WebCryptoAlgorithmPrivate
    : public ThreadSafeRefCounted<WebCryptoAlgorithmPrivate> {
 public:
  WebCryptoAlgorithmPrivate(WebCryptoAlgorithmId id,
                            std::unique_ptr<WebCryptoAlgorithmParams> params)
      : id(id), params(std::move(params)) {}

  WebCryptoAlgorithmId id;
  std::unique_ptr<WebCryptoAlgorithmParams> params;
};

WebCryptoAlgorithm::WebCryptoAlgorithm(
    WebCryptoAlgorithmId id,
    std::unique_ptr<WebCryptoAlgorithmParams> params)
    : private_(base::AdoptRef(
          new WebCryptoAlgorithmPrivate(id, std::move(params)))) {}

WebCryptoAlgorithm WebCryptoAlgorithm::CreateNull() {
  return WebCryptoAlgorithm();
}

WebCryptoAlgorithm WebCryptoAlgorithm::AdoptParamsAndCreate(
    WebCryptoAlgorithmId id,
    WebCryptoAlgorithmParams* params) {
  return WebCryptoAlgorithm(id, base::WrapUnique(params));
}

const WebCryptoAlgorithmInfo* WebCryptoAlgorithm::LookupAlgorithmInfo(
    WebCryptoAlgorithmId id) {
  const unsigned id_int = id;
  if (id_int >= std::size(kAlgorithmIdToInfo))
    return nullptr;
  return &kAlgorithmIdToInfo[id];
}

bool WebCryptoAlgorithm::IsNull() const {
  return private_.IsNull();
}

WebCryptoAlgorithmId WebCryptoAlgorithm::Id() const {
  DCHECK(!IsNull());
  return private_->id;
}

WebCryptoAlgorithmParamsType WebCryptoAlgorithm::ParamsType() const {
  DCHECK(!IsNull());
  if (!private_->params)
    return kWebCryptoAlgorithmParamsTypeNone;
  return private_->params->GetType();
}

const WebCryptoAesCbcParams* WebCryptoAlgorithm::AesCbcParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeAesCbcParams)
    return static_cast<WebCryptoAesCbcParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoAesCtrParams* WebCryptoAlgorithm::AesCtrParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeAesCtrParams)
    return static_cast<WebCryptoAesCtrParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoAesKeyGenParams* WebCryptoAlgorithm::AesKeyGenParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeAesKeyGenParams)
    return static_cast<WebCryptoAesKeyGenParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoHmacImportParams* WebCryptoAlgorithm::HmacImportParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeHmacImportParams)
    return static_cast<WebCryptoHmacImportParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoHmacKeyGenParams* WebCryptoAlgorithm::HmacKeyGenParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeHmacKeyGenParams)
    return static_cast<WebCryptoHmacKeyGenParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoAesGcmParams* WebCryptoAlgorithm::AesGcmParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeAesGcmParams)
    return static_cast<WebCryptoAesGcmParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoRsaOaepParams* WebCryptoAlgorithm::RsaOaepParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeRsaOaepParams)
    return static_cast<WebCryptoRsaOaepParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoRsaHashedImportParams*
WebCryptoAlgorithm::RsaHashedImportParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeRsaHashedImportParams)
    return static_cast<WebCryptoRsaHashedImportParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoRsaHashedKeyGenParams*
WebCryptoAlgorithm::RsaHashedKeyGenParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams)
    return static_cast<WebCryptoRsaHashedKeyGenParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoRsaPssParams* WebCryptoAlgorithm::RsaPssParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeRsaPssParams)
    return static_cast<WebCryptoRsaPssParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoEcdsaParams* WebCryptoAlgorithm::EcdsaParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeEcdsaParams)
    return static_cast<WebCryptoEcdsaParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoEcKeyGenParams* WebCryptoAlgorithm::EcKeyGenParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeEcKeyGenParams)
    return static_cast<WebCryptoEcKeyGenParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoEcKeyImportParams* WebCryptoAlgorithm::EcKeyImportParams()
    const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeEcKeyImportParams)
    return static_cast<WebCryptoEcKeyImportParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoEcdhKeyDeriveParams* WebCryptoAlgorithm::EcdhKeyDeriveParams()
    const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams)
    return static_cast<WebCryptoEcdhKeyDeriveParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoAesDerivedKeyParams* WebCryptoAlgorithm::AesDerivedKeyParams()
    const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams)
    return static_cast<WebCryptoAesDerivedKeyParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoHkdfParams* WebCryptoAlgorithm::HkdfParams() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypeHkdfParams)
    return static_cast<WebCryptoHkdfParams*>(private_->params.get());
  return nullptr;
}

const WebCryptoPbkdf2Params* WebCryptoAlgorithm::Pbkdf2Params() const {
  DCHECK(!IsNull());
  if (ParamsType() == kWebCryptoAlgorithmParamsTypePbkdf2Params)
    return static_cast<WebCryptoPbkdf2Params*>(private_->params.get());
  return nullptr;
}

bool WebCryptoAlgorithm::IsHash(WebCryptoAlgorithmId id) {
  switch (id) {
    case kWebCryptoAlgorithmIdSha1:
    case kWebCryptoAlgorithmIdSha256:
    case kWebCryptoAlgorithmIdSha384:
    case kWebCryptoAlgorithmIdSha512:
      return true;
    case kWebCryptoAlgorithmIdAesCbc:
    case kWebCryptoAlgorithmIdHmac:
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
    case kWebCryptoAlgorithmIdAesGcm:
    case kWebCryptoAlgorithmIdRsaOaep:
    case kWebCryptoAlgorithmIdAesCtr:
    case kWebCryptoAlgorithmIdAesKw:
    case kWebCryptoAlgorithmIdRsaPss:
    case kWebCryptoAlgorithmIdEcdsa:
    case kWebCryptoAlgorithmIdEcdh:
    case kWebCryptoAlgorithmIdHkdf:
    case kWebCryptoAlgorithmIdPbkdf2:
    case kWebCryptoAlgorithmIdEd25519:
    case kWebCryptoAlgorithmIdX25519:
      break;
  }
  return false;
}

bool WebCryptoAlgorithm::IsKdf(WebCryptoAlgorithmId id) {
  switch (id) {
    case kWebCryptoAlgorithmIdHkdf:
    case kWebCryptoAlgorithmIdPbkdf2:
      return true;
    case kWebCryptoAlgorithmIdSha1:
    case kWebCryptoAlgorithmIdSha256:
    case kWebCryptoAlgorithmIdSha384:
    case kWebCryptoAlgorithmIdSha512:
    case kWebCryptoAlgorithmIdAesCbc:
    case kWebCryptoAlgorithmIdHmac:
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
    case kWebCryptoAlgorithmIdAesGcm:
    case kWebCryptoAlgorithmIdRsaOaep:
    case kWebCryptoAlgorithmIdAesCtr:
    case kWebCryptoAlgorithmIdAesKw:
    case kWebCryptoAlgorithmIdRsaPss:
    case kWebCryptoAlgorithmIdEcdsa:
    case kWebCryptoAlgorithmIdEcdh:
    case kWebCryptoAlgorithmIdEd25519:
    case kWebCryptoAlgorithmIdX25519:
      break;
  }
  return false;
}

void WebCryptoAlgorithm::Assign(const WebCryptoAlgorithm& other) {
  private_ = other.private_;
}

void WebCryptoAlgorithm::Reset() {
  private_.Reset();
}

}  // namespace blink
