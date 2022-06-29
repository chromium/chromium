// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_store.h"

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/touch_id_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device::fido::mac {

namespace {

// DefaultKeychainQuery returns a default keychain query dictionary that has
// the keychain item class, keychain access group and RP ID (unless `rp_id` is
// `nullopt`) filled out. More fields can be set on the return value to refine
// the query.
base::ScopedCFTypeRef<CFMutableDictionaryRef> DefaultKeychainQuery(
    const AuthenticatorConfig& config,
    absl::optional<std::string> rp_id) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(config.keychain_access_group));
  if (rp_id) {
    CFDictionarySetValue(
        query, kSecAttrLabel,
        base::SysUTF8ToNSString(EncodeRpId(config.metadata_secret, *rp_id)));
  }
  return query;
}

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

absl::optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
QueryKeychainItemsForProfile(const std::string& keychain_access_group,
                             const std::string& metadata_secret,
                             base::Time created_not_before,
                             base::Time created_not_after) {
  // Query the keychain for all items tagged with the given access group, which
  // should in theory yield all WebAuthentication credentials (for all
  // profiles). Sadly, the kSecAttrAccessGroup filter doesn't quite work, and
  // so we also get results from the legacy keychain that are tagged with no
  // keychain access group.
  std::vector<base::ScopedCFTypeRef<CFDictionaryRef>> result;

  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
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
      return absl::nullopt;
    }
    if (status != errSecSuccess) {
      OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
      return absl::nullopt;
    }
  }

  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items); ++i) {
    CFDictionaryRef attributes = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items, i));
    if (!attributes) {
      DLOG(ERROR) << "unexpected result type";
      return absl::nullopt;
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
    absl::optional<std::string> opt_rp_id =
        DecodeRpId(metadata_secret, base::SysCFStringRefToUTF8(sec_attr_label));
    if (!opt_rp_id) {
      DVLOG(1) << "key doesn't belong to this profile";
      continue;
    }

    result.emplace_back(attributes, base::scoped_policy::RETAIN);
  }

  FilterKeychainItemsByCreationDate(&result, created_not_before,
                                    created_not_after);
  return result;
}

}  // namespace

Credential::Credential(base::ScopedCFTypeRef<SecKeyRef> private_key_,
                       std::vector<uint8_t> credential_id_)
    : private_key(std::move(private_key_)),
      credential_id(std::move(credential_id_)) {}

Credential::Credential(const Credential& other) = default;

Credential::Credential(Credential&& other) = default;

Credential& Credential::operator=(const Credential& other) = default;

Credential& Credential::operator=(Credential&& other) = default;

Credential::~Credential() = default;

bool Credential::operator==(const Credential& other) const {
  return CFEqual(private_key, other.private_key) &&
         credential_id == other.credential_id;
}

TouchIdCredentialStore::TouchIdCredentialStore(AuthenticatorConfig config)
    : config_(std::move(config)) {}
TouchIdCredentialStore::~TouchIdCredentialStore() = default;

base::ScopedCFTypeRef<SecAccessControlRef>
TouchIdCredentialStore::DefaultAccessControl() {
  return base::ScopedCFTypeRef<SecAccessControlRef>(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault,
          // Credential can only be used when the device is unlocked.
          kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
          // Private key is available for signing after user authorization with
          // biometrics or password.
          kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence,
          nullptr));
}

absl::optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
TouchIdCredentialStore::CreateCredential(
    const std::string& rp_id,
    const PublicKeyCredentialUserEntity& user,
    Discoverable discoverable) const {
  std::vector<uint8_t> credential_id =
      SealCredentialId(config_.metadata_secret, rp_id,
                       CredentialMetadata::FromPublicKeyCredentialUserEntity(
                           user, discoverable == kDiscoverable));
  base::ScopedCFTypeRef<CFMutableDictionaryRef> params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(params, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(config_.keychain_access_group));
  CFDictionarySetValue(params, kSecAttrKeyType,
                       kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(params, kSecAttrKeySizeInBits, @256);
  CFDictionarySetValue(params, kSecAttrSynchronizable, @NO);
  CFDictionarySetValue(params, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);

  CFDictionarySetValue(
      params, kSecAttrLabel,
      base::SysUTF8ToNSString(EncodeRpId(config_.metadata_secret, rp_id)));
  CFDictionarySetValue(params, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(EncodeRpIdAndUserId(
                           config_.metadata_secret, rp_id, user.id)));
  CFDictionarySetValue(params, kSecAttrApplicationLabel,
                       [NSData dataWithBytes:credential_id.data()
                                      length:credential_id.size()]);
  base::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(params, kSecPrivateKeyAttrs, private_key_params);
  CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @YES);
  CFDictionarySetValue(private_key_params, kSecAttrAccessControl,
                       DefaultAccessControl());
  if (authentication_context_) {
    CFDictionarySetValue(private_key_params, kSecUseAuthenticationContext,
                         authentication_context_);
  }
  base::ScopedCFTypeRef<CFErrorRef> cferr;
  base::ScopedCFTypeRef<SecKeyRef> private_key =
      Keychain::GetInstance().KeyCreateRandomKey(params,
                                                 cferr.InitializeInto());
  if (!private_key) {
    FIDO_LOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr;
    return absl::nullopt;
  }
  base::ScopedCFTypeRef<SecKeyRef> public_key(
      Keychain::GetInstance().KeyCopyPublicKey(private_key));
  if (!public_key) {
    FIDO_LOG(ERROR) << "SecKeyCopyPublicKey failed";
    return absl::nullopt;
  }

  return std::make_pair(
      Credential(std::move(private_key), std::move(credential_id)),
      std::move(public_key));
}

