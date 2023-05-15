// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/icloud_keychain_sys.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/mac/icloud_keychain_internals.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// This function is needed by the interfaces below, but interfaces must be
// in the top-level namespace.
NSData* ToNSData(base::span<const uint8_t> data) {
  return [NSData dataWithBytes:data.data() length:data.size()];
}

}  // namespace

// ICloudKeychainPresentationDelegate simply returns an `NSWindow` when asked by
// an `ASAuthorizationController`.
API_AVAILABLE(macos(13.3))
@interface ICloudKeychainPresentationDelegate
    : NSObject <ASAuthorizationControllerPresentationContextProviding>
@property(nonatomic, strong) NSWindow* window;
@end

@implementation ICloudKeychainPresentationDelegate
@synthesize window = _window;

- (ASPresentationAnchor)presentationAnchorForAuthorizationController:
    (ASAuthorizationController*)controller {
  return _window;
}
@end

// ICloudKeychainDelegate receives callbacks when an `ASAuthorizationController`
// operation completes (successfully or otherwise) and bridges to a
// `OnceCallback`.
API_AVAILABLE(macos(13.3))
@interface ICloudKeychainDelegate : NSObject <ASAuthorizationControllerDelegate>
- (void)setCallback:
    (base::OnceCallback<void(ASAuthorization* __strong, NSError* __strong)>)
        callback;
- (void)setCleanupCallback:(base::OnceClosure)callback;
@end

@implementation ICloudKeychainDelegate {
  base::OnceCallback<void(ASAuthorization* __strong, NSError* __strong)>
      _callback;
  base::OnceClosure _cleanupCallback;
}

- (void)setCallback:
    (base::OnceCallback<void(ASAuthorization* __strong, NSError* __strong)>)
        callback {
  _callback = std::move(callback);
}

- (void)setCleanupCallback:(base::OnceClosure)callback {
  _cleanupCallback = std::move(callback);
}

- (void)authorizationController:(ASAuthorizationController*)controller
    didCompleteWithAuthorization:(ASAuthorization*)authorization {
  std::move(_callback).Run(authorization, nullptr);
  std::move(_cleanupCallback).Run();
}

- (void)authorizationController:(ASAuthorizationController*)controller
           didCompleteWithError:(NSError*)error {
  std::move(_callback).Run(nullptr, error);
  std::move(_cleanupCallback).Run();
}
@end

// ICloudKeychainCreateController overrides `_requestContextWithRequests` from
// `ASAuthorizationController` so that various extra parameters, which browsers
// need to set, can be specified.
API_AVAILABLE(macos(13.3))
@interface ICloudKeychainCreateController : ASAuthorizationController
@end

@implementation ICloudKeychainCreateController {
  absl::optional<device::CtapMakeCredentialRequest> request_;
}

- (void)setRequest:(device::CtapMakeCredentialRequest)request {
  request_ = std::move(request);
}

- (id<ASCCredentialRequestContext>)
    _requestContextWithRequests:(NSArray<ASAuthorizationRequest*>*)requests
                          error:(NSError**)outError {
  id<ASCCredentialRequestContext> context =
      [super _requestContextWithRequests:requests error:outError];

  id<ASCPublicKeyCredentialCreationOptions> registrationOptions =
      context.platformKeyCredentialCreationOptions;
  registrationOptions.clientDataHash = ToNSData(request_->client_data_hash);
  registrationOptions.challenge = nil;

  NSMutableArray<NSNumber*>* supported_algos = [[NSMutableArray alloc] init];
  for (const device::PublicKeyCredentialParams::CredentialInfo& param :
       request_->public_key_credential_params.public_key_credential_params()) {
    if (param.type == device::CredentialType::kPublicKey) {
      [supported_algos addObject:[NSNumber numberWithInt:base::strict_cast<int>(
                                                             param.algorithm)]];
    }
  }
  if ([supported_algos count] > 0) {
    registrationOptions.supportedAlgorithmIdentifiers = supported_algos;
  }

  registrationOptions.shouldRequireResidentKey =
      request_->resident_key_required;

  const Class descriptor_class =
      NSClassFromString(@"ASCPublicKeyCredentialDescriptor");
  NSMutableArray<ASCPublicKeyCredentialDescriptor*>* exclude_list =
      [[NSMutableArray alloc] init];
  for (const auto& cred : request_->exclude_list) {
    if (cred.credential_type != device::CredentialType::kPublicKey) {
      continue;
    }
    NSMutableArray<NSString*>* transports = [[NSMutableArray alloc] init];
    for (const auto transport : cred.transports) {
      [transports addObject:base::SysUTF8ToNSString(ToString(transport))];
    }
    ASCPublicKeyCredentialDescriptor* descriptor =
        [[descriptor_class alloc] initWithCredentialID:ToNSData(cred.id)
                                            transports:transports];
    [exclude_list addObject:descriptor];
  }
  if ([exclude_list count] > 0) {
    registrationOptions.excludedCredentials = exclude_list;
  }

  return context;
}
@end

