// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/mac/credential_store.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include <optional>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/random.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/touch_id_context.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace device::fido::mac {

namespace {

// DefaultKeychainQuery returns a default keychain query dictionary that has
// the keychain item class, keychain access group and RP ID (unless `rp_id` is
// `nullopt`) filled out. More fields can be set on the return value to refine
// the query.
NSMutableDictionary* DefaultKeychainQuery(const AuthenticatorConfig& config,
                                          std::optional<std::string> rp_id) {
  NSMutableDictionary* query = [NSMutableDictionary dictionary];
  query[CFToNSPtrCast(kSecClass)] = CFToNSPtrCast(kSecClassKey);
  query[CFToNSPtrCast(kSecAttrAccessGroup)] =
      base::SysUTF8ToNSString(config.keychain_access_group);
  if (rp_id) {
    query[CFToNSPtrCast(kSecAttrLabel)] =
        base::SysUTF8ToNSString(EncodeRpId(config.metadata_secret, *rp_id));
  }
  return query;
}

// Erase all keychain items with a creation date that is not within [not_before,
// not_after).
void FilterKeychainItemsByCreationDate(
    std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>* keychain_items,
    base::Time not_before,
    base::Time not_after) {
  std::erase_if(
      *keychain_items,
      [not_before, not_after](
          const base::apple::ScopedCFTypeRef<CFDictionaryRef>& attributes)
          -> bool {
        // If the creation date is missing for some obscure reason, treat as if
        // the date is inside the interval, i.e. keep it in the list.
        CFDateRef creation_date_cf =
            base::apple::GetValueFromDictionary<CFDateRef>(
                attributes.get(), kSecAttrCreationDate);
        if (!creation_date_cf) {
          return false;
        }
        base::Time creation_date = base::Time::FromCFAbsoluteTime(
            CFDateGetAbsoluteTime(creation_date_cf));
        return creation_date < not_before || creation_date >= not_after;
      });
}

std::optional<std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>>
QueryKeychainItemsForProfile(const std::string& keychain_access_group,
                             const std::string& metadata_secret,
                             base::Time created_not_before,
                             base::Time created_not_after) {
  // Query the keychain for all items tagged with the given access group, which
  // should in theory yield all WebAuthentication credentials (for all
  // profiles). Sadly, the kSecAttrAccessGroup filter doesn't quite work, and
  // so we also get results from the legacy keychain that are tagged with no
  // keychain access group.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> result;

  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(keychain_access_group),
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitAll),
    // Return the key reference and its attributes.
    CFToNSPtrCast(kSecReturnRef) : @YES,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
  };

  base::apple::ScopedCFTypeRef<CFArrayRef> keychain_items;
  {
    OSStatus status = crypto::AppleKeychainV2::GetInstance().ItemCopyMatching(
        NSToCFPtrCast(query),
        reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
    if (status == errSecItemNotFound) {
      DVLOG(1) << "no credentials found";
      return std::nullopt;
    }
    if (status != errSecSuccess) {
      OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
      return std::nullopt;
    }
  }

  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items.get()); ++i) {
    CFDictionaryRef attributes = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items.get(), i));
    if (!attributes) {
      DLOG(ERROR) << "unexpected result type";
      return std::nullopt;
    }

    // Skip items that don't belong to the correct keychain access group
    // because the kSecAttrAccessGroup filter is broken.
    CFStringRef attr_access_group =
        base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                         kSecAttrAccessGroup);
    if (!attr_access_group || base::SysCFStringRefToUTF8(attr_access_group) !=
                                  keychain_access_group) {
      DVLOG(1) << "missing/invalid access group";
      continue;
    }

    // If the RP ID, stored encrypted in the item's label, cannot be decrypted
    // with the given metadata secret, then the credential belongs to a
    // different profile and must be ignored.
    CFStringRef sec_attr_label =
        base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                         kSecAttrLabel);
    if (!sec_attr_label) {
      DLOG(ERROR) << "missing label";
      continue;
    }
    std::optional<std::string> opt_rp_id =
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

std::vector<uint8_t> GenerateRandomCredentialId() {
  // The length of CredentialMetadata::Version::kV3 credentials. Older
  // credentials use the sealed metadata as the ID, which varies in size.
  constexpr size_t kCredentialIdLength = 32;
  return crypto::RandBytesAsVector(kCredentialIdLength);
}

}  // namespace

