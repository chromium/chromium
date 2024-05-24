// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/fake_apple_keychain_v2.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include <algorithm>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_typeref.h"
#include "base/check_op.h"
#include "base/memory/scoped_policy.h"
#include "base/notimplemented.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple_keychain_v2.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace crypto {

FakeAppleKeychainV2::FakeAppleKeychainV2(
    const std::string& keychain_access_group)
    : keychain_access_group_(
          base::SysUTF8ToCFStringRef(keychain_access_group)) {}
FakeAppleKeychainV2::~FakeAppleKeychainV2() {
  // Avoid shutdown leak of error string in Security.framework.
  // See
  // https://github.com/apple-oss-distributions/Security/blob/Security-60158.140.3/OSX/libsecurity_keychain/lib/SecBase.cpp#L88
#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif
}

NSArray* FakeAppleKeychainV2::GetTokenIDs() {
  if (is_secure_enclave_available_) {
    return @[ base::apple::CFToNSPtrCast(kSecAttrTokenIDSecureEnclave) ];
  }
  return @[];
}

base::apple::ScopedCFTypeRef<SecKeyRef> FakeAppleKeychainV2::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  // Validate certain fields that we always expect to be set.
  DCHECK(
      base::apple::GetValueFromDictionary<CFStringRef>(params, kSecAttrLabel));
  // kSecAttrApplicationTag is CFDataRef for new credentials and CFStringRef for
  // version < 3. Keychain docs say it should be CFDataRef
  // (https://developer.apple.com/documentation/security/ksecattrapplicationtag).
  CFTypeRef application_tag = nil;
  CFDictionaryGetValueIfPresent(params, kSecAttrApplicationTag,
                                &application_tag);
  if (application_tag) {
    CHECK(base::apple::CFCast<CFDataRef>(application_tag) ||
          base::apple::CFCast<CFStringRef>(application_tag));
  }
  DCHECK_EQ(
      base::apple::GetValueFromDictionary<CFStringRef>(params, kSecAttrTokenID),
      kSecAttrTokenIDSecureEnclave);
  DCHECK(CFEqual(base::apple::GetValueFromDictionary<CFStringRef>(
                     params, kSecAttrAccessGroup),
                 keychain_access_group_.get()));

  // Call Keychain services to create a key pair, but first drop all parameters
  // that aren't appropriate in tests.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> params_copy(
      CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                    params));
  // Don't create a Secure Enclave key.
  CFDictionaryRemoveValue(params_copy.get(), kSecAttrTokenID);
  // Don't bind to a keychain-access-group, which would require an entitlement.
  CFDictionaryRemoveValue(params_copy.get(), kSecAttrAccessGroup);

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
      CFDictionaryCreateMutableCopy(
          kCFAllocatorDefault, /*capacity=*/0,
          base::apple::GetValueFromDictionary<CFDictionaryRef>(
              params_copy.get(), kSecPrivateKeyAttrs)));
  DCHECK(CFEqual(base::apple::GetValueFromDictionary<CFBooleanRef>(
                     private_key_params.get(), kSecAttrIsPermanent),
                 kCFBooleanTrue));
  CFDictionarySetValue(private_key_params.get(), kSecAttrIsPermanent,
                       kCFBooleanFalse);
  CFDictionaryRemoveValue(private_key_params.get(), kSecAttrAccessControl);
  CFDictionaryRemoveValue(private_key_params.get(),
                          kSecUseAuthenticationContext);
  CFDictionarySetValue(params_copy.get(), kSecPrivateKeyAttrs,
                       private_key_params.get());
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key(
      SecKeyCreateRandomKey(params_copy.get(), error));
  if (!private_key) {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }

  // Stash everything in `items_` so it can be  retrieved in with
  // `ItemCopyMatching. This uses the original `params` rather than the modified
  // copy so that `ItemCopyMatching()` will correctly filter on
  // kSecAttrAccessGroup.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> keychain_item(
      CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                    params));
  CFDictionarySetValue(keychain_item.get(), kSecValueRef, private_key.get());

  // When left unset, the real keychain sets the application label to the hash
  // of the public key on creation. We need to retrieve it to allow filtering
  // for it later.
  if (!base::apple::GetValueFromDictionary<CFDataRef>(
          keychain_item.get(), kSecAttrApplicationLabel)) {
    base::apple::ScopedCFTypeRef<CFDictionaryRef> key_metadata(
        SecKeyCopyAttributes(private_key.get()));
    CFDataRef application_label =
        base::apple::GetValueFromDictionary<CFDataRef>(
            key_metadata.get(), kSecAttrApplicationLabel);
    CFDictionarySetValue(keychain_item.get(), kSecAttrApplicationLabel,
                         application_label);
  }
  items_.push_back(keychain_item);

  return private_key;
}

