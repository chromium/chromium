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
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_policy.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/types/cxx26_projected_value_t.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "crypto/apple/keychain_util.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/unexportable_key_apple.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
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
// Will be removed upon expiration
#if BUILDFLAG(IS_MAC)
  static constexpr char kKeyErrorStatusHistogramFormatMacPrefix[] =
      "Crypto.SecureEnclaveOperation.Mac.";
  static constexpr char kKeyErrorStatusHistogramFormatMacSuffix[] = ".Error";
  base::UmaHistogramSparse(
      base::StrCat({kKeyErrorStatusHistogramFormatMacPrefix,
                    OperationToString(operation),
                    kKeyErrorStatusHistogramFormatMacSuffix}),
      status);
#endif  // BUILDFLAG(IS_MAC)
  static constexpr char kKeyErrorStatusHistogramFormatPrefix[] =
      "Crypto.SecureEnclaveOperation.Apple.";
  static constexpr char kKeyErrorStatusHistogramFormatSuffix[] = ".Error";
  base::UmaHistogramSparse(base::StrCat({kKeyErrorStatusHistogramFormatPrefix,
                                         OperationToString(operation),
                                         kKeyErrorStatusHistogramFormatSuffix}),
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

std::optional<std::vector<uint8_t>> SignSlowlyImpl(
    SecKeyRef key,
    base::span<const uint8_t> data,
    TPMOperation operation) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  SecKeyAlgorithm algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
  if (!SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, algorithm)) {
    // This is not expected to happen, but it could happen if e.g. the key had
    // been replaced by a key of a different type with the same label.
    LOG(ERROR) << "Key does not support ECDSA algorithm";
    return std::nullopt;
  }

  NSData* nsdata = [NSData dataWithBytes:data.data() length:data.size()];
  base::apple::ScopedCFTypeRef<CFErrorRef> error;
  base::apple::ScopedCFTypeRef<CFDataRef> signature(
      KeychainV2::GetInstance().KeyCreateSignature(
          key, algorithm, NSToCFPtrCast(nsdata), error.InitializeInto()));
  if (!signature) {
    LOG(ERROR) << "Error signing with key: " << error.get();
    LogKeychainOperationError(operation, error);
    return std::nullopt;
  }
  return base::ToVector(base::apple::CFDataToSpan(signature.get()));
}

// Helper to extract the application tag from a dictionary of key attributes.
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

// Helper to extract the application label from a dictionary of key attributes.
base::span<const uint8_t> GetApplicationLabel(
    CFDictionaryRef key_attributes LIFETIME_BOUND) {
  return base::apple::CFDataToSpan(
      base::apple::GetValueFromDictionary<CFDataRef>(key_attributes,
                                                     kSecAttrApplicationLabel));
}

// Helper to construct a absl::flat_hash_set from a range and an optional
// projection. Like `base::ToVector`, but for `absl::flat_hash_set`.
template <
    typename R,
    typename Proj = std::identity,
    typename T = base::projected_value_t<std::ranges::iterator_t<R>, Proj>>
absl::flat_hash_set<T> ToFlatHashSet(R&& range, Proj proj = {}) {
  absl::flat_hash_set<T> set;
  set.reserve(range.size());
  std::ranges::transform(std::forward<R>(range), std::inserter(set, set.end()),
                         std::move(proj));
  return set;
}

// Options struct for `FindUnexportableKeys`.
struct FindUnexportableKeysOptions {
  NSString* access_group = nullptr;
  std::string_view application_tag_prefix;
  base::raw_span<const uint8_t> wrapped_key;
  LAContext* lacontext = nullptr;
};

// Returns a vector of keychain items matching the given attributes or an
// OSStatus error code in case of failure.
base::expected<std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>,
               OSStatus>
