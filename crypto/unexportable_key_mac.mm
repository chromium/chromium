// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/unexportable_key.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#include <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_policy.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/apple_keychain_util.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/features.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key_mac.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace crypto {

namespace {

// The size of an uncompressed x9.63 encoded EC public key, 04 || X || Y.
constexpr size_t kUncompressedPointLength = 65;

// The value of the kSecAttrLabel when generating the key. The documentation
// claims this should be a user-visible label, but there does not exist any UI
// that shows this value. Therefore, it is left untranslated.
constexpr char kAttrLabel[] = "Chromium unexportable key";

// Copies a CFDataRef into a vector of bytes.
std::vector<uint8_t> CFDataToVec(CFDataRef data) {
  auto span = base::apple::CFDataToSpan(data);
  return {span.begin(), span.end()};
}

std::optional<std::vector<uint8_t>> Convertx963ToDerSpki(
    base::span<const uint8_t> x962) {
  // Parse x9.63 point into an |EC_POINT|.
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (x962.size() != kUncompressedPointLength ||
      x962[0] != POINT_CONVERSION_UNCOMPRESSED ||
      !EC_POINT_oct2point(p256.get(), point.get(), x962.data(), x962.size(),
                          /*ctx=*/nullptr)) {
    LOG(ERROR) << "P-256 public key is not on curve";
    return std::nullopt;
  }
  // Marshal point into a DER SPKI.
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_set_public_key(ec_key.get(), point.get()));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_assign_EC_KEY(pkey.get(), ec_key.release()));
  bssl::ScopedCBB cbb;
  uint8_t* der_bytes = nullptr;
  size_t der_bytes_len = 0;
  CHECK(CBB_init(cbb.get(), /* initial size */ 128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()) &&
        CBB_finish(cbb.get(), &der_bytes, &der_bytes_len));
  std::vector<uint8_t> ret(der_bytes, der_bytes + der_bytes_len);
  OPENSSL_free(der_bytes);
  return ret;
}

// UnexportableSigningKeyMac is an implementation of the UnexportableSigningKey
// interface on top of Apple's Secure Enclave.
class UnexportableSigningKeyMac : public UnexportableSigningKey {
 public:
  UnexportableSigningKeyMac(base::apple::ScopedCFTypeRef<SecKeyRef> key,
                            CFDictionaryRef key_attributes)
      : key_(std::move(key)),
        application_label_(
            CFDataToVec(base::apple::GetValueFromDictionary<CFDataRef>(
                key_attributes,
                kSecAttrApplicationLabel))) {
    base::apple::ScopedCFTypeRef<SecKeyRef> public_key(
        AppleKeychainV2::GetInstance().KeyCopyPublicKey(key_.get()));
    base::apple::ScopedCFTypeRef<CFDataRef> x962_bytes(
        AppleKeychainV2::GetInstance().KeyCopyExternalRepresentation(
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
        AppleKeychainV2::GetInstance().KeyCreateSignature(
            key_.get(), algorithm, NSToCFPtrCast(nsdata),
            error.InitializeInto()));
    if (!signature) {
      LOG(ERROR) << "Error signing with key: " << error.get();
      return std::nullopt;
    }
    return CFDataToVec(signature.get());
  }

  bool IsHardwareBacked() const override { return true; }

  SecKeyRef GetSecKeyRef() const override { return key_.get(); }

 private:
  // The wrapped key as returned by the Keychain API.
  const base::apple::ScopedCFTypeRef<SecKeyRef> key_;

  // The MacOS Keychain API sets the application label to the hash of the public
  // key. We use this to uniquely identify the key in lieu of a wrapped private
  // key.
  const std::vector<uint8_t> application_label_;

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
  return base::Contains(acceptable_algorithms, SignatureVerifier::ECDSA_SHA256)
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
      // incorrect because includingkSecAccessControlBiometryAny causes key
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
      AppleKeychainV2::GetInstance().KeyCreateRandomKey(
          NSToCFPtrCast(attributes), error.InitializeInto()));
  if (!private_key) {
    LOG(ERROR) << "Could not create private key: " << error.get();
    return nullptr;
  }
  base::apple::ScopedCFTypeRef<CFDictionaryRef> key_metadata =
      AppleKeychainV2::GetInstance().KeyCopyAttributes(private_key.get());
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
  base::apple::ScopedCFTypeRef<CFTypeRef> key_data;

  NSMutableDictionary* query = [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecReturnRef) : @YES,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
    CFToNSPtrCast(kSecAttrAccessGroup) : objc_storage_->keychain_access_group_,
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:wrapped_key.data() length:wrapped_key.size()],
  }];
  if (lacontext) {
    query[CFToNSPtrCast(kSecUseAuthenticationContext)] = lacontext;
  }
  AppleKeychainV2::GetInstance().ItemCopyMatching(NSToCFPtrCast(query),
                                                  key_data.InitializeInto());
  CFDictionaryRef key_attributes =
      base::apple::CFCast<CFDictionaryRef>(key_data.get());
  if (!key_attributes) {
    return nullptr;
  }
  base::apple::ScopedCFTypeRef<SecKeyRef> key(
      base::apple::GetValueFromDictionary<SecKeyRef>(key_attributes,
                                                     kSecValueRef),
      base::scoped_policy::RETAIN);
  return std::make_unique<UnexportableSigningKeyMac>(std::move(key),
                                                     key_attributes);
}

bool UnexportableKeyProviderMac::DeleteSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrAccessGroup) : objc_storage_->keychain_access_group_,
    CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:wrapped_key.data() length:wrapped_key.size()],
  };
  OSStatus result =
      AppleKeychainV2::GetInstance().ItemDelete(NSToCFPtrCast(query));
  return result == errSecSuccess;
}

std::unique_ptr<UnexportableKeyProviderMac> GetUnexportableKeyProviderMac(
    UnexportableKeyProvider::Config config) {
  if (!base::FeatureList::IsEnabled(crypto::kEnableMacUnexportableKeys)) {
    return nullptr;
  }
  CHECK(!config.keychain_access_group.empty())
      << "A keychain access group must be set when using unexportable keys on "
         "macOS";
  if (![AppleKeychainV2::GetInstance().GetTokenIDs()
          containsObject:CFToNSPtrCast(kSecAttrTokenIDSecureEnclave)]) {
    return nullptr;
  }
  // Inspecting the binary for the entitlement is not available on iOS, assume
  // it is available.
#if !BUILDFLAG(IS_IOS)
  if (!ExecutableHasKeychainAccessGroupEntitlement(
          config.keychain_access_group)) {
    return nullptr;
  }
#endif  // !BUILDFLAG(IS_IOS)
  return std::make_unique<UnexportableKeyProviderMac>(std::move(config));
}

}  // namespace crypto
