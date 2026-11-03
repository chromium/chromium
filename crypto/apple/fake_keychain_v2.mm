// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/fake_keychain_v2.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_typeref.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/scoped_policy.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "crypto/apple/keychain_v2.h"

#if !BUILDFLAG(IS_IOS_TVOS)
#import <LocalAuthentication/LocalAuthentication.h>
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace crypto::apple {

namespace {

// Returns true if the `item_value` matches the `query_value`.
// A null `query_value` is a wildcard and is always considered a match.
bool Matches(CFTypeRef query_value, CFTypeRef item_value) {
  return !query_value || (item_value && CFEqual(query_value, item_value));
}

// Null-safe symmetric equality for CoreFoundation values: returns true if both
// are null, or if both are non-null and `CFEqual` (which crashes on null
// arguments). Unlike `Matches()`, a null `lhs` is not a wildcard.
bool SameAttribute(CFTypeRef lhs, CFTypeRef rhs) {
  if (!lhs || !rhs) {
    return !lhs && !rhs;
  }
  return CFEqual(lhs, rhs);
}

bool IsGenericPassword(CFDictionaryRef item) {
  return SameAttribute(CFDictionaryGetValue(item, kSecClass),
                       kSecClassGenericPassword);
}

// Two generic passwords collide when they share the same `(service, account)`
// pair, treating an absent attribute as a distinct primary-key value rather
// than a wildcard.
bool GenericPasswordsCollide(CFDictionaryRef lhs, CFDictionaryRef rhs) {
  return IsGenericPassword(lhs) && IsGenericPassword(rhs) &&
         SameAttribute(CFDictionaryGetValue(lhs, kSecAttrService),
                       CFDictionaryGetValue(rhs, kSecAttrService)) &&
         SameAttribute(CFDictionaryGetValue(lhs, kSecAttrAccount),
                       CFDictionaryGetValue(rhs, kSecAttrAccount));
}

bool HasDuplicateGenericPassword(
    base::span<const base::apple::ScopedCFTypeRef<CFDictionaryRef>> items) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (!IsGenericPassword(items[i].get())) {
      continue;
    }
    for (size_t j = i + 1; j < items.size(); ++j) {
      if (GenericPasswordsCollide(items[i].get(), items[j].get())) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if `item` matches `query` on its class and on every other
// attribute this fake models. Attributes absent from `query` are wildcards.
// For class-qualified queries, checking the class first keeps class-specific
// attributes from being compared across item types; for example, generic
// passwords have no application label and keys have no account. This is the
// single matcher shared by `ItemCopyMatching()`, `ItemUpdate()` and
// `ItemDelete()`.
bool ItemMatchesQuery(CFDictionaryRef query, CFDictionaryRef item) {
  for (CFStringRef attribute :
       {kSecClass, kSecAttrLabel, kSecAttrApplicationLabel,
        kSecAttrApplicationTag, kSecAttrService, kSecAttrAccount,
        kSecAttrAccessible}) {
    if (!Matches(CFDictionaryGetValue(query, attribute),
                 CFDictionaryGetValue(item, attribute))) {
      return false;
    }
  }
  return true;
}

constexpr char kPassword[] = "mock_password";

// Adds an entry to a local histogram to indicate that the Keychain would have
// been accessed, if this class were not a mock of the Keychain.
void IncrementKeychainAccessHistogram() {
  // This local histogram is accessed by Telemetry to track the number of times
  // the keychain is accessed, since keychain access is known to be synchronous
  // and slow.
  LOCAL_HISTOGRAM_BOOLEAN("OSX.Keychain.Access", true);
}

}  // namespace

FakeKeychainV2::FakeKeychainV2(const std::string& keychain_access_group)
    : keychain_access_group_(
          base::SysUTF8ToCFStringRef(keychain_access_group)) {
  CHECK(keychain_access_group_);
}
FakeKeychainV2::~FakeKeychainV2() {
  // Avoid shutdown leak of error string in Security.framework.
  // See
  // https://github.com/apple-oss-distributions/Security/blob/Security-60158.140.3/OSX/libsecurity_keychain/lib/SecBase.cpp#L88
#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif
}

NSArray* FakeKeychainV2::GetTokenIDs() {
  base::AutoLock auto_lock(lock_);
  if (is_secure_enclave_available_) {
    return @[ base::apple::CFToNSPtrCast(kSecAttrTokenIDSecureEnclave) ];
  }
  return @[];
}

base::apple::ScopedCFTypeRef<SecKeyRef> FakeKeychainV2::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  base::AutoLock auto_lock(lock_);
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
  DCHECK(SameAttribute(
      base::apple::GetValueFromDictionary<CFStringRef>(params, kSecAttrTokenID),
      kSecAttrTokenIDSecureEnclave));
  DCHECK(SameAttribute(base::apple::GetValueFromDictionary<CFStringRef>(
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
  DCHECK(SameAttribute(base::apple::GetValueFromDictionary<CFBooleanRef>(
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
  // SecKeyCreateRandomKey() parameters don't carry a class, but the stored
  // item needs one so that class-qualified queries match it, as they would
  // in the real Keychain.
  CFDictionarySetValue(keychain_item.get(), kSecClass, kSecClassKey);

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

  CFDateRef unix_epoch =
      base::apple::NSToCFPtrCast(base::Time::UnixEpoch().ToNSDate());
  CFDictionarySetValue(keychain_item.get(), kSecAttrCreationDate, unix_epoch);
  CFDictionarySetValue(keychain_item.get(), kSecAttrModificationDate,
                       unix_epoch);

  items_.push_back(keychain_item);

  return private_key;
}

base::apple::ScopedCFTypeRef<CFDictionaryRef> FakeKeychainV2::KeyCopyAttributes(
    SecKeyRef key) {
  base::AutoLock auto_lock(lock_);
  if (!key) {
    return base::apple::ScopedCFTypeRef<CFDictionaryRef>();
  }
  const auto it = std::ranges::find_if(items_, [&key](const auto& item) {
    CFTypeRef item_key = CFDictionaryGetValue(item.get(), kSecValueRef);
    return item_key && CFEqual(key, item_key);
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

OSStatus FakeKeychainV2::ItemAdd(CFDictionaryRef attributes,
                                 CFTypeRef* result) {
  CHECK(attributes);
  base::AutoLock auto_lock(lock_);
  if (item_add_result_ != noErr) {
    return item_add_result_;
  }
  if (!SameAttribute(CFDictionaryGetValue(attributes, kSecAttrAccessGroup),
                     keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }
  CFStringRef item_class =
      base::apple::GetValueFromDictionary<CFStringRef>(attributes, kSecClass);
  if (!item_class) {
    // SecItemAdd() rejects items without a class.
    return errSecParam;
  }
  CHECK(CFEqual(item_class, kSecClassKey) ||
        CFEqual(item_class, kSecClassGenericPassword));
  if (IsGenericPassword(attributes)) {
    // Like SecItemAdd, reject a second generic password with the same service
    // and account.
    if (std::ranges::any_of(items_, [&](const auto& existing) {
          return GenericPasswordsCollide(attributes, existing.get());
        })) {
      return errSecDuplicateItem;
    }
  }
  // Store a mutable copy so caller edits after `ItemAdd()` don't affect
  // `items_`, while existing tests that `const_cast` `items()` entries remain
  // valid.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> item(
      CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                    attributes));
  items_.push_back(std::move(item));
  return errSecSuccess;
}

OSStatus FakeKeychainV2::ItemCopyMatching(CFDictionaryRef query,
                                          CFTypeRef* result) {
  CHECK(query);
  base::AutoLock auto_lock(lock_);
  if (CFTypeRef query_access_group =
          CFDictionaryGetValue(query, kSecAttrAccessGroup);
      query_access_group &&
      !SameAttribute(query_access_group, keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }

  CFStringRef match_limit =
      base::apple::GetValueFromDictionary<CFStringRef>(query, kSecMatchLimit);
  bool match_all = match_limit && CFEqual(match_limit, kSecMatchLimitAll);

  // Filter the items based on `query`.
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> items(
      CFArrayCreateMutable(nullptr, items_.size(), &kCFTypeArrayCallBacks));
  for (auto& item : items_) {
    // Each `FakeKeychainV2` instance operates only on items in its
    // `keychain_access_group_`.
    if (!SameAttribute(keychain_access_group_.get(),
                       CFDictionaryGetValue(item.get(), kSecAttrAccessGroup))) {
      continue;
    }

    if (!ItemMatchesQuery(query, item.get())) {
      continue;
    }
    if (match_all) {
      base::apple::ScopedCFTypeRef<CFDictionaryRef> item_copy(
          CFDictionaryCreateCopy(kCFAllocatorDefault, item.get()));
      CFArrayAppendValue(items.get(), item_copy.get());
    } else {
      if (result) {
        *result = CFDictionaryCreateCopy(kCFAllocatorDefault, item.get());
      }
      return errSecSuccess;
    }
  }
  if (CFArrayGetCount(items.get()) == 0) {
    return errSecItemNotFound;
  }
  if (result) {
    *result = items.release();
  }
  return errSecSuccess;
}

OSStatus FakeKeychainV2::ItemDelete(CFDictionaryRef query) {
  CHECK(query);
  base::AutoLock auto_lock(lock_);
  if (!SameAttribute(CFDictionaryGetValue(query, kSecAttrAccessGroup),
                     keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }
  // Validate certain fields that we always expect to be set, so that an
  // under-specified query can't delete unrelated items.
  CFStringRef query_class =
      base::apple::GetValueFromDictionary<CFStringRef>(query, kSecClass);
  if (!query_class) {
    return errSecParam;
  }
  if (CFEqual(query_class, kSecClassGenericPassword)) {
    CHECK(base::apple::GetValueFromDictionary<CFStringRef>(query,
                                                           kSecAttrAccount));
  } else {
    CHECK(CFEqual(query_class, kSecClassKey));
    CHECK(base::apple::GetValueFromDictionary<CFDataRef>(
        query, kSecAttrApplicationLabel));
  }

  const size_t n_erased = std::erase_if(
      items_, [&](const base::apple::ScopedCFTypeRef<CFDictionaryRef>& item) {
        return ItemMatchesQuery(query, item.get());
      });
  return n_erased != 0 ? errSecSuccess : errSecItemNotFound;
}

OSStatus FakeKeychainV2::ItemUpdate(CFDictionaryRef query,
                                    CFDictionaryRef attributes_to_update) {
  CHECK(query);
  CHECK(attributes_to_update);
  base::AutoLock auto_lock(lock_);
  if (item_update_result_ != noErr) {
    return item_update_result_;
  }
  if (!SameAttribute(CFDictionaryGetValue(query, kSecAttrAccessGroup),
                     keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }
  CFStringRef query_class =
      base::apple::GetValueFromDictionary<CFStringRef>(query, kSecClass);
  if (!query_class) {
    return errSecParam;
  }
  CHECK(CFEqual(query_class, kSecClassKey) ||
        CFEqual(query_class, kSecClassGenericPassword));
  if (CFDictionaryContainsKey(attributes_to_update, kSecClass)) {
    // SecItemUpdate() rejects kSecClass in the update dictionary.
    return errSecParam;
  }
  if (CFTypeRef updated_access_group =
          CFDictionaryGetValue(attributes_to_update, kSecAttrAccessGroup);
      updated_access_group &&
      !SameAttribute(updated_access_group, keychain_access_group_.get())) {
    return errSecMissingEntitlement;
  }

  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> new_items;
  new_items.reserve(items_.size());
  OSStatus result = errSecItemNotFound;
  for (base::apple::ScopedCFTypeRef<CFDictionaryRef>& item : items_) {
    if (!ItemMatchesQuery(query, item.get())) {
      new_items.push_back(item);
      continue;
    }

    base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> item_copy(
        CFDictionaryCreateMutableCopy(kCFAllocatorDefault, /*capacity=*/0,
                                      item.get()));
    [base::apple::CFToNSPtrCast(item_copy.get())
        addEntriesFromDictionary:base::apple::CFToNSPtrCast(
                                     attributes_to_update)];
    new_items.push_back(std::move(item_copy));
    // Succeed if we replaced at least one item matching the query.
    result = errSecSuccess;
  }

  if (result == errSecSuccess) {
    if (HasDuplicateGenericPassword(new_items)) {
      return errSecDuplicateItem;
    }
    items_ = std::move(new_items);
  }
  return result;
}

// `FindGenericPassword()` and `AddGenericPassword()` intentionally do not
// touch `items_`; see the comment in fake_keychain_v2.h.
base::expected<std::vector<uint8_t>, OSStatus>
FakeKeychainV2::FindGenericPassword(std::string_view service_name,
                                    std::string_view account_name) {
  IncrementKeychainAccessHistogram();

  base::AutoLock auto_lock(lock_);
  // When simulating |noErr|, return mock password. Otherwise, return given
  // code.
  if (find_generic_result_ == noErr) {
    return base::ToVector(base::byte_span_from_cstring(kPassword));
  }

  return base::unexpected(find_generic_result_);
}

OSStatus FakeKeychainV2::AddGenericPassword(
    std::string_view service_name,
    std::string_view account_name,
    base::span<const uint8_t> password) {
  IncrementKeychainAccessHistogram();
  base::AutoLock auto_lock(lock_);
  called_add_generic_ = true;

  DCHECK(!password.empty());
  return noErr;
}

std::string FakeKeychainV2::GetEncryptionPassword() const {
  IncrementKeychainAccessHistogram();
  return kPassword;
}

#if !BUILDFLAG(IS_IOS)
base::apple::ScopedCFTypeRef<CFTypeRef>
FakeKeychainV2::TaskCopyValueForEntitlement(SecTaskRef task,
                                            CFStringRef entitlement,
                                            CFErrorRef* error) {
  CHECK(task);
  CHECK(SameAttribute(entitlement, CFSTR("keychain-access-groups")))
      << "Entitlement " << entitlement << " not supported by fake";
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> keychain_access_groups(
      CFArrayCreateMutable(kCFAllocatorDefault, /*capacity=*/1,
                           &kCFTypeArrayCallBacks));
  CFArrayAppendValue(keychain_access_groups.get(),
                     keychain_access_group_.get());
  return keychain_access_groups;
}
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS_TVOS)
BOOL FakeKeychainV2::LAContextCanEvaluatePolicy(
    LAPolicy policy,
    NSError* __autoreleasing* error) {
  base::AutoLock auto_lock(lock_);
  switch (policy) {
    case LAPolicyDeviceOwnerAuthentication:
      return uv_method_ == UVMethod::kBiometrics ||
             uv_method_ == UVMethod::kPasswordOnly;
    case LAPolicyDeviceOwnerAuthenticationWithBiometrics:
      return uv_method_ == UVMethod::kBiometrics;
#if !BUILDFLAG(IS_IOS)
    case LAPolicyDeviceOwnerAuthenticationWithBiometricsOrCompanion:
      return uv_method_ == UVMethod::kBiometrics;
#endif        // !BUILDFLAG(IS_IOS)
    default:  // Avoid needing to refer to values not available in the minimum
              // supported macOS version.
      NOTIMPLEMENTED();
      return false;
  }
}
#endif  // !BUILDFLAG(IS_IOS_TVOS)

}  // namespace crypto::apple
