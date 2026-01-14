// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#include <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/scoped_policy.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "crypto/apple/keychain_util.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/unexportable_key_mac.h"
#include "crypto/keypair.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key_metrics.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace crypto::apple {

namespace {

// The value of the kSecAttrLabel when generating the key. The documentation
// claims this should be a user-visible label, but there does not exist any UI
// that shows this value. Therefore, it is left untranslated.
constexpr char kAttrLabel[] = "Chromium unexportable key";

// Logs `status` to an error histogram capturing that `operation` failed for a
// key backed by Secure Enclave.
void LogKeychainOperationError(TPMOperation operation, OSStatus status) {
  static constexpr char kKeyErrorStatusHistogramFormat[] =
      "Crypto.SecureEnclaveOperation.Mac.%s.Error";
  base::UmaHistogramSparse(
      base::StringPrintf(kKeyErrorStatusHistogramFormat,
                         OperationToString(operation).c_str()),
      status);
}

// Logs `error` to an error histogram capturing that `operation` failed for a
// key backed by Secure Enclave. Defaults to `errSecCoreFoundationUnknown` if
// `error` is missing.
void LogKeychainOperationError(
    TPMOperation operation,
    base::apple::ScopedCFTypeRef<CFErrorRef>& error) {
  LogKeychainOperationError(operation, error ? CFErrorGetCode(error.get())
                                             : errSecCoreFoundationUnknown);
}

// Returns a vector of keychain items matching the given attributes or an
// OSStatus error code in case of failure.
base::expected<std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>,
               OSStatus>
FindUnexportableKeys(NSString* access_group,
                     base::span<const uint8_t> wrapped_key = {},
                     LAContext* lacontext = nullptr) {
  NSMutableDictionary* query = [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrAccessGroup) : access_group,
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitAll),
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
    CFToNSPtrCast(kSecReturnRef) : @YES,
  }];

  if (!wrapped_key.empty()) {
    query[CFToNSPtrCast(kSecAttrApplicationLabel)] =
        [NSData dataWithBytes:wrapped_key.data() length:wrapped_key.size()];
  }

  if (lacontext) {
    query[CFToNSPtrCast(kSecUseAuthenticationContext)] = lacontext;
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  switch (OSStatus status =
              crypto::apple::KeychainV2::GetInstance().ItemCopyMatching(
                  NSToCFPtrCast(query), result.InitializeInto());
          status) {
    case errSecSuccess:
      break;
    case errSecItemNotFound:
      // `errSecItemNotFound` is expected if no keys could be found. Return an
      // empty vector instead of propagating the error.
      return {};
    default:
      return base::unexpected(status);
  }

  CFArrayRef array = base::apple::CFCast<CFArrayRef>(result.get());
  if (!array) {
    return {};
  }

  const CFIndex count = CFArrayGetCount(array);
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items;
  items.reserve(count);
  for (CFIndex i = 0; i < count; ++i) {
    if (CFDictionaryRef dict = base::apple::CFCast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(array, i))) {
      // Retain the dictionary so it survives after the array is released.
      items.emplace_back(dict, base::scoped_policy::RETAIN);
    }
  }
  return items;
}

std::string GetApplicationTag(CFDictionaryRef key_attributes) {
  // kSecAttrApplicationTag can be CFStringRef for legacy credentials and
  // CFDataRef for new ones, hence querying both.
  if (CFStringRef str = base::apple::GetValueFromDictionary<CFStringRef>(
          key_attributes, kSecAttrApplicationTag)) {
    return base::SysCFStringRefToUTF8(str);
  }

  if (CFDataRef data = base::apple::GetValueFromDictionary<CFDataRef>(
          key_attributes, kSecAttrApplicationTag)) {
    return std::string(base::as_string_view(base::apple::CFDataToSpan(data)));
  }

  return "";
}

enum class ApplicationTagMatching {
  kEquals,
  kStartsWith,
};