FindUnexportableKeys(FindUnexportableKeysOptions options) {
  auto [access_group, application_tag_prefix, wrapped_key, lacontext] = options;
  NSMutableDictionary* query = [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
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
  switch (OSStatus status = KeychainV2::GetInstance().ItemCopyMatching(
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

  // Perform prefix matching on the application tag.
  std::erase_if(items, [&](auto& item) {
    return !GetApplicationTag(item.get()).starts_with(application_tag_prefix);
  });

  return items;
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

  if (OSStatus status =
          KeychainV2::GetInstance().ItemDelete(NSToCFPtrCast(delete_query));
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

std::optional<std::vector<uint8_t>> ConvertX962ToDerSpki(
    base::span<const uint8_t> x962) {
  std::optional<keypair::PublicKey> imported =
      keypair::PublicKey::FromEcP256Point(x962);
  if (!imported) {
    LOG(ERROR) << "P-256 public key is not on curve";
    return std::nullopt;
  }
  return imported->ToSubjectPublicKeyInfo();
}

template <typename BaseInterface>
class AppleKeyImpl : public BaseInterface, public StatefulKey {
 public:
  explicit AppleKeyImpl(CFDictionaryRef key_attributes)
      : AppleKeyImpl(
            base::apple::ScopedCFTypeRef<SecKeyRef>(
                base::apple::GetValueFromDictionary<SecKeyRef>(key_attributes,
                                                               kSecValueRef),
                base::scoped_policy::RETAIN),
            key_attributes) {}

  AppleKeyImpl(base::apple::ScopedCFTypeRef<SecKeyRef> key,
               CFDictionaryRef key_attributes)
      : key_(std::move(key)),
        application_label_(base::ToVector(GetApplicationLabel(key_attributes))),
        application_tag_(GetApplicationTag(key_attributes)),
        creation_time_(GetCreationTimeFromAttributes(key_attributes)) {
    base::apple::ScopedCFTypeRef<SecKeyRef> public_key(
        KeychainV2::GetInstance().KeyCopyPublicKey(key_.get()));
    CHECK(public_key);
    base::apple::ScopedCFTypeRef<CFDataRef> x962_bytes(
        KeychainV2::GetInstance().KeyCopyExternalRepresentation(
            public_key.get(), /*error=*/nil));
    CHECK(x962_bytes);
    public_key_spki_ = CHECK_DEREF(
        ConvertX962ToDerSpki(base::apple::CFDataToSpan(x962_bytes.get())));
  }

  sign::SignatureKind Algorithm() const override { return sign::ECDSA_SHA256; }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return public_key_spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    return application_label_;
  }

  bool IsHardwareBacked() const override { return true; }

  SecKeyRef GetSecKeyRef() const override { return key_.get(); }

  const StatefulKey* AsStatefulKey() const LIFETIME_BOUND override {
    return this;
  }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return SignSlowlyImpl(GetSecKeyRef(), data, TPMOperation::kMessageSigning);
  }

  // StatefulKey:
  std::string GetKeyTag() const override { return application_tag_; }

  base::Time GetCreationTime() const override { return creation_time_; }

 private:
  // The wrapped key as returned by the Keychain API.
  const base::apple::ScopedCFTypeRef<SecKeyRef> key_;

  // The Apple Keychain API sets the application label to the hash of the public
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

class UnexportableSigningKeyApple
    : public AppleKeyImpl<UnexportableSigningKey> {
 public:
  using AppleKeyImpl<UnexportableSigningKey>::AppleKeyImpl;
};

class UnexportableAttestationKeyApple
    : public AppleKeyImpl<UnexportableAttestationKey> {
 public:
  using AppleKeyImpl<UnexportableAttestationKey>::AppleKeyImpl;

  std::optional<AttestationStatement> CertifySlowly(
      const UnexportableSigningKey& signing_key,
      base::span<const uint8_t> challenge) override {
    std::vector<uint8_t> raw_stmt;
    // TODO(crbug.com/406190025): Make the hash algorithm generic once we use
    // the crypto::sign algorithms.
    raw_stmt.reserve(challenge.size() + hash::kSha256Size);
    base::Extend(raw_stmt, challenge);
    base::Extend(raw_stmt, hash::Sha256(signing_key.GetSubjectPublicKeyInfo()));

    ASSIGN_OR_RETURN(std::vector<uint8_t> der_sig,
                     SignSlowlyImpl(GetSecKeyRef(), raw_stmt,
                                    TPMOperation::kKeyCertification));

    ASSIGN_OR_RETURN(keypair::PublicKey public_key,
                     keypair::PublicKey::FromSubjectPublicKeyInfo(
                         GetSubjectPublicKeyInfo()));

    ASSIGN_OR_RETURN(std::vector<uint8_t> raw_sig,
                     ConvertEcdsaDerSignatureToRaw(public_key, der_sig));

    return AttestationStatement{
        .format = AttestationStatement::kSecureEnclave,
        .statement = std::move(raw_stmt),
        .signature = std::move(raw_sig),
    };
  }
};

template <typename KeyClass>
std::unique_ptr<KeyClass> GenerateKeySlowlyImpl(
    UnexportableKeyProvider::Config::AccessControl access_control,
    NSString* keychain_access_group,
    NSString* application_tag,
    base::span<const sign::SignatureKind> acceptable_algorithms,
    LAContext* lacontext) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // The Secure Enclave only supports elliptic curve keys.
  if (!std::ranges::contains(acceptable_algorithms, sign::ECDSA_SHA256)) {
    return nullptr;
  }

  // Generate the key pair.
  SecAccessControlCreateFlags control_flags = kSecAccessControlPrivateKeyUsage;
  switch (access_control) {
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
    CFToNSPtrCast(kSecAttrAccessGroup) : keychain_access_group,
    CFToNSPtrCast(kSecAttrLabel) : base::SysUTF8ToNSString(kAttrLabel),
    CFToNSPtrCast(kSecAttrApplicationTag) : application_tag,
  };

  base::apple::ScopedCFTypeRef<CFErrorRef> error;
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key(
      KeychainV2::GetInstance().KeyCreateRandomKey(NSToCFPtrCast(attributes),
                                                   error.InitializeInto()));
  if (!private_key) {
    LOG(ERROR) << "Could not create private key: " << error.get();
    TPMOperation operation =
        std::same_as<KeyClass, UnexportableAttestationKeyApple>
            ? TPMOperation::kNewAttestationKeyCreation
            : TPMOperation::kNewKeyCreation;
    LogKeychainOperationError(operation, error);
    return nullptr;
  }
  base::apple::ScopedCFTypeRef<CFDictionaryRef> key_metadata =
      KeychainV2::GetInstance().KeyCopyAttributes(private_key.get());
  return std::make_unique<KeyClass>(std::move(private_key), key_metadata.get());
}

template <typename KeyClass>
std::unique_ptr<KeyClass> FromWrappedKeySlowlyImpl(
    NSString* keychain_access_group,
    NSString* application_tag,
    base::span<const uint8_t> wrapped_key,
    LAContext* lacontext) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  TPMOperation operation =
      std::same_as<KeyClass, UnexportableAttestationKeyApple>
          ? TPMOperation::kWrappedAttestationKeyCreation
          : TPMOperation::kWrappedKeyCreation;

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> key_dicts,
      FindUnexportableKeys({
          .access_group = keychain_access_group,
          .wrapped_key = wrapped_key,
          .lacontext = lacontext,
      }),
      [operation](OSStatus status) {
        LogKeychainOperationError(operation, status);
        return nullptr;
      });

  if (key_dicts.empty()) {
    LogKeychainOperationError(operation, errSecItemNotFound);
    return nullptr;
  }

  // Try to find an exact match for the desired `application_tag`, return if
  // found.
  if (auto it =
          std::ranges::find(key_dicts, base::SysNSStringToUTF8(application_tag),
                            [](const auto& key_dict) {
                              return GetApplicationTag(key_dict.get());
                            });
      it != key_dicts.end()) {
    return std::make_unique<KeyClass>(it->get());
  }

  // Lastly, if there are matching entries for `wrapped_key`, but no exact match
  // for `application_tag`, make a copy of the first partial match, explicitly
  // set the application_tag, and write it to the keychain. Return this key if
  // no error occurred.
  NSMutableDictionary* key_attributes = [NSMutableDictionary
      dictionaryWithDictionary:CFToNSPtrCast(key_dicts.front().get())];
  key_attributes[CFToNSPtrCast(kSecAttrApplicationTag)] = application_tag;
  if (lacontext) {
    key_attributes[CFToNSPtrCast(kSecUseAuthenticationContext)] = lacontext;
  }

  if (OSStatus status =
          KeychainV2::GetInstance().ItemAdd(NSToCFPtrCast(key_attributes),
                                            /*result=*/nil);
      status != errSecSuccess) {
    LogKeychainOperationError(operation, status);
    return nullptr;
  }

  return std::make_unique<KeyClass>(NSToCFPtrCast(key_attributes));
}

}  // namespace

