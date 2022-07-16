// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/blink_key_handle.h"

#include <utility>

#include "base/check_op.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {

class SymKey;
class AsymKey;

// Base class for wrapping OpenSSL keys in a type that can be passed to
// Blink (blink::WebCryptoKeyHandle).
//
// In addition to the key's internal OpenSSL representation (EVP_PKEY or just
// raw bytes), each key maintains a copy of its serialized form in either
// 'raw', 'pkcs8', or 'spki' format. This is to allow structured cloning of
// keys to be done synchronously from the target Blink thread, without having to
// lock access to the key throughout the code.
//
// TODO(eroman): Should be able to do the key export needed for structured
//               clone synchronously.
class Key : public blink::WebCryptoKeyHandle {
 public:
  explicit Key(const CryptoData& serialized_key_data)
      : serialized_key_data_(
            serialized_key_data.bytes(),
            serialized_key_data.bytes() + serialized_key_data.byte_length()) {}

  ~Key() override {}

  // Helpers to add some safety to casting.
  virtual SymKey* AsSymKey() { return nullptr; }
  virtual AsymKey* AsAsymKey() { return nullptr; }

  const std::vector<uint8_t>& serialized_key_data() const {
    return serialized_key_data_;
  }

 private:
  const std::vector<uint8_t> serialized_key_data_;
};

class SymKey : public Key {
 public:
  explicit SymKey(const CryptoData& raw_key_data) : Key(raw_key_data) {}

  SymKey(const SymKey&) = delete;
  SymKey& operator=(const SymKey&) = delete;

  SymKey* AsSymKey() override { return this; }

  const std::vector<uint8_t>& raw_key_data() const {
    return serialized_key_data();
  }
};

class AsymKey : public Key {
 public:
  // After construction the |pkey| should NOT be mutated.
  AsymKey(bssl::UniquePtr<EVP_PKEY> pkey,
          const std::vector<uint8_t>& serialized_key_data)
      : Key(CryptoData(serialized_key_data)), pkey_(std::move(pkey)) {}

  AsymKey(const AsymKey&) = delete;
  AsymKey& operator=(const AsymKey&) = delete;

  AsymKey* AsAsymKey() override { return this; }

  // The caller should NOT mutate this EVP_PKEY.
  EVP_PKEY* pkey() { return pkey_.get(); }

 private:
  bssl::UniquePtr<EVP_PKEY> pkey_;
};

Key* GetKey(const blink::WebCryptoKey& key) {
  return static_cast<Key*>(key.Handle());
}

}  // namespace

const std::vector<uint8_t>& GetSymmetricKeyData(
    const blink::WebCryptoKey& key) {
  DCHECK_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
  return GetKey(key)->AsSymKey()->raw_key_data();
}

EVP_PKEY* GetEVP_PKEY(const blink::WebCryptoKey& key) {
  DCHECK_NE(blink::kWebCryptoKeyTypeSecret, key.GetType());
  return GetKey(key)->AsAsymKey()->pkey();
}

const std::vector<uint8_t>& GetSerializedKeyData(
    const blink::WebCryptoKey& key) {
  return GetKey(key)->serialized_key_data();
}

blink::WebCryptoKeyHandle* CreateSymmetricKeyHandle(
    const CryptoData& key_bytes) {
  return new SymKey(key_bytes);
}

blink::WebCryptoKeyHandle* CreateAsymmetricKeyHandle(
    bssl::UniquePtr<EVP_PKEY> pkey,
    const std::vector<uint8_t>& serialized_key_data) {
  return new AsymKey(std::move(pkey), serialized_key_data);
}

}  // namespace webcrypto
