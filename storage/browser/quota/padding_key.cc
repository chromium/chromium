// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/padding_key.h"

#include <cstdint>
#include <vector>

#include "base/no_destructor.h"
#include "crypto/hmac.h"

using crypto::SymmetricKey;

namespace storage {

namespace {

const SymmetricKey::Algorithm kPaddingKeyAlgorithm = SymmetricKey::AES;

// The range of the padding added to response sizes for opaque resources.
// Increment the CacheStorage padding version if changed.
constexpr uint64_t kPaddingRange = 14431 * 1024;

std::unique_ptr<SymmetricKey>* GetPaddingKeyInternal() {
  static base::NoDestructor<std::unique_ptr<SymmetricKey>> s_padding_key([] {
    return SymmetricKey::GenerateRandomKey(kPaddingKeyAlgorithm, 128);
  }());
  return s_padding_key.get();
}

}  // namespace

const SymmetricKey* GetDefaultPaddingKey() {
  return GetPaddingKeyInternal()->get();
}

std::unique_ptr<SymmetricKey> CopyDefaultPaddingKey() {
  return SymmetricKey::Import(kPaddingKeyAlgorithm,
                              (*GetPaddingKeyInternal())->key());
}

std::unique_ptr<SymmetricKey> DeserializePaddingKey(
    const std::string& raw_key) {
  return SymmetricKey::Import(kPaddingKeyAlgorithm, raw_key);
}

std::string SerializeDefaultPaddingKey() {
  return (*GetPaddingKeyInternal())->key();
}

void ResetPaddingKeyForTesting() {
  *GetPaddingKeyInternal() =
      SymmetricKey::GenerateRandomKey(kPaddingKeyAlgorithm, 128);
}

int64_t ComputeResponsePadding(const std::string& response_url,
                               const crypto::SymmetricKey* padding_key,
                               bool has_metadata) {
  DCHECK(!response_url.empty());

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(padding_key));

  std::string key = has_metadata ? response_url + "METADATA" : response_url;
  uint64_t digest_start;
  CHECK(hmac.Sign(key, reinterpret_cast<uint8_t*>(&digest_start),
                  sizeof(digest_start)));
  return digest_start % kPaddingRange;
}

}  // namespace storage