Credential::Credential(base::apple::ScopedCFTypeRef<SecKeyRef> private_key,
                       std::vector<uint8_t> credential_id,
                       CredentialMetadata metadata,
                       std::string rp_id)
    : private_key(std::move(private_key)),
      credential_id(std::move(credential_id)),
      metadata(std::move(metadata)),
      rp_id(rp_id) {}

Credential::Credential(const Credential& other) = default;

Credential::Credential(Credential&& other) = default;

Credential& Credential::operator=(const Credential& other) = default;

Credential& Credential::operator=(Credential&& other) = default;

Credential::~Credential() = default;

bool Credential::operator==(const Credential& other) const {
  return CFEqual(private_key.get(), other.private_key.get()) &&
         credential_id == other.credential_id && metadata == other.metadata;
}

bool Credential::RequiresUvForSignature() const {
  return metadata.version < CredentialMetadata::Version::kV4;
}

struct TouchIdCredentialStore::ObjCStorage {
  LAContext* __strong authentication_context;
};

TouchIdCredentialStore::TouchIdCredentialStore(AuthenticatorConfig config)
    : config_(std::move(config)),
      objc_storage_(std::make_unique<ObjCStorage>()) {}
TouchIdCredentialStore::~TouchIdCredentialStore() = default;

void TouchIdCredentialStore::SetAuthenticationContext(
    LAContext* authentication_context) {
  objc_storage_->authentication_context = authentication_context;
}

std::optional<std::pair<Credential, base::apple::ScopedCFTypeRef<SecKeyRef>>>
TouchIdCredentialStore::CreateCredential(
    const std::string& rp_id,
    const PublicKeyCredentialUserEntity& user,
    Discoverable discoverable) const {
  NSMutableDictionary* private_key_params = [NSMutableDictionary dictionary];
  private_key_params[CFToNSPtrCast(kSecAttrIsPermanent)] = @YES;

  // The credential can only be used for signing, and the device needs to be in
  // an unlocked state.
  auto flags = kSecAccessControlPrivateKeyUsage;
  base::apple::ScopedCFTypeRef<SecAccessControlRef> access_control(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
          flags, /*error=*/nullptr));
  private_key_params[CFToNSPtrCast(kSecAttrAccessControl)] =
      (__bridge id)access_control.get();
  if (objc_storage_->authentication_context) {
    private_key_params[CFToNSPtrCast(kSecUseAuthenticationContext)] =
        objc_storage_->authentication_context;
  }

  auto credential_metadata =
      CredentialMetadata::FromPublicKeyCredentialUserEntity(
          user, discoverable == kDiscoverable);
  const std::vector<uint8_t> sealed_metadata = SealCredentialMetadata(
      config_.metadata_secret, rp_id, credential_metadata);

  const std::vector<uint8_t> credential_id = GenerateRandomCredentialId();

  NSDictionary* params = @{
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(config_.keychain_access_group),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
    CFToNSPtrCast(kSecAttrTokenID) :
        CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
    CFToNSPtrCast(kSecAttrLabel) :
        base::SysUTF8ToNSString(EncodeRpId(config_.metadata_secret, rp_id)),
    CFToNSPtrCast(kSecAttrApplicationTag) :
        [NSData dataWithBytes:sealed_metadata.data()
                       length:sealed_metadata.size()],
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:credential_id.data() length:credential_id.size()],
    CFToNSPtrCast(kSecPrivateKeyAttrs) : private_key_params,
  };

  base::apple::ScopedCFTypeRef<CFErrorRef> cferr;
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key =
      crypto::AppleKeychainV2::GetInstance().KeyCreateRandomKey(
          NSToCFPtrCast(params), cferr.InitializeInto());
  if (!private_key) {
    FIDO_LOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr.get();
    return std::nullopt;
  }
  base::apple::ScopedCFTypeRef<SecKeyRef> public_key(
      crypto::AppleKeychainV2::GetInstance().KeyCopyPublicKey(
          private_key.get()));
  if (!public_key) {
    FIDO_LOG(ERROR) << "SecKeyCopyPublicKey failed";
    return std::nullopt;
  }

  return std::make_pair(
      Credential(std::move(private_key), std::move(credential_id),
                 std::move(credential_metadata), std::move(rp_id)),
      std::move(public_key));
}

