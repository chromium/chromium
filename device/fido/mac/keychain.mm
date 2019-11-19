// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/keychain.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/mac/credential_metadata.h"

namespace device {
namespace fido {
namespace mac {

static API_AVAILABLE(macos(10.12.2)) Keychain* g_keychain_instance_override =
    nullptr;

// static
Keychain& Keychain::GetInstance() {
  if (g_keychain_instance_override) {
    return *g_keychain_instance_override;
  }
  static base::NoDestructor<Keychain> k;
  return *k;
}

// static
void Keychain::SetInstanceOverride(Keychain* keychain) {
  CHECK(!g_keychain_instance_override);
  g_keychain_instance_override = keychain;
}

// static
void Keychain::ClearInstanceOverride() {
  CHECK(g_keychain_instance_override);
  g_keychain_instance_override = nullptr;
}

Keychain::Keychain() = default;
Keychain::~Keychain() = default;

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCreateRandomKey(params, error));
}

base::ScopedCFTypeRef<CFDataRef> Keychain::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCopyPublicKey(SecKeyRef key) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

OSStatus Keychain::ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus Keychain::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

Credential::Credential(base::ScopedCFTypeRef<SecKeyRef> private_key_,
                       std::vector<uint8_t> credential_id_)
    : private_key(std::move(private_key_)),
      credential_id(std::move(credential_id_)) {}
Credential::~Credential() = default;
Credential::Credential(Credential&& other) = default;
Credential& Credential::operator=(Credential&& other) = default;

// Like FindCredentialsInKeychain(), but empty |allowed_credential_ids| allows
// any credential to match.
static std::list<Credential> FindCredentialsImpl(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    const std::set<std::vector<uint8_t>>& allowed_credential_ids,
    LAContext* authentication_context) API_AVAILABLE(macosx(10.12.2)) {
  base::Optional<std::string> encoded_rp_id =
      EncodeRpId(metadata_secret, rp_id);
  if (!encoded_rp_id) {
    return {};
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(keychain_access_group));
  CFDictionarySetValue(query, kSecAttrLabel,
                       base::SysUTF8ToNSString(*encoded_rp_id));
  if (authentication_context) {
    CFDictionarySetValue(query, kSecUseAuthenticationContext,
                         authentication_context);
  }
  CFDictionarySetValue(query, kSecReturnRef, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

  base::ScopedCFTypeRef<CFArrayRef> keychain_items;
  OSStatus status = Keychain::GetInstance().ItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
  if (status == errSecItemNotFound) {
    // No credentials for the RP.
    return {};
  }
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
    return {};
  }

  // Filter credentials for the RP down to |allowed_credential_ids|, unless it's
  // empty in which case all credentials should be returned.
  std::list<Credential> result;
  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items); ++i) {
    CFDictionaryRef attributes = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items, i));
    CFDataRef application_label = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes, kSecAttrApplicationLabel);
    SecKeyRef key =
        base::mac::GetValueFromDictionary<SecKeyRef>(attributes, kSecValueRef);
    if (!application_label || !key) {
      // Corrupted keychain?
      DLOG(ERROR) << "could not find application label or key ref: "
                  << attributes;
      continue;
    }
    std::vector<uint8_t> credential_id(CFDataGetBytePtr(application_label),
                                       CFDataGetBytePtr(application_label) +
                                           CFDataGetLength(application_label));
    if (!allowed_credential_ids.empty() &&
        !base::Contains(allowed_credential_ids, credential_id)) {
      continue;
    }
    base::ScopedCFTypeRef<SecKeyRef> private_key(key,
                                                 base::scoped_policy::RETAIN);
    result.emplace_back(
        Credential(std::move(private_key), std::move(credential_id)));
  }
  return result;
}

std::list<Credential> FindCredentialsInKeychain(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    const std::set<std::vector<uint8_t>>& allowed_credential_ids,
    LAContext* authentication_context) {
  if (allowed_credential_ids.empty()) {
    NOTREACHED();
    return {};
  }
  return FindCredentialsImpl(keychain_access_group, metadata_secret, rp_id,
                             allowed_credential_ids, authentication_context);
}

std::list<Credential> FindResidentCredentialsInKeychain(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    LAContext* authentication_context) {
  std::list<Credential> result = FindCredentialsImpl(
      keychain_access_group, metadata_secret, rp_id,
      /*allowed_credential_ids=*/{}, authentication_context);
  result.remove_if([&metadata_secret, &rp_id](const Credential& credential) {
    auto opt_metadata =
        UnsealCredentialId(metadata_secret, rp_id, credential.credential_id);
    if (!opt_metadata) {
      FIDO_LOG(ERROR) << "UnsealCredentialId() failed";
      return true;
    }
    return !opt_metadata->is_resident;
  });
  return result;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