size_t FilterKeysByApplicationTag(
    std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>& keys,
    std::string_view application_tag,
    ApplicationTagMatching matching) {
  auto key_matches = [&](const auto& key) {
    const std::string key_tag = GetApplicationTag(key.get());
    switch (matching) {
      case ApplicationTagMatching::kEquals:
        return key_tag == application_tag;
      case ApplicationTagMatching::kStartsWith:
        return key_tag.starts_with(application_tag);
    }
  };

  // Remove keys that don't match `application_tag` according to `matching`.
  return std::erase_if(keys, std::not_fn(key_matches));
}

// Deletes a key from the key chain specified by `key_attributes`. Returns
// whether the operation succeeded.
bool DeleteKey(CFDictionaryRef key_attributes) {
  NSMutableDictionary* delete_query =
      [NSMutableDictionary dictionaryWithDictionary:@{
        CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
        CFToNSPtrCast(kSecAttrKeyType) :
            CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
      }];

  // Iterate and copy only if the value exists
  for (id key in @[
         CFToNSPtrCast(kSecAttrAccessGroup),
         CFToNSPtrCast(kSecAttrApplicationLabel),
         CFToNSPtrCast(kSecAttrApplicationTag),
       ]) {
    if (id value = CFToNSPtrCast(key_attributes)[key]) {
      delete_query[key] = value;
    }
  }

  if (OSStatus status = crypto::apple::KeychainV2::GetInstance().ItemDelete(
          NSToCFPtrCast(delete_query));
      status != errSecSuccess) {
    LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
    return false;
  }

  return true;
}

base::Time GetCreationTimeFromAttributes(CFDictionaryRef key_attributes) {
  const auto date = base::apple::GetValueFromDictionary<CFDateRef>(
      key_attributes, kSecAttrCreationDate);
  return date ? base::Time::FromCFAbsoluteTime(CFDateGetAbsoluteTime(date))
              : base::Time::Now();
}

std::optional<std::vector<uint8_t>> Convertx963ToDerSpki(
    base::span<const uint8_t> x962) {
  std::optional<crypto::keypair::PublicKey> imported =
      crypto::keypair::PublicKey::FromEcP256Point(x962);
  if (!imported) {
    LOG(ERROR) << "P-256 public key is not on curve";
    return std::nullopt;
  }
  return imported->ToSubjectPublicKeyInfo();
}

// UnexportableSigningKeyMac is an implementation of the UnexportableSigningKey
// interface on top of Apple's Secure Enclave.
class UnexportableSigningKeyMac : public StatefulUnexportableSigningKey {
 public:
  explicit UnexportableSigningKeyMac(CFDictionaryRef key_attributes)
      : UnexportableSigningKeyMac(
            base::apple::ScopedCFTypeRef<SecKeyRef>(
                base::apple::GetValueFromDictionary<SecKeyRef>(key_attributes,
                                                               kSecValueRef),
                base::scoped_policy::RETAIN),
            key_attributes) {}

  UnexportableSigningKeyMac(base::apple::ScopedCFTypeRef<SecKeyRef> key,
                            CFDictionaryRef key_attributes)
      : key_(std::move(key)),
        application_label_(base::ToVector(base::apple::CFDataToSpan(
            base::apple::GetValueFromDictionary<CFDataRef>(
                key_attributes,
                kSecAttrApplicationLabel)))),
        application_tag_(GetApplicationTag(key_attributes)),
        creation_time_(GetCreationTimeFromAttributes(key_attributes)) {
    base::apple::ScopedCFTypeRef<SecKeyRef> public_key(
        crypto::apple::KeychainV2::GetInstance().KeyCopyPublicKey(key_.get()));
    base::apple::ScopedCFTypeRef<CFDataRef> x962_bytes(
        crypto::apple::KeychainV2::GetInstance().KeyCopyExternalRepresentation(
            public_key.get(), /*error=*/nil));
    CHECK(x962_bytes);
    base::span<const uint8_t> x962_span =
        base::apple::CFDataToSpan(x962_bytes.get());
    public_key_spki_ = *Convertx963ToDerSpki(x962_span);
  }