struct UnexportableKeyProviderApple::ObjCStorage {
  NSString* __strong keychain_access_group_;
  NSString* __strong application_tag_;
};

UnexportableKeyProviderApple::UnexportableKeyProviderApple(Config config)
    : access_control_(config.access_control),
      objc_storage_(new ObjCStorage{
          .keychain_access_group_ =
              base::SysUTF8ToNSString(config.keychain_access_group),
          .application_tag_ = base::SysUTF8ToNSString(config.application_tag),
      }) {}
UnexportableKeyProviderApple::~UnexportableKeyProviderApple() = default;

std::optional<sign::SignatureKind>
UnexportableKeyProviderApple::SelectAlgorithm(
    base::span<const sign::SignatureKind> acceptable_algorithms) {
  return std::ranges::contains(acceptable_algorithms, sign::ECDSA_SHA256)
             ? std::optional(sign::ECDSA_SHA256)
             : std::nullopt;
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderApple::GenerateSigningKeySlowly(
    base::span<const sign::SignatureKind> acceptable_algorithms) {
  return GenerateSigningKeySlowly(acceptable_algorithms, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderApple::GenerateSigningKeySlowly(
    base::span<const sign::SignatureKind> acceptable_algorithms,
    LAContext* lacontext) {
  return GenerateKeySlowlyImpl<UnexportableSigningKeyApple>(
      access_control_, objc_storage_->keychain_access_group_,
      objc_storage_->application_tag_, acceptable_algorithms, lacontext);
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderApple::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return FromWrappedSigningKeySlowly(wrapped_key, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableSigningKey>
UnexportableKeyProviderApple::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key,
    LAContext* lacontext) {
  return FromWrappedKeySlowlyImpl<UnexportableSigningKeyApple>(
      objc_storage_->keychain_access_group_, objc_storage_->application_tag_,
      wrapped_key, lacontext);
}

std::unique_ptr<UnexportableAttestationKey>
UnexportableKeyProviderApple::GenerateAttestationKeySlowly(
    base::span<const sign::SignatureKind> acceptable_algorithms) {
  return GenerateAttestationKeySlowly(acceptable_algorithms, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableAttestationKey>
UnexportableKeyProviderApple::GenerateAttestationKeySlowly(
    base::span<const sign::SignatureKind> acceptable_algorithms,
    LAContext* lacontext) {
  return GenerateKeySlowlyImpl<UnexportableAttestationKeyApple>(
      access_control_, objc_storage_->keychain_access_group_,
      objc_storage_->application_tag_, acceptable_algorithms, lacontext);
}

std::unique_ptr<UnexportableAttestationKey>
UnexportableKeyProviderApple::FromWrappedAttestationKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return FromWrappedAttestationKeySlowly(wrapped_key, /*lacontext=*/nil);
}

std::unique_ptr<UnexportableAttestationKey>
UnexportableKeyProviderApple::FromWrappedAttestationKeySlowly(
    base::span<const uint8_t> wrapped_key,
    LAContext* lacontext) {
  return FromWrappedKeySlowlyImpl<UnexportableAttestationKeyApple>(
      objc_storage_->keychain_access_group_, objc_storage_->application_tag_,
      wrapped_key, lacontext);
}

StatefulUnexportableKeyProvider*
UnexportableKeyProviderApple::AsStatefulUnexportableKeyProvider() {
  return this;
}

std::optional<std::vector<std::unique_ptr<UnexportableSigningKey>>>
UnexportableKeyProviderApple::GetAllKeysSlowly() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys({
          .access_group = objc_storage_->keychain_access_group_,
          .application_tag_prefix =
              base::SysNSStringToUTF8(objc_storage_->application_tag_),
      }),
      [](OSStatus) { return std::nullopt; });

  return base::ToVector(
      keys, [](const auto& key) -> std::unique_ptr<UnexportableSigningKey> {
        return std::make_unique<UnexportableSigningKeyApple>(key.get());
      });
}

std::optional<size_t> UnexportableKeyProviderApple::DeleteWrappedKeysSlowly(
    base::span<const base::span<const uint8_t>> wrapped_keys) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (wrapped_keys.empty()) {
    return 0;
  }

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys({
          .access_group = objc_storage_->keychain_access_group_,
          .application_tag_prefix =
              base::SysNSStringToUTF8(objc_storage_->application_tag_),
      }),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  const auto keys_to_delete = ToFlatHashSet(wrapped_keys);
  std::erase_if(keys, [&](const auto& key) {
    return !keys_to_delete.contains(GetApplicationLabel(key.get()));
  });

  return std::ranges::count_if(
      keys, [&](const auto& key) { return DeleteKey(key.get()); });
}

std::optional<size_t> UnexportableKeyProviderApple::DeleteKeysSlowly(
    base::span<const UnexportableSigningKey* const> keys) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (keys.empty()) {
    return 0;
  }

  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keychain_keys,
      FindUnexportableKeys({
          .access_group = objc_storage_->keychain_access_group_,
          .application_tag_prefix =
              base::SysNSStringToUTF8(objc_storage_->application_tag_),
      }),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  const auto keys_and_tags_to_delete = ToFlatHashSet(keys, [](const auto* key) {
    // NOTE: The keys passed to this method should always be instances of
    // `UnexportableSigningKeyApple`, which are guaranteed to be stateful.
    // Thus we can confidently `CHECK` here.
    return std::pair{key->GetWrappedKey(),
                     CHECK_DEREF(key->AsStatefulKey()).GetKeyTag()};
  });

  std::erase_if(keychain_keys, [&](const auto& key) {
    return !keys_and_tags_to_delete.contains({
        base::ToVector(GetApplicationLabel(key.get())),
        GetApplicationTag(key.get()),
    });
  });

  return std::ranges::count_if(
      keychain_keys, [&](const auto& key) { return DeleteKey(key.get()); });
}

std::optional<size_t> UnexportableKeyProviderApple::DeleteAllKeysSlowly() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  const std::string application_tag_prefix =
      base::SysNSStringToUTF8(objc_storage_->application_tag_);
  ASSIGN_OR_RETURN(
      std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys,
      FindUnexportableKeys({
          .access_group = objc_storage_->keychain_access_group_,
          .application_tag_prefix = application_tag_prefix,
      }),
      [](OSStatus status) {
        LogKeychainOperationError(TPMOperation::kKeyDeletion, status);
        return std::nullopt;
      });

  // As a safeguard, don't perform prefix matching if the application_tag_prefix
  // used in the query was empty.
  if (application_tag_prefix.empty()) {
    std::erase_if(keys, [](const auto& key) {
      return !GetApplicationTag(key.get()).empty();
    });
  }

  return std::ranges::count_if(
      keys, [&](const auto& key) { return DeleteKey(key.get()); });
}

std::unique_ptr<UnexportableKeyProviderApple> GetUnexportableKeyProviderApple(
    UnexportableKeyProvider::Config config) {
  CHECK(!config.keychain_access_group.empty())
      << "A keychain access group must be set when using unexportable keys on "
         "Apple platforms";
#if !BUILDFLAG(IS_IOS)
  // Secure Enclave on iOS is available since the iPhone 5s (A7 chip). This
  // redundant check causes crashes because of the synchronous initialization of
  // `TKTokenWatcher` on every call.
  if (![KeychainV2::GetInstance().GetTokenIDs()
          containsObject:CFToNSPtrCast(kSecAttrTokenIDSecureEnclave)]) {
    return nullptr;
  }
  // Inspecting the binary for the entitlement is not available on iOS, assume
  // it is available.
  if (!ExecutableHasKeychainAccessGroupEntitlement(
          config.keychain_access_group)) {
    return nullptr;
  }
#endif  // !BUILDFLAG(IS_IOS)
  return std::make_unique<UnexportableKeyProviderApple>(std::move(config));
}

}  // namespace crypto::apple