// ICloudKeychainGetController overrides `_requestContextWithRequests` from
// `ASAuthorizationController` so that various extra parameters, which browsers
// need to set, can be specified.
API_AVAILABLE(macos(13.3))
@interface ICloudKeychainGetController : ASAuthorizationController
@end

@implementation ICloudKeychainGetController {
  absl::optional<device::CtapGetAssertionRequest> request_;
}

- (void)setRequest:(device::CtapGetAssertionRequest)request {
  request_ = std::move(request);
}

- (id<ASCCredentialRequestContext>)
    _requestContextWithRequests:(NSArray<ASAuthorizationRequest*>*)requests
                          error:(NSError**)outError {
  id<ASCCredentialRequestContext> context =
      [super _requestContextWithRequests:requests error:outError];

  id<ASCPublicKeyCredentialAssertionOptions> assertionOptions =
      context.platformKeyCredentialAssertionOptions;
  assertionOptions.clientDataHash = ToNSData(request_->client_data_hash);
  context.platformKeyCredentialAssertionOptions =
      [assertionOptions copyWithZone:nil];
  return context;
}
@end

namespace device::fido::icloud_keychain {
namespace {

API_AVAILABLE(macos(13.3))
ASAuthorizationWebBrowserPublicKeyCredentialManager* GetManager() {
  return [[ASAuthorizationWebBrowserPublicKeyCredentialManager alloc] init];
}

bool ProcessHasEntitlement() {
  base::ScopedCFTypeRef<SecTaskRef> task(SecTaskCreateFromSelf(nullptr));
  if (!task) {
    return false;
  }

  base::ScopedCFTypeRef<CFTypeRef> entitlement_value_cftype(
      SecTaskCopyValueForEntitlement(
          task, CFSTR("com.apple.developer.web-browser.public-key-credential"),
          nullptr));
  return entitlement_value_cftype;
}

API_AVAILABLE(macos(13.3))
ASAuthorizationPublicKeyCredentialAttestationKind Convert(
    AttestationConveyancePreference preference) {
  // If attestation is requested then the request immediately fails, so
  // all types are mapped to `none`.
  return ASAuthorizationPublicKeyCredentialAttestationKindNone;
}

API_AVAILABLE(macos(13.3))
ASAuthorizationPublicKeyCredentialUserVerificationPreference Convert(
    UserVerificationRequirement uv) {
  switch (uv) {
    case UserVerificationRequirement::kDiscouraged:
      return ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged;
    case UserVerificationRequirement::kPreferred:
      return ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred;
    case UserVerificationRequirement::kRequired:
      return ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired;
  }
}

class API_AVAILABLE(macos(13.3)) NativeSystemInterface
    : public SystemInterface {
 public:
  bool IsAvailable() const override {
    static bool available = ProcessHasEntitlement();
    return available;
  }

  AuthState GetAuthState() override {
    return GetManager().authorizationStateForPlatformCredentials;
  }

  void AuthorizeAndContinue(base::OnceCallback<void()> callback) override {
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    __block auto internal_callback = std::move(callback);
    [GetManager()
        requestAuthorizationForPublicKeyCredentials:^(AuthState state) {
          task_runner->PostTask(FROM_HERE,
                                base::BindOnce(std::move(internal_callback)));
        }];
  }

  void GetPlatformCredentials(
      const std::string& rp_id,
      void (^handler)(
          NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*))
      override {
    [GetManager()
        platformCredentialsForRelyingParty:base::SysUTF8ToNSString(rp_id)
                         completionHandler:handler];
  }

  void MakeCredential(
      NSWindow* window,
      CtapMakeCredentialRequest request,
      base::OnceCallback<void(ASAuthorization* __strong, NSError* __strong)>
          callback) override {
    DCHECK(!create_controller_);
    DCHECK(!get_controller_);
    DCHECK(!delegate_);
    DCHECK(!presentation_delegate_);

    ASAuthorizationPlatformPublicKeyCredentialProvider* provider =
        [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc]
            initWithRelyingPartyIdentifier:base::SysUTF8ToNSString(
                                               request.rp.id)];
    NSData* challenge = ToNSData(request.client_data_hash);
    NSData* user_id = ToNSData(request.user.id);
    NSString* name = base::SysUTF8ToNSString(request.user.name.value_or(""));
    ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest*
        create_request =
            [provider createCredentialRegistrationRequestWithChallenge:challenge
                                                                  name:name
                                                                userID:user_id];
    create_request.attestationPreference =
        Convert(request.attestation_preference);
    create_request.userVerificationPreference =
        Convert(request.user_verification);
    if (request.user.display_name) {
      create_request.displayName =
          base::SysUTF8ToNSString(*request.user.display_name);
    }

    create_controller_ = [[ICloudKeychainCreateController alloc]
        initWithAuthorizationRequests:@[ create_request ]];
    [create_controller_ setRequest:std::move(request)];
    delegate_ = [[ICloudKeychainDelegate alloc] init];
    [delegate_ setCallback:std::move(callback)];
    [delegate_ setCleanupCallback:base::BindOnce(
                                      &NativeSystemInterface::Cleanup, this)];
    create_controller_.delegate = delegate_;
    presentation_delegate_ = [[ICloudKeychainPresentationDelegate alloc] init];
    presentation_delegate_.window = window;
    create_controller_.presentationContextProvider = presentation_delegate_;

    [create_controller_ performRequests];
  }