  ~UnexportableSigningKeyMac() override = default;

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return public_key_spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    return application_label_;
  }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    SecKeyAlgorithm algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
    if (!SecKeyIsAlgorithmSupported(key_.get(), kSecKeyOperationTypeSign,
                                    algorithm)) {
      // This is not expected to happen, but it could happen if e.g. the key had
      // been replaced by a key of a different type with the same label.
      LOG(ERROR) << "Key does not support ECDSA algorithm";
      return std::nullopt;
    }

    NSData* nsdata = [NSData dataWithBytes:data.data() length:data.size()];
    base::apple::ScopedCFTypeRef<CFErrorRef> error;
    base::apple::ScopedCFTypeRef<CFDataRef> signature(
        crypto::apple::KeychainV2::GetInstance().KeyCreateSignature(
            key_.get(), algorithm, NSToCFPtrCast(nsdata),
            error.InitializeInto()));
    if (!signature) {
      LOG(ERROR) << "Error signing with key: " << error.get();
      LogKeychainOperationError(TPMOperation::kMessageSigning, error);
      return std::nullopt;
    }
    return base::ToVector(base::apple::CFDataToSpan(signature.get()));
  }

  bool IsHardwareBacked() const override { return true; }

  SecKeyRef GetSecKeyRef() const override { return key_.get(); }

  StatefulUnexportableSigningKey* AsStatefulUnexportableSigningKey()
      LIFETIME_BOUND override {
    return this;
  }

  // StatefulUnexportableSigningKey:
  std::string GetKeyTag() const override { return application_tag_; }

  base::Time GetCreationTime() const override { return creation_time_; }

 private:
  // The wrapped key as returned by the Keychain API.
  const base::apple::ScopedCFTypeRef<SecKeyRef> key_;

  // The MacOS Keychain API sets the application label to the hash of the public
  // key. We use this to uniquely identify the key in lieu of a wrapped private
  // key.
  const std::vector<uint8_t> application_label_;

  // The application tag of the key.
  const std::string application_tag_;

  // The creation time of the key.
  const base::Time creation_time_;

  // The public key in DER SPKI format.
  std::vector<uint8_t> public_key_spki_;
};

}  // namespace

struct UnexportableKeyProviderMac::ObjCStorage {
  NSString* __strong keychain_access_group_;
  NSString* __strong application_tag_;
};

UnexportableKeyProviderMac::UnexportableKeyProviderMac(Config config)
    : access_control_(config.access_control),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->keychain_access_group_ =
      base::SysUTF8ToNSString(std::move(config.keychain_access_group));
  objc_storage_->application_tag_ =
      base::SysUTF8ToNSString(std::move(config.application_tag));
}
UnexportableKeyProviderMac::~UnexportableKeyProviderMac() = default;

