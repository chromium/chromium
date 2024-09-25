// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <AuthenticationServices/AuthenticationServices.h>
#import <objc/message.h>

#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mac/fake_icloud_keychain_sys.h"

// A number of AuthenticationServices objects are subclassed so that the
// values of readonly properties can be overridden in tests.

API_AVAILABLE(macos(13.3))
@interface FakeBrowserPlatformPublicKeyCredential
    : ASAuthorizationWebBrowserPlatformPublicKeyCredential
@property(nonatomic, copy) NSData* credentialID;
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* relyingParty;
@property(nonatomic, copy) NSData* userHandle;
@end

@implementation FakeBrowserPlatformPublicKeyCredential
@synthesize credentialID = _credentialID;
@synthesize name = _name;
@synthesize relyingParty = _relyingParty;
@synthesize userHandle = _userHandle;
@end

API_AVAILABLE(macos(13.3))
@interface FakeASAuthorization : ASAuthorization
@property(nonatomic, strong) id<ASAuthorizationCredential> credential;
@end

@implementation FakeASAuthorization
@synthesize credential = _credential;
@end

API_AVAILABLE(macos(13.3))
@interface FakeASAuthorizationPublicKeyCredentialRegistration
    : NSObject <ASAuthorizationPublicKeyCredentialRegistration>
@property(nonatomic, copy) NSData* rawAttestationObject;
@property(nonatomic, copy) NSData* rawClientDataJSON;
@property(nonatomic, copy) NSData* credentialID;
@end

@implementation FakeASAuthorizationPublicKeyCredentialRegistration
@synthesize rawAttestationObject = _rawAttestationObject;
@synthesize rawClientDataJSON = _rawClientDataJSON;
@synthesize credentialID = _credentialID;

+ (BOOL)supportsSecureCoding {
  return NO;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  NOTREACHED();
}

- (id)copyWithZone:(NSZone*)zone {
  NOTREACHED();
}
@end

API_AVAILABLE(macos(13.3))
@interface FakeASAuthorizationPublicKeyCredentialAssertion
    : NSObject <ASAuthorizationPublicKeyCredentialAssertion>
@property(nonatomic, copy) NSData* rawAuthenticatorData;
@property(nonatomic, copy) NSData* signature;
@property(nonatomic, copy) NSData* userID;
@property(nonatomic, copy) NSData* credentialID;
@property(nonatomic, copy) NSData* rawClientDataJSON;
@end

@implementation FakeASAuthorizationPublicKeyCredentialAssertion
@synthesize rawAuthenticatorData = _rawAuthenticatorData;
@synthesize signature = _signature;
@synthesize userID = _userID;
@synthesize credentialID = _credentialID;
@synthesize rawClientDataJSON = _rawClientDataJSON;

+ (BOOL)supportsSecureCoding {
  return NO;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  NOTREACHED();
}

- (id)copyWithZone:(NSZone*)zone {
  NOTREACHED();
}
@end