base::apple::ScopedCFTypeRef<CFDictionaryRef>
FakeAppleKeychainV2::KeyCopyAttributes(SecKeyRef key) {
  const auto& it = std::ranges::find_if(items_, [&key](const auto& item) {
    return CFEqual(key, CFDictionaryGetValue(item.get(), kSecValueRef));
  });
  if (it == items_.end()) {
    return base::apple::ScopedCFTypeRef<CFDictionaryRef>();
  }
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> result(
      CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                    it->get()));
  // The real implementation does not return the actual key.
  CFDictionaryRemoveValue(result.get(), kSecValueRef);
  return result;
}

OSStatus FakeAppleKeychainV2::ItemAdd(CFDictionaryRef attributes,
                                      CFTypeRef* result) {
  CFStringRef keychain_access_group =
      base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                       kSecAttrAccessGroup);
  if (!CFEqual(keychain_access_group, keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }
  base::apple::ScopedCFTypeRef<CFDictionaryRef> item(
      attributes, base::scoped_policy::RETAIN);
  items_.push_back(item);
  return errSecSuccess;
}

OSStatus FakeAppleKeychainV2::ItemCopyMatching(CFDictionaryRef query,
                                               CFTypeRef* result) {
  // In practice we don't need to care about limit queries, or leaving out the
  // SecKeyRef or attributes from the result set.
  DCHECK_EQ(
      base::apple::GetValueFromDictionary<CFBooleanRef>(query, kSecReturnRef),
      kCFBooleanTrue);
  DCHECK_EQ(base::apple::GetValueFromDictionary<CFBooleanRef>(
                query, kSecReturnAttributes),
            kCFBooleanTrue);
  CFStringRef match_limit =
      base::apple::GetValueFromDictionary<CFStringRef>(query, kSecMatchLimit);
  bool match_all = match_limit && CFEqual(match_limit, kSecMatchLimitAll);

  // Match fields present in `query`.
  CFStringRef query_label =
      base::apple::GetValueFromDictionary<CFStringRef>(query, kSecAttrLabel);
  CFDataRef query_application_label =
      base::apple::GetValueFromDictionary<CFDataRef>(query,
                                                     kSecAttrApplicationLabel);
  // kSecAttrApplicationTag can be CFStringRef for legacy credentials and
  // CFDataRef for new ones, hence using CFTypeRef.
  CFTypeRef query_application_tag =
      CFDictionaryGetValue(query, kSecAttrApplicationTag);

  // Filter the items based on `query`.
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> items(
      CFArrayCreateMutable(nullptr, items_.size(), &kCFTypeArrayCallBacks));
  for (auto& item : items_) {
    // Each `Keychain` instance is expected to operate only on items of a single
    // keychain-access-group, which is tied to the `Profile`.
    CFStringRef keychain_access_group =
        base::apple::GetValueFromDictionary<CFStringRef>(query,
                                                         kSecAttrAccessGroup);
    DCHECK(CFEqual(keychain_access_group,
                   base::apple::GetValueFromDictionary<CFStringRef>(
                       item.get(), kSecAttrAccessGroup)) &&
           CFEqual(keychain_access_group, keychain_access_group_.get()));

    CFStringRef item_label = base::apple::GetValueFromDictionary<CFStringRef>(
        item.get(), kSecAttrLabel);
    CFDataRef item_application_label =
        base::apple::GetValueFromDictionary<CFDataRef>(
            item.get(), kSecAttrApplicationLabel);
    CFTypeRef item_application_tag =
        CFDictionaryGetValue(item.get(), kSecAttrApplicationTag);
    if ((query_label && (!item_label || !CFEqual(query_label, item_label))) ||
        (query_application_label &&
         (!item_application_label ||
          !CFEqual(query_application_label, item_application_label))) ||
        (query_application_tag &&
         (!item_application_tag ||
          !CFEqual(query_application_tag, item_application_tag)))) {
      continue;
    }
    if (match_all) {
      base::apple::ScopedCFTypeRef<CFDictionaryRef> item_copy(
          CFDictionaryCreateCopy(kCFAllocatorDefault, item.get()));
      CFArrayAppendValue(items.get(), item_copy.get());
    } else {
      *result = CFDictionaryCreateCopy(kCFAllocatorDefault, item.get());
      return errSecSuccess;
    }
  }
  if (CFArrayGetCount(items.get()) == 0) {
    return errSecItemNotFound;
  }
  *result = items.release();
  return errSecSuccess;
}

