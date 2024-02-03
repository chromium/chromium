// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/user_verifying_key.h"

#include "base/check.h"
#include "base/functional/bind.h"

namespace crypto {

UserVerifyingSigningKey::~UserVerifyingSigningKey() = default;
UserVerifyingKeyProvider::~UserVerifyingKeyProvider() = default;

#if BUILDFLAG(IS_WIN)
std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProviderWin();
#endif

std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProvider() {
#if BUILDFLAG(IS_WIN)
  return GetUserVerifyingKeyProviderWin();
#else
  return nullptr;
#endif
}

}  // namespace crypto
