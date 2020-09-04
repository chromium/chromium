// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_store.h"

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
#include "components/device_event_log/device_event_log.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/keychain.h"

namespace device {
namespace fido {
namespace mac {

namespace {

// DefaultKeychainQuery returns a default keychain query dictionary that has
// the keychain item class, keychain access group and RP ID filled out (but
// not the credential ID). More fields can be set on the return value to
// refine the query.
base::ScopedCFTypeRef<CFMutableDictionaryRef> DefaultKeychainQuery(
    const AuthenticatorConfig& config,
    const std::string& rp_id) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(config.keychain_access_group));
  CFDictionarySetValue(
      query, kSecAttrLabel,
      base::SysUTF8ToNSString(EncodeRpId(config.metadata_secret, rp_id)));
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
  for (const base::ScopedCFTypeRef<CFDictionaryRef>& attributes :
       *keychain_items) {
    CFDataRef sec_attr_app_label = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes.get(), kSecAttrApplicationLabel);
    if (!sec_attr_app_label) {
      DLOG(ERROR) << "missing application label";
      continue;
    }
    base::ScopedCFTypeRef<CFMutableDictionaryRef> delete_query(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
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

Credential::Credential(base::ScopedCFTypeRef<SecKeyRef> private_key_,
                       std::vector<uint8_t> credential_id_)
    : private_key(std::move(private_key_)),
      credential_id(std::move(credential_id_)) {}
Credential::~Credential() = default;
Credential::Credential(Credential&& other) = default;
Credential& Credential::operator=(Credential&& other) = default;

TouchIdCredentialStore::TouchIdCredentialStore(AuthenticatorConfig config)
    : config_(std::move(config)) {}
TouchIdCredentialStore::~TouchIdCredentialStore() = default;

base::Optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
TouchIdCredentialStore::CreateCredential(
    const std::string& rp_id,
    const PublicKeyCredentialUserEntity& user,
    bool is_resident,
    SecAccessControlRef access_control) const {
  std::vector<uint8_t> credential_id = SealCredentialId(
      config_.metadata_secret, rp_id,
      CredentialMetadata::FromPublicKeyCredentialUserEntity(user, is_resident));

  base::ScopedCFTypeRef<CFMutableDictionaryRef> params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(params, kSecAttrKeyType,
                       kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(params, kSecAttrKeySizeInBits, @256);
  CFDictionarySetValue(params, kSecAttrSynchronizable, @NO);
  CFDictionarySetValue(params, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);

  base::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params =
      DefaultKeychainQuery(config_, rp_id);
  CFDictionarySetValue(params, kSecPrivateKeyAttrs, private_key_params);
  CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @YES);
  CFDictionarySetValue(private_key_params, kSecAttrAccessControl,
                       access_control);
  if (authentication_context_) {
    CFDictionarySetValue(private_key_params, kSecUseAuthenticationContext,
                         authentication_context_);
  }
  CFDictionarySetValue(private_key_params, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(EncodeRpIdAndUserId(
                           config_.metadata_secret, rp_id, user.id)));
  CFDictionarySetValue(private_key_params, kSecAttrApplicationLabel,
                       [NSData dataWithBytes:credential_id.data()
                                      length:credential_id.size()]);

  base::ScopedCFTypeRef<CFErrorRef> cferr;
  base::ScopedCFTypeRef<SecKeyRef> private_key(
      Keychain::GetInstance().KeyCreateRandomKey(params,
                                                 cferr.InitializeInto()));
  if (!private_key) {
    FIDO_LOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr;
    return base::nullopt;
  }
  base::ScopedCFTypeRef<SecKeyRef> public_key(
      Keychain::GetInstance().KeyCopyPublicKey(private_key));
  if (!public_key) {
    FIDO_LOG(ERROR) << "SecKeyCopyPublicKey failed";
    return base::nullopt;
  }

  return std::make_pair(
      Credential(std::move(private_key), std::move(credential_id)),
      std::move(public_key));
}

base::Optional<std::list<Credential>>
TouchIdCredentialStore::FindCredentialsFromCredentialDescriptorList(
    const std::string& rp_id,
    const std::vector<PublicKeyCredentialDescriptor>& descriptors) const {
  std::set<std::vector<uint8_t>> credential_ids;
  for (const auto& descriptor : descriptors) {
    if (descriptor.credential_type() == CredentialType::kPublicKey &&
        (descriptor.transports().empty() ||
         base::Contains(descriptor.transports(),
                        FidoTransportProtocol::kInternal))) {
      credential_ids.insert(descriptor.id());
    }
  }
  if (credential_ids.empty()) {
    // Don't call FindCredentialsImpl(). Given an empty |credential_ids|, it
    // returns *all* credentials for |rp_id|.
    return {};
  }
  return FindCredentialsImpl(rp_id, credential_ids);
}

base::Optional<std::list<Credential>>
TouchIdCredentialStore::FindResidentCredentials(
    const std::string& rp_id) const {
  base::Optional<std::list<Credential>> credentials =
      FindCredentialsImpl(rp_id, /*credential_ids=*/{});
  if (!credentials) {
    return base::nullopt;
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

base::Optional<CredentialMetadata> TouchIdCredentialStore::UnsealMetadata(
    const std::string& rp_id,
    const Credential& credential) const {
  return UnsealCredentialId(config_.metadata_secret, rp_id,
                            credential.credential_id);
}

bool TouchIdCredentialStore::DeleteCredentialsForUserId(
    const std::string& rp_id,
    base::span<const uint8_t> user_id) const {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query =
      DefaultKeychainQuery(config_, rp_id);
  CFDictionarySetValue(query, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(EncodeRpIdAndUserId(
                           config_.metadata_secret, rp_id, user_id)));

  OSStatus status = Keychain::GetInstance().ItemDelete(query);
  if (status != errSecSuccess && status != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
    return false;
  }
  return true;
}

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

API_AVAILABLE(macosx(10.12.2))
base::Optional<std::list<Credential>>
TouchIdCredentialStore::FindCredentialsImpl(
    const std::string& rp_id,
    const std::set<std::vector<uint8_t>>& credential_ids) const {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query =
      DefaultKeychainQuery(config_, rp_id);
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
    return base::nullopt;
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
      return base::nullopt;
    }
    SecKeyRef key =
        base::mac::GetValueFromDictionary<SecKeyRef>(attributes, kSecValueRef);
    if (!key) {
      FIDO_LOG(ERROR) << "credential with missing value ref";
      return base::nullopt;
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

}  // namespace mac
}  // namespace fido
}  // namespace device
