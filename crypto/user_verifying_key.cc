// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/user_verifying_key.h"

#include "base/check.h"

namespace crypto {

UserVerifyingSigningKey::~UserVerifyingSigningKey() = default;
UserVerifyingKeyProvider::~UserVerifyingKeyProvider() = default;

RefCountedUserVerifyingSigningKey::RefCountedUserVerifyingSigningKey(
    std::unique_ptr<crypto::UserVerifyingSigningKey> key)
    : key_(std::move(key)) {
  CHECK(key_);
}

RefCountedUserVerifyingSigningKey::~RefCountedUserVerifyingSigningKey() =
    default;

#if BUILDFLAG(IS_WIN)
std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProviderWin();
void IsKeyCredentialManagerAvailable(base::OnceCallback<void(bool)> callback);
#endif

std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProvider() {
#if BUILDFLAG(IS_WIN)
  return GetUserVerifyingKeyProviderWin();
#else
  return nullptr;
#endif
}

void AreUserVerifyingKeysSupported(base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_WIN)
  IsKeyCredentialManagerAvailable(std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

}  // namespace crypto