std::optional<std::pair<Credential, base::apple::ScopedCFTypeRef<SecKeyRef>>>
TouchIdCredentialStore::CreateCredentialLegacyCredentialForTesting(
    CredentialMetadata::Version version,
    const std::string& rp_id,
    const PublicKeyCredentialUserEntity& user,
    Discoverable discoverable) const {
  DCHECK(discoverable == Discoverable::kNonDiscoverable ||
         version > CredentialMetadata::Version::kV0);

  const bool is_discoverable = discoverable == Discoverable::kDiscoverable;
  std::vector<uint8_t> credential_id = SealLegacyCredentialIdForTestingOnly(
      version, config_.metadata_secret, rp_id, user.id, user.name.value_or(""),
      user.display_name.value_or(""), is_discoverable);
  std::optional<CredentialMetadata> metadata =
      UnsealMetadataFromLegacyCredentialId(config_.metadata_secret, rp_id,
                                           credential_id);
  DCHECK(metadata);

  NSMutableDictionary* private_key_params = [NSMutableDictionary dictionary];
  private_key_params[CFToNSPtrCast(kSecAttrIsPermanent)] = @YES;

  // Credential can only be used when the device is unlocked. Private key is
  // available for signing after user authorization with biometrics or
  // password.
  base::apple::ScopedCFTypeRef<SecAccessControlRef> access_control(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
          kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence,
          /*error=*/nullptr));
  private_key_params[CFToNSPtrCast(kSecAttrAccessControl)] =
      (__bridge id)access_control.get();
  if (objc_storage_->authentication_context) {
    private_key_params[CFToNSPtrCast(kSecUseAuthenticationContext)] =
        objc_storage_->authentication_context;
  }

  NSDictionary* params = @{
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(config_.keychain_access_group),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
    CFToNSPtrCast(kSecAttrTokenID) :
        CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
    CFToNSPtrCast(kSecAttrLabel) :
        base::SysUTF8ToNSString(EncodeRpId(config_.metadata_secret, rp_id)),
    CFToNSPtrCast(kSecAttrApplicationTag) : base::SysUTF8ToNSString(
        EncodeRpIdAndUserIdDeprecated(config_.metadata_secret, rp_id, user.id)),
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:credential_id.data() length:credential_id.size()],
    CFToNSPtrCast(kSecPrivateKeyAttrs) : private_key_params,
  };

  base::apple::ScopedCFTypeRef<CFErrorRef> cferr;
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key =
      crypto::AppleKeychainV2::GetInstance().KeyCreateRandomKey(
          NSToCFPtrCast(params), cferr.InitializeInto());
  if (!private_key) {
    FIDO_LOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr.get();
    return std::nullopt;
  }
  base::apple::ScopedCFTypeRef<SecKeyRef> public_key(
      crypto::AppleKeychainV2::GetInstance().KeyCopyPublicKey(
          private_key.get()));
  if (!public_key) {
    FIDO_LOG(ERROR) << "SecKeyCopyPublicKey failed";
    return std::nullopt;
  }

  return std::make_pair(
      Credential(std::move(private_key), std::move(credential_id),
                 std::move(*metadata), std::move(rp_id)),
      std::move(public_key));
}

std::optional<std::list<Credential>>
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
  return FindCredentialsImpl(rp_id, credential_ids);
}

std::optional<std::list<Credential>>
TouchIdCredentialStore::FindResidentCredentials(
    const std::optional<std::string>& rp_id) const {
  std::optional<std::list<Credential>> credentials =
      FindCredentialsImpl(rp_id, /*credential_ids=*/{});
  if (!credentials) {
    return std::nullopt;
  }
  credentials->remove_if([](const Credential& credential) {
    return !credential.metadata.is_resident;
  });
  return credentials;
}

