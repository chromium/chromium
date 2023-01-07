// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/blink_key_handle.h"

#include <utility>

#include "base/check_op.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {

class SymKey;
class AsymKey;

// Base class for wrapping OpenSSL keys in a type that can be passed to
// Blink (blink::WebCryptoKeyHandle).
class Key : public blink::WebCryptoKeyHandle {
 public:
  // Helpers to add some safety to casting.
  virtual SymKey* AsSymKey() { return nullptr; }
  virtual AsymKey* AsAsymKey() { return nullptr; }
};

class SymKey : public Key {
 public:
  explicit SymKey(base::span<const uint8_t> raw_key_data)
      : raw_key_data_(raw_key_data.begin(), raw_key_data.end()) {}

  SymKey(const SymKey&) = delete;
  SymKey& operator=(const SymKey&) = delete;

  SymKey* AsSymKey() override { return this; }

  const std::vector<uint8_t>& raw_key_data() const { return raw_key_data_; }

 private:
  std::vector<uint8_t> raw_key_data_;
};

class AsymKey : public Key {
 public:
  // After construction the |pkey| should NOT be mutated.
  explicit AsymKey(bssl::UniquePtr<EVP_PKEY> pkey) : pkey_(std::move(pkey)) {}

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

blink::WebCryptoKeyHandle* CreateSymmetricKeyHandle(
    base::span<const uint8_t> key_bytes) {
  return new SymKey(key_bytes);
}

blink::WebCryptoKeyHandle* CreateAsymmetricKeyHandle(
    bssl::UniquePtr<EVP_PKEY> pkey) {
  return new AsymKey(std::move(pkey));
}

}  // namespace webcrypto