std::optional<SignatureVerifier::SignatureAlgorithm>
UnexportableKeyProviderMac::SelectAlgorithm(
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  return std::ranges::contains(acceptable_algorithms,
                               SignatureVerifier::ECDSA_SHA256)
             ? std::make_optional(SignatureVerifier::ECDSA_SHA256)
             : std::nullopt;
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderMac::GenerateSigningKeySlowly(
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  return GenerateSigningKeySlowly(acceptable_algorithms, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderMac::GenerateSigningKeySlowly(
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    LAContext* lacontext) {
  // The Secure Enclave only supports elliptic curve keys.
  if (!SelectAlgorithm(acceptable_algorithms)) {
    return nullptr;
  }

  // Generate the key pair.
  SecAccessControlCreateFlags control_flags = kSecAccessControlPrivateKeyUsage;
  switch (access_control_) {
    case UnexportableKeyProvider::Config::AccessControl::kUserPresence:
      // kSecAccessControlUserPresence is documented[1] (at the time of
      // writing) to be "equivalent to specifying kSecAccessControlBiometryAny,
      // kSecAccessControlOr, and kSecAccessControlDevicePasscode". This is
      // incorrect because including kSecAccessControlBiometryAny causes key
      // creation to fail if biometrics are supported but not enrolled. It also
      // appears to support Apple Watch confirmation, but this isn't documented
      // (and kSecAccessControlWatch is deprecated as of macOS 15).
      //
      // Reported as FB14040169.
      //
      // [1] https://developer.apple.com/documentation/security/
      //     secaccesscontrolcreateflags/ksecaccesscontroluserpresence
      control_flags |= kSecAccessControlUserPresence;
      break;
    case UnexportableKeyProvider::Config::AccessControl::kNone:
      // No additional flag.
      break;
  }
  base::apple::ScopedCFTypeRef<SecAccessControlRef> access(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
          control_flags,
          /*error=*/nil));
  CHECK(access);

  NSMutableDictionary* key_attributes =
      [NSMutableDictionary dictionaryWithDictionary:@{
        CFToNSPtrCast(kSecAttrIsPermanent) : @YES,
        CFToNSPtrCast(kSecAttrAccessControl) : (__bridge id)access.get(),
      }];
  if (lacontext) {
    key_attributes[CFToNSPtrCast(kSecUseAuthenticationContext)] = lacontext;
  }

  NSDictionary* attributes = @{
    CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
    CFToNSPtrCast(kSecAttrTokenID) :
        CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
    CFToNSPtrCast(kSecPrivateKeyAttrs) : key_attributes,
    CFToNSPtrCast(kSecAttrAccessGroup) : objc_storage_->keychain_access_group_,
    CFToNSPtrCast(kSecAttrLabel) : base::SysUTF8ToNSString(kAttrLabel),
    CFToNSPtrCast(kSecAttrApplicationTag) : objc_storage_->application_tag_,
  };

  base::apple::ScopedCFTypeRef<CFErrorRef> error;
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key(
      crypto::apple::KeychainV2::GetInstance().KeyCreateRandomKey(
          NSToCFPtrCast(attributes), error.InitializeInto()));
  if (!private_key) {
    LOG(ERROR) << "Could not create private key: " << error.get();
    LogKeychainOperationError(TPMOperation::kNewKeyCreation, error);
    return nullptr;
  }
  base::apple::ScopedCFTypeRef<CFDictionaryRef> key_metadata =
      crypto::apple::KeychainV2::GetInstance().KeyCopyAttributes(
          private_key.get());
  return std::make_unique<UnexportableSigningKeyMac>(std::move(private_key),
                                                     key_metadata.get());
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderMac::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return FromWrappedSigningKeySlowly(wrapped_key, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderMac::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key,
    LAContext* lacontext) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> key_dicts,
      FindUnexportableKeys(objc_storage_->keychain_access_group_, wrapped_key,
                           lacontext),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kWrappedKeyExport, status);
        return nullptr;
      });

  if (key_dicts.empty()) {
    return nullptr;
  }

  // Try to find an exact match for the desired `application_tag`, return if
  // found.
  if (auto it = std::ranges::find(
          key_dicts, base::SysNSStringToUTF8(objc_storage_->application_tag_),
          [](const auto& key_dict) {
            return GetApplicationTag(key_dict.get());
          });
      it != key_dicts.end()) {
    return std::make_unique<UnexportableSigningKeyMac>(it->get());
  }

  // Lastly, if there are matching entries for `wrapped_key`, but no exact match
  // for `application_tag`, make a copy of the first partial match, explicitly
  // set the application_tag, and write it to the keychain. Return this key if
  // no error occurred.
  NSMutableDictionary* key_attributes = [NSMutableDictionary
      dictionaryWithDictionary:CFToNSPtrCast(key_dicts.front().get())];
  key_attributes[CFToNSPtrCast(kSecAttrApplicationTag)] =
      objc_storage_->application_tag_;
  if (lacontext) {
    key_attributes[CFToNSPtrCast(kSecUseAuthenticationContext)] = lacontext;
  }

  if (OSStatus status = crypto::apple::KeychainV2::GetInstance().ItemAdd(
          NSToCFPtrCast(key_attributes),
          /*result=*/nil);
      status != errSecSuccess) {
    LogKeychainOperationError(TPMOperation::kWrappedKeyExport, status);
    return nullptr;
  }

  return std::make_unique<UnexportableSigningKeyMac>(
      NSToCFPtrCast(key_attributes));
}

StatefulUnexportableKeyProvider*
UnexportableKeyProviderMac::AsStatefulUnexportableKeyProvider() {
  return this;
}