  void GetAssertion(
      NSWindow* window,
      CtapGetAssertionRequest request,
      base::OnceCallback<void(ASAuthorization* __strong, NSError* __strong)>
          callback) override {
    DCHECK(!create_controller_);
    DCHECK(!get_controller_);
    DCHECK(!delegate_);
    DCHECK(!presentation_delegate_);

    ASAuthorizationPlatformPublicKeyCredentialProvider* provider =
        [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc]
            initWithRelyingPartyIdentifier:base::SysUTF8ToNSString(
                                               request.rp_id)];

    NSData* challenge = ToNSData(request.client_data_hash);
    ASAuthorizationPlatformPublicKeyCredentialAssertionRequest* get_request =
        [provider createCredentialAssertionRequestWithChallenge:challenge];
    NSMutableArray* allowedCredentials = [[NSMutableArray alloc] init];
    for (const auto& cred : request.allow_list) {
      // All credentials are assumed to be platform credentials because we don't
      // wish to trigger macOS's handling of security keys.
      [allowedCredentials
          addObject:[[ASAuthorizationPlatformPublicKeyCredentialDescriptor
                        alloc] initWithCredentialID:ToNSData(cred.id)]];
    }
    get_request.allowedCredentials = allowedCredentials;
    get_request.userVerificationPreference = Convert(request.user_verification);
    get_controller_ = [[ICloudKeychainGetController alloc]
        initWithAuthorizationRequests:@[ get_request ]];
    [get_controller_ setRequest:std::move(request)];
    delegate_ = [[ICloudKeychainDelegate alloc] init];
    [delegate_ setCallback:std::move(callback)];
    [delegate_ setCleanupCallback:base::BindOnce(
                                      &NativeSystemInterface::Cleanup, this)];
    get_controller_.delegate = delegate_;
    presentation_delegate_ = [[ICloudKeychainPresentationDelegate alloc] init];
    presentation_delegate_.window = window;
    get_controller_.presentationContextProvider = presentation_delegate_;

    [get_controller_ performRequests];
  }

 protected:
  ~NativeSystemInterface() override = default;

  void Cleanup() {
    create_controller_ = nullptr;
    get_controller_ = nullptr;
    delegate_ = nullptr;
    presentation_delegate_ = nullptr;
  }

  ICloudKeychainCreateController* __strong create_controller_;
  ICloudKeychainGetController* __strong get_controller_;
  ICloudKeychainDelegate* __strong delegate_;
  ICloudKeychainPresentationDelegate* __strong presentation_delegate_;
};

API_AVAILABLE(macos(13.3))
scoped_refptr<SystemInterface> GetNativeSystemInterface() {
  static scoped_refptr<SystemInterface> native_sys_interface =
      base::MakeRefCounted<NativeSystemInterface>();
  return native_sys_interface;
}

API_AVAILABLE(macos(13.3))
scoped_refptr<SystemInterface>& GetTestInterface() {
  static scoped_refptr<SystemInterface> test_interface;
  return test_interface;
}

}  // namespace

SystemInterface::~SystemInterface() = default;

API_AVAILABLE(macos(13.3))
scoped_refptr<SystemInterface> GetSystemInterface() {
  scoped_refptr<SystemInterface>& test_interface = GetTestInterface();
  if (test_interface) {
    return test_interface;
  }
  return GetNativeSystemInterface();
}

API_AVAILABLE(macos(13.3))
void SetSystemInterfaceForTesting(  // IN-TEST
    scoped_refptr<SystemInterface> sys_interface) {
  GetTestInterface() = sys_interface;
}

}  // namespace device::fido::icloud_keychain
