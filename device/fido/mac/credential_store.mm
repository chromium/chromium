// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_store.h"

#include <string>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "device/base/features.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/keychain.h"

namespace device {
namespace fido {
namespace mac {

namespace {

// Erase all keychain items with a creation date that is not within [not_before,
// not_after).
void FilterKeychainItemsByCreationDate(
    std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>* keychain_items,
    base::Time not_before,
    base::Time not_after) {
  base::EraseIf(
      *keychain_items,
      [not_before, not_after](const CFDictionaryRef& attributes) -> bool {
        // If the creation date is missing for some obscure reason, treat as if
        // the date is inside the interval, i.e. keep it in the list.
        CFDateRef creation_date_cf =
            base::mac::GetValueFromDictionary<CFDateRef>(attributes,
                                                         kSecAttrCreationDate);
        if (!creation_date_cf) {
          return false;
        }
        base::Time creation_date = base::Time::FromCFAbsoluteTime(
            CFDateGetAbsoluteTime(creation_date_cf));
        return creation_date < not_before || creation_date >= not_after;
      });
}
base::Optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
QueryKeychainItemsForProfile(const std::string& keychain_access_group,
                             const std::string& metadata_secret,
                             base::Time created_not_before,
                             base::Time created_not_after)
    API_AVAILABLE(macosx(10.12.2)) {
  // Query the keychain for all items tagged with the given access group, which
  // should in theory yield all WebAuthentication credentials (for all
  // profiles). Sadly, the kSecAttrAccessGroup filter doesn't quite work, and
  // so we also get results from the legacy keychain that are tagged with no
  // keychain access group.
  std::vector<base::ScopedCFTypeRef<CFDictionaryRef>> result;

  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(keychain_access_group));
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  // Return the key reference and its attributes.
  CFDictionarySetValue(query, kSecReturnRef, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);

  base::ScopedCFTypeRef<CFArrayRef> keychain_items;
  {
    OSStatus status = Keychain::GetInstance().ItemCopyMatching(
        query, reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
    if (status == errSecItemNotFound) {
      DVLOG(1) << "no credentials found";
      return base::nullopt;
    }
    if (status != errSecSuccess) {
      OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
      return base::nullopt;
    }
  }

  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items); ++i) {
    CFDictionaryRef attributes = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items, i));
    if (!attributes) {
      DLOG(ERROR) << "unexpected result type";
      return base::nullopt;
    }

    // Skip items that don't belong to the correct keychain access group
    // because the kSecAttrAccessGroup filter is broken.
    CFStringRef attr_access_group =
        base::mac::GetValueFromDictionary<CFStringRef>(attributes,
                                                       kSecAttrAccessGroup);
    if (!attr_access_group || base::SysCFStringRefToUTF8(attr_access_group) !=
                                  keychain_access_group) {
      DVLOG(1) << "missing/invalid access group";
      continue;
    }

    // If the RP ID, stored encrypted in the item's label, cannot be decrypted
    // with the given metadata secret, then the credential belongs to a
    // different profile and must be ignored.
    CFStringRef sec_attr_label = base::mac::GetValueFromDictionary<CFStringRef>(
        attributes, kSecAttrLabel);
    if (!sec_attr_label) {
      DLOG(ERROR) << "missing label";
      continue;
    }
    base::Optional<std::string> opt_rp_id =
        DecodeRpId(metadata_secret, base::SysCFStringRefToUTF8(sec_attr_label));
    if (!opt_rp_id) {
      DVLOG(1) << "key doesn't belong to this profile";
      continue;
    }

    result.push_back(base::ScopedCFTypeRef<CFDictionaryRef>(
        attributes, base::scoped_policy::RETAIN));
  }

  FilterKeychainItemsByCreationDate(&result, created_not_before,
                                    created_not_after);
  return result;
}

bool DoDeleteWebAuthnCredentials(const std::string& keychain_access_group,
                                 const std::string& metadata_secret,
                                 base::Time created_not_before,
                                 base::Time created_not_after)
    API_AVAILABLE(macosx(10.12.2)) {
  bool result = true;
  base::Optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
      keychain_items =
          QueryKeychainItemsForProfile(keychain_access_group, metadata_secret,
                                       created_not_before, created_not_after);
  if (!keychain_items) {
    return false;
  }

  // The sane way to delete this item would be to build a query that has the
  // kSecMatchItemList field set to a list of SecKeyRef objects that need
  // deleting. Sadly, on macOS that appears to work only if you also set
  // kSecAttrNoLegacy (which is an internal symbol); otherwise it appears to
  // only search the "legacy" keychain and return errSecItemNotFound. What
  // does work however, is to look up and delete by the (unique)
  // kSecAttrApplicationLabel (which stores the credential id). So we clumsily
  // do this for each item instead.
  for (const CFDictionaryRef& attributes : *keychain_items) {
    CFDataRef sec_attr_app_label = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes, kSecAttrApplicationLabel);
    if (!sec_attr_app_label) {
      DLOG(ERROR) << "missing application label";
      continue;
    }
    base::ScopedCFTypeRef<CFMutableDictionaryRef> delete_query(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
    CFDictionarySetValue(delete_query, kSecClass, kSecClassKey);
    CFDictionarySetValue(delete_query, kSecAttrApplicationLabel,
                         sec_attr_app_label);
    OSStatus status = Keychain::GetInstance().ItemDelete(delete_query);
    if (status != errSecSuccess) {
      OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
      result = false;
      continue;
    }
  }
  return result;
}

size_t DoCountWebAuthnCredentials(const std::string& keychain_access_group,
                                  const std::string& metadata_secret,
                                  base::Time created_not_before,
                                  base::Time created_not_after)
    API_AVAILABLE(macosx(10.12.2)) {
  base::Optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
      keychain_items =
          QueryKeychainItemsForProfile(keychain_access_group, metadata_secret,
                                       created_not_before, created_not_after);
  if (!keychain_items) {
    DLOG(ERROR) << "Failed to query credentials in keychain";
    return 0;
  }

  return keychain_items->size();
}
}  // namespace

TouchIdCredentialStore::TouchIdCredentialStore(AuthenticatorConfig config)
    : config_(std::move(config)) {}
TouchIdCredentialStore::~TouchIdCredentialStore() = default;

bool TouchIdCredentialStore::DeleteCredentials(base::Time created_not_before,
                                               base::Time created_not_after) {
  // Touch ID uses macOS APIs available in 10.12.2 or newer. No need to check
  // for credentials in lower OS versions.
  if (__builtin_available(macos 10.12.2, *)) {
    return DoDeleteWebAuthnCredentials(config_.keychain_access_group,
                                       config_.metadata_secret,
                                       created_not_before, created_not_after);
  }
  return true;
}

size_t TouchIdCredentialStore::CountCredentials(base::Time created_not_before,
                                                base::Time created_not_after) {
  if (__builtin_available(macos 10.12.2, *)) {
    return DoCountWebAuthnCredentials(config_.keychain_access_group,
                                      config_.metadata_secret,
                                      created_not_before, created_not_after);
  }
  return 0;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
