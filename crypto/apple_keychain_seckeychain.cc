// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain_seckeychain.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/types/expected.h"
#include "crypto/mac_security_services_lock.h"

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/40233280 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace crypto {

AppleKeychainSecKeychain::AppleKeychainSecKeychain() = default;

AppleKeychainSecKeychain::~AppleKeychainSecKeychain() = default;

base::expected<std::vector<uint8_t>, OSStatus>
AppleKeychainSecKeychain::FindGenericPassword(
    std::string_view service_name,
    std::string_view account_name) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  uint32_t password_length = 0;
  void* password_data = nullptr;
  OSStatus status = SecKeychainFindGenericPassword(
      nullptr, service_name.length(), service_name.data(),
      account_name.length(), account_name.data(), &password_length,
      &password_data, nullptr);
  if (status != noErr) {
    return base::unexpected(status);
  }

  // SAFETY: SecKeychainFindGenericPassword returns an allocation of
  // `password_length` bytes in size.
  UNSAFE_BUFFERS(base::span password_span(
      static_cast<const uint8_t*>(password_data), password_length));
  auto result = base::ToVector(password_span);
  SecKeychainItemFreeContent(nullptr, password_data);
  return result;
}

OSStatus AppleKeychainSecKeychain::AddGenericPassword(
    std::string_view service_name,
    std::string_view account_name,
    base::span<const uint8_t> password) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  return SecKeychainAddGenericPassword(
      nullptr, base::checked_cast<uint32_t>(service_name.length()),
      service_name.data(), base::checked_cast<uint32_t>(account_name.length()),
      account_name.data(), base::checked_cast<uint32_t>(password.size()),
      password.data(), nullptr);
}

#pragma clang diagnostic pop

}  // namespace crypto