std::optional<std::vector<std::unique_ptr<UnexportableSigningKey>>>
UnexportableKeyProviderMac::GetAllSigningKeysSlowly() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys(objc_storage_->keychain_access_group_),
      [](OSStatus) { return std::nullopt; });

  FilterKeysByApplicationTag(
      keys, base::SysNSStringToUTF8(objc_storage_->application_tag_),
      ApplicationTagMatching::kStartsWith);

  return base::ToVector(
      keys, [](const auto& key) -> std::unique_ptr<UnexportableSigningKey> {
        return std::make_unique<UnexportableSigningKeyMac>(key.get());
      });
}

bool UnexportableKeyProviderMac::DeleteSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  return static_cast<bool>(DeleteSigningKeySlowlyImpl(wrapped_key).value_or(0));
}

std::optional<size_t> UnexportableKeyProviderMac::DeleteSigningKeysSlowly(
    base::span<const base::span<const uint8_t>> wrapped_keys) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // Short-circuit for empty and single-element spans.
  //
  // NOTE: While we could also repeatedly call DeleteSigningKeySlowlyImpl() in
  // the multi-key case, each of these calls would involve an OS call to the
  // keychain. This is likely less efficient than a single keychain query and
  // then processing the results in-memory.
  switch (wrapped_keys.size()) {
    case 0:
      return 0;
    case 1:
      return DeleteSigningKeySlowlyImpl(wrapped_keys.front());
  }

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys(objc_storage_->keychain_access_group_),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  FilterKeysByApplicationTag(
      keys, base::SysNSStringToUTF8(objc_storage_->application_tag_),
      ApplicationTagMatching::kStartsWith);

  if (keys.empty()) {
    return 0;
  }

  // Remove all keys that don't match any of the provided wrapped keys.
  const absl::flat_hash_set<base::span<const uint8_t>> keys_to_delete(
      wrapped_keys.begin(), wrapped_keys.end());
  std::erase_if(keys, [&](const auto& key) {
    return !keys_to_delete.contains(base::apple::CFDataToSpan(
        base::apple::GetValueFromDictionary<CFDataRef>(
            key.get(), kSecAttrApplicationLabel)));
  });

  return std::ranges::count_if(
      keys, [&](const auto& key) { return DeleteKey(key.get()); });
}

std::optional<size_t> UnexportableKeyProviderMac::DeleteAllSigningKeysSlowly() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys(objc_storage_->keychain_access_group_),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  const std::string application_tag =
      base::SysNSStringToUTF8(objc_storage_->application_tag_);
  FilterKeysByApplicationTag(keys, application_tag,
                             // As a safeguard, don't perform prefix matching if
                             // the application_tag used in the query was empty.
                             application_tag.empty()
                                 ? ApplicationTagMatching::kEquals
                                 : ApplicationTagMatching::kStartsWith);

  return std::ranges::count_if(
      keys, [&](const auto& key) { return DeleteKey(key.get()); });
}

std::optional<size_t> UnexportableKeyProviderMac::DeleteSigningKeySlowlyImpl(
    base::span<const uint8_t> wrapped_key) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys(objc_storage_->keychain_access_group_, wrapped_key),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  FilterKeysByApplicationTag(
      keys, base::SysNSStringToUTF8(objc_storage_->application_tag_),
      ApplicationTagMatching::kStartsWith);

  return std::ranges::count_if(
      keys, [](const auto& key) { return DeleteKey(key.get()); });
}

std::unique_ptr<UnexportableKeyProviderMac> GetUnexportableKeyProviderMac(
    UnexportableKeyProvider::Config config) {
  CHECK(!config.keychain_access_group.empty())
      << "A keychain access group must be set when using unexportable keys on "
         "macOS";
  if (![crypto::apple::KeychainV2::GetInstance().GetTokenIDs()
          containsObject:CFToNSPtrCast(kSecAttrTokenIDSecureEnclave)]) {
    return nullptr;
  }
  // Inspecting the binary for the entitlement is not available on iOS, assume
  // it is available.
#if !BUILDFLAG(IS_IOS)
  if (!crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
          config.keychain_access_group)) {
    return nullptr;
  }
#endif  // !BUILDFLAG(IS_IOS)
  return std::make_unique<UnexportableKeyProviderMac>(std::move(config));
}

}  // namespace crypto::apple