absl::optional<std::list<Credential>>
TouchIdCredentialStore::FindCredentialsFromCredentialDescriptorList(
    const std::string& rp_id,
    const std::vector<PublicKeyCredentialDescriptor>& descriptors) const {
  std::set<std::vector<uint8_t>> credential_ids;
  for (const auto& descriptor : descriptors) {
    if (descriptor.credential_type == CredentialType::kPublicKey &&
        (descriptor.transports.empty() ||
         base::Contains(descriptor.transports,
                        FidoTransportProtocol::kInternal))) {
      credential_ids.insert(descriptor.id);
    }
  }
  if (credential_ids.empty()) {
    // Don't call FindCredentialsImpl(). Given an empty |credential_ids|, it
    // returns *all* credentials for |rp_id|.
    return std::list<Credential>();
  }
  return FindCredentialsImpl(rp_id, /*user_id=*/absl::nullopt, credential_ids);
}

absl::optional<std::list<Credential>>
TouchIdCredentialStore::FindResidentCredentials(
    const std::string& rp_id) const {
  absl::optional<std::list<Credential>> credentials = FindCredentialsImpl(
      rp_id, /*user_id=*/absl::nullopt, /*credential_ids=*/{});
  if (!credentials) {
    return absl::nullopt;
  }
  credentials->remove_if([this, &rp_id](const Credential& credential) {
    auto opt_metadata = UnsealCredentialId(config_.metadata_secret, rp_id,
                                           credential.credential_id);
    if (!opt_metadata) {
      FIDO_LOG(ERROR) << "UnsealCredentialId() failed";
      return true;
    }
    return !opt_metadata->is_resident;
  });
  return credentials;
}

absl::optional<CredentialMetadata> TouchIdCredentialStore::UnsealMetadata(
    const std::string& rp_id,
    const Credential& credential) const {
  return UnsealCredentialId(config_.metadata_secret, rp_id,
                            credential.credential_id);
}

bool TouchIdCredentialStore::DeleteCredentialsForUserId(
    const std::string& rp_id,
    base::span<const uint8_t> user_id) const {
  absl::optional<std::list<Credential>> credentials =
      FindCredentialsImpl(rp_id, user_id, /*credential_ids=*/{});
  if (!credentials) {
    return false;
  }
  for (const Credential& credential : *credentials) {
    absl::optional<CredentialMetadata> metadata =
        UnsealMetadata(rp_id, credential);
    if (!metadata) {
      FIDO_LOG(ERROR) << "UnsealMetadata failed";
      continue;
    }
    if (!DeleteCredentialById(credential.credential_id)) {
      return false;
    }
  }
  return true;
}

void TouchIdCredentialStore::DeleteCredentials(base::Time created_not_before,
                                               base::Time created_not_after,
                                               base::OnceClosure callback) {
  DeleteCredentialsSync(created_not_before, created_not_after);
  std::move(callback).Run();
}

void TouchIdCredentialStore::CountCredentials(
    base::Time created_not_before,
    base::Time created_not_after,
    base::OnceCallback<void(size_t)> callback) {
  std::move(callback).Run(
      CountCredentialsSync(created_not_before, created_not_after));
}

bool TouchIdCredentialStore::DeleteCredentialsSync(
    base::Time created_not_before,
    base::Time created_not_after) {
  absl::optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
      keychain_items = QueryKeychainItemsForProfile(
          config_.keychain_access_group, config_.metadata_secret,
          created_not_before, created_not_after);
  if (!keychain_items) {
    return false;
  }

  bool result = true;
  for (const base::ScopedCFTypeRef<CFDictionaryRef>& attributes :
       *keychain_items) {
    // kSecAttrApplicationLabel stores the credential ID.
    CFDataRef credential_id_data = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes.get(), kSecAttrApplicationLabel);
    if (!credential_id_data) {
      DLOG(ERROR) << "missing application label";
      continue;
    }
    if (!DeleteCredentialById(
            base::make_span(CFDataGetBytePtr(credential_id_data),
                            CFDataGetLength(credential_id_data)))) {
      // Indicate failure, but keep deleting remaining items.
      result = false;
    }
  }
  return result;
}