OSStatus FakeAppleKeychainV2::ItemDelete(CFDictionaryRef query) {
  // Validate certain fields that we always expect to be set.
  DCHECK_EQ(base::apple::GetValueFromDictionary<CFStringRef>(query, kSecClass),
            kSecClassKey);
  DCHECK(CFEqual(base::apple::GetValueFromDictionary<CFStringRef>(
                     query, kSecAttrAccessGroup),
                 keychain_access_group_.get()));
  // Only supporting deletion via `kSecAttrApplicationLabel` (credential ID) for
  // now (see `TouchIdCredentialStore::DeleteCredentialById()`).
  CFDataRef query_credential_id =
      base::apple::GetValueFromDictionary<CFDataRef>(query,
                                                     kSecAttrApplicationLabel);
  DCHECK(query_credential_id);
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    const base::apple::ScopedCFTypeRef<CFDictionaryRef>& item = *it;
    CFDataRef item_credential_id =
        base::apple::GetValueFromDictionary<CFDataRef>(
            item.get(), kSecAttrApplicationLabel);
    DCHECK(item_credential_id);
    if (CFEqual(query_credential_id, item_credential_id)) {
      items_.erase(it);  // N.B. `it` becomes invalid
      return errSecSuccess;
    }
  }
  return errSecItemNotFound;
}

OSStatus FakeAppleKeychainV2::ItemUpdate(CFDictionaryRef query,
                                         CFDictionaryRef attributes_to_update) {
  DCHECK_EQ(base::apple::GetValueFromDictionary<CFStringRef>(query, kSecClass),
            kSecClassKey);
  DCHECK(CFEqual(base::apple::GetValueFromDictionary<CFStringRef>(
                     query, kSecAttrAccessGroup),
                 keychain_access_group_.get()));
  CFDataRef query_credential_id =
      base::apple::GetValueFromDictionary<CFDataRef>(query,
                                                     kSecAttrApplicationLabel);
  DCHECK(query_credential_id);
  for (base::apple::ScopedCFTypeRef<CFDictionaryRef>& item : items_) {
    CFDataRef item_credential_id =
        base::apple::GetValueFromDictionary<CFDataRef>(
            item.get(), kSecAttrApplicationLabel);
    DCHECK(item_credential_id);
    if (!CFEqual(query_credential_id, item_credential_id)) {
      continue;
    }
    base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> item_copy(
        CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                      item.get()));
    [base::apple::CFToNSPtrCast(item_copy.get())
        addEntriesFromDictionary:base::apple::CFToNSPtrCast(
                                     attributes_to_update)];
    item = item_copy;
    return errSecSuccess;
  }
  return errSecItemNotFound;
}

#if !BUILDFLAG(IS_IOS)
base::apple::ScopedCFTypeRef<CFTypeRef>
FakeAppleKeychainV2::TaskCopyValueForEntitlement(SecTaskRef task,
                                                 CFStringRef entitlement,
                                                 CFErrorRef* error) {
  CHECK(task);
  CHECK(CFEqual(entitlement,
                base::SysUTF8ToCFStringRef("keychain-access-groups").get()))
      << "Entitlement " << entitlement << " not supported by fake";
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> keychain_access_groups(
      CFArrayCreateMutable(kCFAllocatorDefault, /*capacity=*/1,
                           &kCFTypeArrayCallBacks));
  CFArrayAppendValue(
      keychain_access_groups.get(),
      CFStringCreateCopy(kCFAllocatorDefault, keychain_access_group_.get()));
  return keychain_access_groups;
}
#endif  // !BUILDFLAG(IS_IOS)

BOOL FakeAppleKeychainV2::LAContextCanEvaluatePolicy(
    LAPolicy policy,
    NSError* __autoreleasing* error) {
  switch (policy) {
    case LAPolicyDeviceOwnerAuthentication:
      return uv_method_ == UVMethod::kBiometrics ||
             uv_method_ == UVMethod::kPasswordOnly;
    case LAPolicyDeviceOwnerAuthenticationWithBiometrics:
      return uv_method_ == UVMethod::kBiometrics;
    case LAPolicyDeviceOwnerAuthenticationWithBiometricsOrWatch:
      return uv_method_ == UVMethod::kBiometrics;
    default:  // Avoid needing to refer to values not available in the minimum
              // supported macOS version.
      NOTIMPLEMENTED();
      return false;
  }
}

}  // namespace crypto