namespace device::fido::icloud_keychain {

namespace {
NSData* ToNSData(base::span<const uint8_t> data) {
  return [NSData dataWithBytes:data.data() length:data.size()];
}
}  // namespace

FakeSystemInterface::FakeSystemInterface() = default;

void FakeSystemInterface::set_auth_state(AuthState auth_state) {
  auth_state_ = auth_state;
}

void FakeSystemInterface::set_next_auth_state(AuthState next_auth_state) {
  next_auth_state_ = next_auth_state;
}

void FakeSystemInterface::SetMakeCredentialResult(
    base::span<const uint8_t> attestation_object_bytes,
    base::span<const uint8_t> credential_id) {
  CHECK(!make_credential_error_code_);
  make_credential_attestation_object_bytes_ =
      fido_parsing_utils::Materialize(attestation_object_bytes);
  make_credential_credential_id_ =
      fido_parsing_utils::Materialize(credential_id);
}

void FakeSystemInterface::SetMakeCredentialError(int code) {
  CHECK(!make_credential_attestation_object_bytes_);
  make_credential_error_code_ = code;
}

void FakeSystemInterface::SetGetAssertionResult(
    base::span<const uint8_t> authenticator_data,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> user_id,
    base::span<const uint8_t> credential_id) {
  get_assertion_authenticator_data_ =
      fido_parsing_utils::Materialize(authenticator_data);
  get_assertion_signature_ = fido_parsing_utils::Materialize(signature);
  get_assertion_user_id_ = fido_parsing_utils::Materialize(user_id);
  get_assertion_credential_id_ = fido_parsing_utils::Materialize(credential_id);
}

void FakeSystemInterface::SetGetAssertionError(int code, std::string msg) {
  get_assertion_error_ = std::make_pair(code, std::move(msg));
}

void FakeSystemInterface::SetCredentials(
    std::vector<DiscoverableCredentialMetadata> creds) {
  creds_ = std::move(creds);
}

bool FakeSystemInterface::IsAvailable() const {
  return true;
}

SystemInterface::AuthState FakeSystemInterface::GetAuthState() {
  return auth_state_;
}

void FakeSystemInterface::AuthorizeAndContinue(
    base::OnceCallback<void()> callback) {
  CHECK(next_auth_state_.has_value());
  auth_state_ = *next_auth_state_;
  std::move(callback).Run();
}

void FakeSystemInterface::GetPlatformCredentials(
    const std::string& rp_id,
    void (^block)(
        NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*)) {
  NSMutableArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>* ret =
      [[NSMutableArray alloc] init];
  for (const auto& cred_values : creds_) {
    // `init` on `ASAuthorizationWebBrowserPlatformPublicKeyCredential` is
    // marked as `NS_UNAVAILABLE` and so is not called.
    FakeBrowserPlatformPublicKeyCredential* cred =
        [FakeBrowserPlatformPublicKeyCredential alloc];
    cred.credentialID = ToNSData(cred_values.cred_id);
    cred.userHandle = ToNSData(cred_values.user.id);
    cred.relyingParty = base::SysUTF8ToNSString(rp_id);
    cred.name = base::SysUTF8ToNSString(cred_values.user.name.value_or(""));
    [ret addObject:cred];
  }

  block(ret);
}

void FakeSystemInterface::MakeCredential(
    NSWindow* window,
    CtapMakeCredentialRequest request,
    base::OnceCallback<void(ASAuthorization*, NSError*)> callback) {
  auto attestation_object_bytes =
      std::move(make_credential_attestation_object_bytes_);
  make_credential_attestation_object_bytes_.reset();
  auto credential_id = std::move(make_credential_credential_id_);
  make_credential_credential_id_.reset();
  auto error_code = std::move(make_credential_error_code_);
  make_credential_error_code_.reset();

  if (error_code) {
    std::move(callback).Run(nullptr,
                            [[NSError alloc] initWithDomain:@"WKErrorDomain"
                                                       code:error_code.value()
                                                   userInfo:nullptr]);
  } else if (!attestation_object_bytes) {
    // Error code 1001 is a arbitrary number that doesn't trigger any special
    // processing.
    std::move(callback).Run(nullptr, [[NSError alloc] initWithDomain:@""
                                                                code:1001
                                                            userInfo:nullptr]);
  } else {
    FakeASAuthorizationPublicKeyCredentialRegistration* result =
        [[FakeASAuthorizationPublicKeyCredentialRegistration alloc] init];
    result.rawAttestationObject = ToNSData(*attestation_object_bytes);
    result.credentialID = ToNSData(*credential_id);

    FakeASAuthorization* authorization = [FakeASAuthorization alloc];
    authorization.credential = result;

    std::move(callback).Run(authorization, nullptr);
  }
}

void FakeSystemInterface::GetAssertion(
    NSWindow* window,
    CtapGetAssertionRequest request,
    base::OnceCallback<void(ASAuthorization*, NSError*)> callback) {
  if (get_assertion_error_) {
    NSError* error = [[NSError alloc]
        initWithDomain:@""
                  code:get_assertion_error_->first
              userInfo:@{
                NSLocalizedDescriptionKey : base::SysUTF8ToNSString(
                    get_assertion_error_->second.c_str())
              }];
    get_assertion_error_.reset();
    std::move(callback).Run(nullptr, error);
    return;
  }

  if (!get_assertion_authenticator_data_) {
    std::move(callback).Run(nullptr, [[NSError alloc] initWithDomain:@""
                                                                code:1001
                                                            userInfo:nullptr]);
    return;
  }

  FakeASAuthorizationPublicKeyCredentialAssertion* result =
      [[FakeASAuthorizationPublicKeyCredentialAssertion alloc] init];
  result.rawAuthenticatorData = ToNSData(*get_assertion_authenticator_data_);
  result.signature = ToNSData(*get_assertion_signature_);
  result.userID = ToNSData(*get_assertion_user_id_);
  result.credentialID = ToNSData(*get_assertion_credential_id_);

  get_assertion_authenticator_data_.reset();
  get_assertion_signature_.reset();
  get_assertion_user_id_.reset();
  get_assertion_credential_id_.reset();

  FakeASAuthorization* authorization = [FakeASAuthorization alloc];
  authorization.credential = result;

  std::move(callback).Run(authorization, nullptr);
}

void FakeSystemInterface::Cancel() {
  cancel_count_++;
}

FakeSystemInterface::~FakeSystemInterface() = default;

}  // namespace device::fido::icloud_keychain