size_t TouchIdCredentialStore::CountCredentialsSync(
    base::Time created_not_before,
    base::Time created_not_after) {
  absl::optional<std::vector<base::ScopedCFTypeRef<CFDictionaryRef>>>
      keychain_items = QueryKeychainItemsForProfile(
          config_.keychain_access_group, config_.metadata_secret,
          created_not_before, created_not_after);
  if (!keychain_items) {
    DLOG(ERROR) << "Failed to query credentials in keychain";
    return 0;
  }
  return keychain_items->size();
}

// static
std::vector<std::pair<Credential, CredentialMetadata>>
TouchIdCredentialStore::FindCredentialsForTesting(AuthenticatorConfig config,
                                                  std::string rp_id) {
  TouchIdCredentialStore store(std::move(config));
  absl::optional<std::list<Credential>> credentials = store.FindCredentialsImpl(
      rp_id, /*user_id=*/absl::nullopt, /*credential_ids=*/{});
  DCHECK(credentials) << "FindCredentialsImpl shouldn't fail in tests";
  std::vector<std::pair<Credential, CredentialMetadata>> result;
  for (Credential& credential : *credentials) {
    absl::optional<CredentialMetadata> metadata =
        store.UnsealMetadata(rp_id, credential);
    DCHECK(metadata) << "UnsealMetadata shouldn't fail in tests";
    result.emplace_back(std::move(credential), std::move(*metadata));
  }
  return result;
}

absl::optional<std::list<Credential>>
TouchIdCredentialStore::FindCredentialsImpl(
    const std::string& rp_id,
    absl::optional<base::span<const uint8_t>> user_id,
    const std::set<std::vector<uint8_t>>& credential_ids) const {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query =
      DefaultKeychainQuery(config_, rp_id);
  if (user_id) {
    CFDictionarySetValue(query, kSecAttrApplicationTag,
                         base::SysUTF8ToNSString(EncodeRpIdAndUserId(
                             config_.metadata_secret, rp_id, *user_id)));
  }
  if (authentication_context_) {
    CFDictionarySetValue(query, kSecUseAuthenticationContext,
                         authentication_context_);
  }
  CFDictionarySetValue(query, kSecReturnRef, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

  base::ScopedCFTypeRef<CFArrayRef> keychain_items;
  OSStatus status = Keychain::GetInstance().ItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
  if (status == errSecItemNotFound) {
    return std::list<Credential>();
  }
  if (status != errSecSuccess) {
    FIDO_LOG(ERROR) << "SecItemCopyMatching failed: "
                    << logging::DescriptionFromOSStatus(status);
    return absl::nullopt;
  }

  // Filter credentials for the RP down to |credential_ids|, unless it's
  // empty in which case all credentials should be returned.
  std::list<Credential> credentials;
  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items); ++i) {
    CFDictionaryRef attributes = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items, i));
    CFDataRef application_label = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes, kSecAttrApplicationLabel);
    if (!application_label) {
      FIDO_LOG(ERROR) << "credential with missing application label";
      return absl::nullopt;
    }
    SecKeyRef key =
        base::mac::GetValueFromDictionary<SecKeyRef>(attributes, kSecValueRef);
    if (!key) {
      FIDO_LOG(ERROR) << "credential with missing value ref";
      return absl::nullopt;
    }
    std::vector<uint8_t> credential_id(CFDataGetBytePtr(application_label),
                                       CFDataGetBytePtr(application_label) +
                                           CFDataGetLength(application_label));
    if (!credential_ids.empty() &&
        !base::Contains(credential_ids, credential_id)) {
      continue;
    }
    base::ScopedCFTypeRef<SecKeyRef> private_key(key,
                                                 base::scoped_policy::RETAIN);
    credentials.emplace_back(
        Credential{std::move(private_key), std::move(credential_id)});
  }
  return std::move(credentials);
}

bool TouchIdCredentialStore::DeleteCredentialById(
    base::span<const uint8_t> credential_id) const {
  // The sane way to delete a credential would be by SecKeyRef, like so:
  //
  //   base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
  //       CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
  //                                 &kCFTypeDictionaryKeyCallBacks,
  //                                 &kCFTypeDictionaryValueCallBacks));
  //   CFDictionarySetValue(query, kSecValueRef, sec_key_ref);
  //   OSStatus status = Keychain::GetInstance().ItemDelete(query);
  //
  // But on macOS that looks for `sec_key_ref` in the legacy keychain instead of
  // the "iOS" keychain that secure enclave credentials live in, and so the call
  // fails with `errSecItemNotFound`. macOS 10.15 added
  // `kSecUseDataProtectionKeychain` to force a query to the right keychain, but
  // we need to support older versions of macOS for now. Hence, we must delete
  // keychain items by credential ID (stored in `kSecAttrApplicationLabel`).
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(config_.keychain_access_group));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrApplicationLabel,
                       [NSData dataWithBytes:credential_id.data()
                                      length:credential_id.size()]);
  OSStatus status = Keychain::GetInstance().ItemDelete(query);
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
    return false;
  }
  return true;
}

}  // namespace device::fido::mac