bool TouchIdCredentialStore::DeleteCredentialsForUserId(
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id) const {
  std::optional<std::list<Credential>> credentials =
      FindCredentialsImpl(rp_id, /*credential_ids=*/{});
  if (!credentials) {
    return false;
  }
  for (const Credential& credential : *credentials) {
    if (user_id != credential.metadata.user_id) {
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
  std::optional<std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>>
      keychain_items = QueryKeychainItemsForProfile(
          config_.keychain_access_group, config_.metadata_secret,
          created_not_before, created_not_after);
  if (!keychain_items) {
    return false;
  }

  bool result = true;
  for (const base::apple::ScopedCFTypeRef<CFDictionaryRef>& attributes :
       *keychain_items) {
    // kSecAttrApplicationLabel stores the credential ID.
    CFDataRef credential_id_data =
        base::apple::GetValueFromDictionary<CFDataRef>(
            attributes.get(), kSecAttrApplicationLabel);
    if (!credential_id_data) {
      DLOG(ERROR) << "missing application label";
      continue;
    }
    if (!DeleteCredentialById(base::apple::CFDataToSpan(credential_id_data))) {
      // Indicate failure, but keep deleting remaining items.
      result = false;
    }
  }
  return result;
}

size_t TouchIdCredentialStore::CountCredentialsSync(
    base::Time created_not_before,
    base::Time created_not_after) {
  std::optional<std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>>
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
std::vector<Credential> TouchIdCredentialStore::FindCredentialsForTesting(
    AuthenticatorConfig config,
    std::string rp_id) {
  TouchIdCredentialStore store(std::move(config));
  std::optional<std::list<Credential>> credentials =
      store.FindCredentialsImpl(rp_id, /*credential_ids=*/{});
  DCHECK(credentials) << "FindCredentialsImpl shouldn't fail in tests";
  std::vector<Credential> result;
  for (Credential& credential : *credentials) {
    result.emplace_back(std::move(credential));
  }
  return result;
}

std::optional<std::list<Credential>>
TouchIdCredentialStore::FindCredentialsImpl(
    const std::optional<std::string>& rp_id,
    const std::set<std::vector<uint8_t>>& credential_ids) const {
  // Query all credentials for the RP. Filtering for `rp_id` here ensures we
  // don't retrieve credentials for other profiles, because their
  // `kSecAttrLabel` attribute wouldn't match the encoded RP ID.
  NSMutableDictionary* query = DefaultKeychainQuery(config_, rp_id);
  if (objc_storage_->authentication_context) {
    query[CFToNSPtrCast(kSecUseAuthenticationContext)] =
        objc_storage_->authentication_context;
  }
  query[CFToNSPtrCast(kSecReturnRef)] = @YES;
  query[CFToNSPtrCast(kSecReturnAttributes)] = @YES;
  query[CFToNSPtrCast(kSecMatchLimit)] = CFToNSPtrCast(kSecMatchLimitAll);

  base::apple::ScopedCFTypeRef<CFArrayRef> keychain_items;
  OSStatus status = crypto::AppleKeychainV2::GetInstance().ItemCopyMatching(
      NSToCFPtrCast(query),
      reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
  if (status == errSecItemNotFound) {
    return std::list<Credential>();
  }
  if (status != errSecSuccess) {
    FIDO_LOG(ERROR) << "SecItemCopyMatching failed: "
                    << logging::DescriptionFromOSStatus(status);
    return std::nullopt;
  }

  // Filter credentials for the RP down to |credential_ids|, unless it's
  // empty in which case all credentials should be returned.
  std::list<Credential> credentials;
  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items.get()); ++i) {
    CFDictionaryRef attributes = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items.get(), i));
    if (!attributes) {
      FIDO_LOG(ERROR) << "credential with missing attributes";
      return std::nullopt;
    }
    // Skip items that don't belong to the correct keychain access group
    // because the kSecAttrAccessGroup filter is broken.
    CFStringRef attr_access_group =
        base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                         kSecAttrAccessGroup);
    if (!attr_access_group) {
      continue;
    }
    std::string rp_id_value;
    if (!rp_id) {
      CFStringRef sec_attr_label =
          base::apple::GetValueFromDictionary<CFStringRef>(attributes,
                                                           kSecAttrLabel);
      if (!sec_attr_label) {
        FIDO_LOG(ERROR) << "credential with missing kSecAttrLabel_data";
        continue;
      }
      std::optional<std::string> opt_rp_id = DecodeRpId(
          config_.metadata_secret, base::SysCFStringRefToUTF8(sec_attr_label));
      if (!opt_rp_id) {
        FIDO_LOG(ERROR) << "could not decode RP ID";
        continue;
      }
      rp_id_value = *opt_rp_id;
    } else {
      rp_id_value = *rp_id;
    }
    CFDataRef application_label =
        base::apple::GetValueFromDictionary<CFDataRef>(
            attributes, kSecAttrApplicationLabel);
    if (!application_label) {
      FIDO_LOG(ERROR) << "credential with missing application label";
      return std::nullopt;
    }
    auto credential_id_span = base::apple::CFDataToSpan(application_label);
    const std::vector<uint8_t> credential_id(credential_id_span.begin(),
                                             credential_id_span.end());
    if (!credential_ids.empty() &&
        !base::Contains(credential_ids, credential_id)) {
      continue;
    }

    // Decode `CredentialMetadata` from the `kSecAttrApplicationTag` attribute
    // for V3 credentials, or from the credential ID for version <= 2.
    std::optional<CredentialMetadata> metadata;
    CFDataRef application_tag_ref =
        base::apple::GetValueFromDictionary<CFDataRef>(attributes,
                                                       kSecAttrApplicationTag);
    // On version < 3 credentials, kSecAttrApplicationTag is a CFStringRef,
    // which means `application_tag_ref` would be nullptr.
    if (application_tag_ref) {
      auto application_tag = base::apple::CFDataToSpan(application_tag_ref);
      metadata = UnsealMetadataFromApplicationTag(config_.metadata_secret,
                                                  rp_id_value, application_tag);
    } else {
      metadata = UnsealMetadataFromLegacyCredentialId(
          config_.metadata_secret, rp_id_value, credential_id);
    }
    if (!metadata) {
      FIDO_LOG(ERROR) << "credential with invalid metadata";
      return std::nullopt;
    }

    SecKeyRef key = base::apple::GetValueFromDictionary<SecKeyRef>(
        attributes, kSecValueRef);
    if (!key) {
      FIDO_LOG(ERROR) << "credential with missing value ref";
      return std::nullopt;
    }
    base::apple::ScopedCFTypeRef<SecKeyRef> private_key(
        key, base::scoped_policy::RETAIN);

    credentials.emplace_back(std::move(private_key), std::move(credential_id),
                             std::move(*metadata), std::move(rp_id_value));
  }
  return std::move(credentials);
}

