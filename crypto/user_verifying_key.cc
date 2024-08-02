// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/user_verifying_key.h"

#include "base/check.h"

namespace crypto {

namespace {

std::unique_ptr<UserVerifyingKeyProvider> (*g_mock_provider)() = nullptr;

}  // namespace

UserVerifyingKeyProvider::Config::Config() = default;
UserVerifyingKeyProvider::Config::Config(Config&& config) = default;
UserVerifyingKeyProvider::Config& UserVerifyingKeyProvider::Config::operator=(
    Config&& config) = default;
UserVerifyingKeyProvider::Config::~Config() = default;

UserVerifyingSigningKey::~UserVerifyingSigningKey() = default;

bool UserVerifyingSigningKey::IsHardwareBacked() const {
  return false;
}

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

#if BUILDFLAG(IS_MAC)
std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProviderMac(
    UserVerifyingKeyProvider::Config config);
void AreMacUnexportableKeysAvailable(UserVerifyingKeyProvider::Config config,
                                     base::OnceCallback<void(bool)> callback);
#endif

std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProvider(
    UserVerifyingKeyProvider::Config config) {
  if (g_mock_provider) {
    return g_mock_provider();
  }
#if BUILDFLAG(IS_WIN)
  return GetUserVerifyingKeyProviderWin();
#elif BUILDFLAG(IS_MAC)
  return GetUserVerifyingKeyProviderMac(std::move(config));
#else
  return nullptr;
#endif
}

void AreUserVerifyingKeysSupported(UserVerifyingKeyProvider::Config config,
                                   base::OnceCallback<void(bool)> callback) {
  if (g_mock_provider) {
    std::move(callback).Run(g_mock_provider() != nullptr);
    return;
  }
#if BUILDFLAG(IS_WIN)
  IsKeyCredentialManagerAvailable(std::move(callback));
#elif BUILDFLAG(IS_MAC)
  AreMacUnexportableKeysAvailable(std::move(config), std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

namespace internal {

void SetUserVerifyingKeyProviderForTesting(
    std::unique_ptr<UserVerifyingKeyProvider> (*func)()) {
  if (g_mock_provider) {
    // Prevent nesting of scoped providers.
    CHECK(!func);
    g_mock_provider = nullptr;
  } else {
    g_mock_provider = func;
  }
}

}  // namespace internal

}  // namespace crypto
