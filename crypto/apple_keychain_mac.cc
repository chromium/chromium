// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"

namespace {

// Supports the pattern where a function F(T* out) allows |out| to be nullptr
// but its implementation requires a T variable even in the absence of |out|.
// Such a function can maintain a local OptionalOutParameter<T> to provide the
// internal T value, assigning its value to *out on destruction if possible.
template <typename T>
class OptionalOutParameter {
 public:
  OptionalOutParameter(const OptionalOutParameter&) = delete;
  OptionalOutParameter& operator=(const OptionalOutParameter&) = delete;

  OptionalOutParameter(T* out, T value = T()) : out_(out), value_(value) {}

  ~OptionalOutParameter() {
    if (out_) {
      *out_ = value_;
    }
  }

  OptionalOutParameter& operator=(T value) {
    value_ = value;
    return *this;
  }
  operator T() const { return value_; }

 private:
  const raw_ptr<T> out_;
  T value_;
};

}  // namespace

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace crypto {

AppleKeychain::AppleKeychain() = default;

AppleKeychain::~AppleKeychain() = default;

OSStatus AppleKeychain::FindGenericPassword(
    UInt32 service_name_length,
    const char* service_name,
    UInt32 account_name_length,
    const char* account_name,
    UInt32* password_length,
    void** password_data,
    AppleSecKeychainItemRef* item) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  return SecKeychainFindGenericPassword(
      nullptr, service_name_length, service_name, account_name_length,
      account_name, password_length, password_data, item);
}

OSStatus AppleKeychain::ItemFreeContent(void* data) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  return SecKeychainItemFreeContent(nullptr, data);
}

OSStatus AppleKeychain::AddGenericPassword(
    UInt32 service_name_length,
    const char* service_name,
    UInt32 account_name_length,
    const char* account_name,
    UInt32 password_length,
    const void* password_data,
    AppleSecKeychainItemRef* item) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  return SecKeychainAddGenericPassword(
      nullptr, service_name_length, service_name, account_name_length,
      account_name, password_length, password_data, item);
}

OSStatus AppleKeychain::ItemDelete(AppleSecKeychainItemRef item) const {
  base::AutoLock lock(GetMacSecurityServicesLock());
  return SecKeychainItemDelete(item);
}

ScopedKeychainUserInteractionAllowed::ScopedKeychainUserInteractionAllowed(
    Boolean allowed,
    OSStatus* status) {
  Boolean was_allowed;
  OptionalOutParameter<OSStatus> local_status(
      status, SecKeychainGetUserInteractionAllowed(&was_allowed));
  if (local_status != noErr) {
    return;
  }

  local_status = SecKeychainSetUserInteractionAllowed(allowed);
  if (local_status != noErr) {
    return;
  }

  was_allowed_ = was_allowed;
}

ScopedKeychainUserInteractionAllowed::~ScopedKeychainUserInteractionAllowed() {
  if (was_allowed_) {
    SecKeychainSetUserInteractionAllowed(*was_allowed_);
  }
}

#pragma clang diagnostic pop

}  // namespace crypto