bool TouchIdCredentialStore::DeleteCredentialById(
    base::span<const uint8_t> credential_id) const {
  // The reasonable way to delete a credential would be by SecKeyRef, like so:
  //
  //   NSDictionary* query = @{
  //     CFToNSPtrCast(kSecValueRef) : (__bridge id)sec_key_ref,
  //   };
  //   OSStatus status =
  //       AppleKeychainV2::GetInstance().ItemDelete(NSToCFPtrCast(query));
  //
  // But on macOS that looks for `sec_key_ref` in the legacy keychain instead of
  // the "iOS" keychain that secure enclave credentials live in, and so the call
  // fails with `errSecItemNotFound`. macOS 10.15 added
  // `kSecUseDataProtectionKeychain` to force a query to the right keychain, but
  // we need to support older versions of macOS for now. Hence, we must delete
  // keychain items by credential ID (stored in `kSecAttrApplicationLabel`).
  // TODO(https://crbug.com/40275358): Update to this better approach that
  // requires 10.15 now that Chromium requires 10.15.
  NSDictionary* query = @{
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(config_.keychain_access_group),
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:credential_id.data() length:credential_id.size()],
  };

  OSStatus status =
      crypto::AppleKeychainV2::GetInstance().ItemDelete(NSToCFPtrCast(query));
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
    return false;
  }
  return true;
}

bool TouchIdCredentialStore::UpdateCredential(
    base::span<uint8_t> credential_id_span,
    const std::string& username) {
  std::vector<uint8_t> credential_id =
      fido_parsing_utils::Materialize(credential_id_span);

  std::optional<std::list<Credential>> credentials = FindCredentialsImpl(
      /*rp_id=*/std::nullopt, {credential_id});
  if (!credentials) {
    FIDO_LOG(ERROR) << "no credentials found";
    return false;
  }

  NSData* sealed_metadata_data = nil;
  for (Credential& credential : *credentials) {
    if (credential.credential_id == credential_id) {
      credential.metadata.user_name = username;
      std::vector<uint8_t> sealed_metadata = SealCredentialMetadata(
          config_.metadata_secret, credential.rp_id, credential.metadata);
      sealed_metadata_data = [NSData dataWithBytes:sealed_metadata.data()
                                            length:sealed_metadata.size()];
      break;
    }
  }
  if (!sealed_metadata_data) {
    FIDO_LOG(ERROR) << "no credential with matching credential_id";
    return false;
  }

  NSDictionary* query = @{
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(config_.keychain_access_group),
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:credential_id.data() length:credential_id.size()],
  };
  NSDictionary* params =
      @{CFToNSPtrCast(kSecAttrApplicationTag) : sealed_metadata_data};
  OSStatus status = crypto::AppleKeychainV2::GetInstance().ItemUpdate(
      NSToCFPtrCast(query), NSToCFPtrCast(params));
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemUpdate failed";
    return false;
  }
  return true;
}

}  // namespace device::fido::mac
