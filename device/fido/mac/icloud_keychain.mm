// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/icloud_keychain.h"

#import <AuthenticationServices/ASFoundation.h>
#import <AuthenticationServices/AuthenticationServices.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/icloud_keychain_sys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace device::fido::icloud_keychain {

namespace {

base::span<const uint8_t> ToSpan(NSData* data) {
  return base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.bytes),
                                   data.length);
}

std::vector<uint8_t> ToVector(NSData* data) {
  const auto* p = reinterpret_cast<const uint8_t*>(data.bytes);
  return std::vector<uint8_t>(p, p + data.length);
}

AuthenticatorSupportedOptions AuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  return options;
}

class API_AVAILABLE(macos(13.3)) Authenticator : public FidoAuthenticator {
 public:
  explicit Authenticator(NSWindow* window) : window_(window) {}
  Authenticator(const Authenticator&) = delete;
  Authenticator& operator=(const Authenticator&) = delete;

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();
    auto continuation =
        base::BindOnce(&Authenticator::OnMakeCredentialComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));

    // Authentication is not required for this operation, but it's a moment
    // when we can reasonably ask for it. If the user authorizes Chromium then
    // `platformCredentialsForRelyingParty` will start working.
    switch (sys_interface->GetAuthState()) {
      case SystemInterface::kAuthNotAuthorized:
        FIDO_LOG(DEBUG) << "iCKC: requesting permission";
        sys_interface->AuthorizeAndContinue(base::BindOnce(
            &SystemInterface::MakeCredential, sys_interface, window_,
            std::move(request), std::move(continuation)));
        break;
      case SystemInterface::kAuthDenied:
        // The operation continues even if the user denied access. See above.
        FIDO_LOG(DEBUG) << "iCKC: passkeys permission is denied";
        [[fallthrough]];
      case SystemInterface::kAuthAuthorized:
        sys_interface->MakeCredential(window_, std::move(request),
                                      std::move(continuation));
        break;
    }
  }

  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();
    auto continuation =
        base::BindOnce(&Authenticator::OnGetAssertionComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));

    // Authentication is not required for this operation, but it's a moment
    // when we can reasonably ask for it. If the user authorizes Chromium then
    // `platformCredentialsForRelyingParty` will start working.
    switch (sys_interface->GetAuthState()) {
      case SystemInterface::kAuthNotAuthorized:
        FIDO_LOG(DEBUG) << "iCKC: requesting permission";
        sys_interface->AuthorizeAndContinue(base::BindOnce(
            &SystemInterface::GetAssertion, GetSystemInterface(), window_,
            std::move(request), std::move(continuation)));
        break;
      case SystemInterface::kAuthDenied:
        // The operation continues even if the user denied access. See above.
        FIDO_LOG(DEBUG) << "iCKC: passkeys permission is denied";
        [[fallthrough]];
      case SystemInterface::kAuthAuthorized:
        GetSystemInterface()->GetAssertion(window_, std::move(request),
                                           std::move(continuation));
        break;
    }
  }

  void GetPlatformCredentialInfoForRequest(
      const CtapGetAssertionRequest& request,
      const CtapGetAssertionOptions& options,
      GetPlatformCredentialInfoForRequestCallback callback) override {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();
    switch (sys_interface->GetAuthState()) {
      case SystemInterface::kAuthNotAuthorized:
      case SystemInterface::kAuthDenied:
        FIDO_LOG(DEBUG)
            << "iCKC: cannot query credentials because of lack of permission";
        std::move(callback).Run(
            {}, FidoRequestHandlerBase::RecognizedCredential::kUnknown);
        break;
      case SystemInterface::kAuthAuthorized:
        break;
    }

    scoped_refptr<base::SequencedTaskRunner> origin_task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    __block auto internal_callback = std::move(callback);
    const std::string rp_id = request.rp_id;
    auto handler = ^(
        NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*
            credentials) {
      std::vector<DiscoverableCredentialMetadata> ret;
      for (NSUInteger i = 0; i < credentials.count; i++) {
        const auto& cred = credentials[i];
        ret.emplace_back(AuthenticatorType::kICloudKeychain, rp_id,
                         ToVector(cred.credentialID),
                         PublicKeyCredentialUserEntity(
                             ToVector(cred.userHandle), cred.name.UTF8String,
                             /* iCloud Keychain does not store
                                a displayName for passkeys */
                             absl::nullopt));
      }
      const auto has_credentials =
          ret.empty() ? FidoRequestHandlerBase::RecognizedCredential::
                            kNoRecognizedCredential
                      : FidoRequestHandlerBase::RecognizedCredential::
                            kHasRecognizedCredential;
      origin_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(internal_callback),
                                    std::move(ret), has_credentials));
    };
    sys_interface->GetPlatformCredentials(rp_id, handler);
  }

  void Cancel() override {}

  AuthenticatorType GetType() const override {
    return AuthenticatorType::kICloudKeychain;
  }

  std::string GetId() const override { return "iCloudKeychain"; }

  const AuthenticatorSupportedOptions& Options() const override {
    static const AuthenticatorSupportedOptions options = AuthenticatorOptions();
    return options;
  }

  absl::optional<FidoTransportProtocol> AuthenticatorTransport()
      const override {
    return FidoTransportProtocol::kInternal;
  }

  void GetTouch(base::OnceClosure callback) override {
    NOTREACHED();
    std::move(callback).Run();
  }

  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnMakeCredentialComplete(MakeCredentialCallback callback,
                                ASAuthorization* __strong authorization,
                                NSError* __strong error) {
    if (error) {
      const std::string domain = base::SysNSStringToUTF8(error.domain);
      FIDO_LOG(ERROR) << "iCKC: makeCredential failed, domain: " << domain
                      << " code: " << error.code
                      << " msg: " << error.localizedDescription.UTF8String;
      if (domain == "WKErrorDomain" && error.code == 8) {
        std::move(callback).Run(
            CtapDeviceResponseCode::kCtap2ErrCredentialExcluded, absl::nullopt);
      } else {
        // All other errors are currently mapped to `kCtap2ErrOperationDenied`
        // because it's not obvious that we want to differentiate them:
        // https://developer.apple.com/documentation/authenticationservices/asauthorizationerror?language=objc
        //
        std::move(callback).Run(
            CtapDeviceResponseCode::kCtap2ErrOperationDenied, absl::nullopt);
      }
      return;
    }

    FIDO_LOG(DEBUG) << "iCKC: makeCredential completed";
    CHECK([authorization.credential
        conformsToProtocol:
            @protocol(ASAuthorizationPublicKeyCredentialRegistration)]);
    id<ASAuthorizationPublicKeyCredentialRegistration> result =
        (id<ASAuthorizationPublicKeyCredentialRegistration>)
            authorization.credential;

    absl::optional<cbor::Value> attestation_object_value =
        cbor::Reader::Read(ToSpan(result.rawAttestationObject));
    if (!attestation_object_value || !attestation_object_value->is_map()) {
      FIDO_LOG(ERROR) << "iCKC: failed to parse attestation CBOR";
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                              absl::nullopt);
      return;
    }

    absl::optional<AttestationObject> attestation_object =
        AttestationObject::Parse(*attestation_object_value);
    if (!attestation_object) {
      FIDO_LOG(ERROR) << "iCKC: failed to parse attestation object";
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                              absl::nullopt);
      return;
    }

    AuthenticatorMakeCredentialResponse response(
        FidoTransportProtocol::kInternal, std::move(*attestation_object));

    std::vector<uint8_t> credential_id_from_auth_data =
        response.attestation_object.authenticator_data().GetCredentialId();
    base::span<const uint8_t> credential_id = ToSpan(result.credentialID);
    if (!base::ranges::equal(credential_id_from_auth_data, credential_id)) {
      FIDO_LOG(ERROR) << "iCKC: credential ID mismatch: "
                      << base::HexEncode(credential_id_from_auth_data) << " vs "
                      << base::HexEncode(credential_id);
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                              absl::nullopt);
      return;
    }

    response.is_resident_key = true;
    response.transports.emplace();
    response.transports->insert(FidoTransportProtocol::kHybrid);
    response.transports->insert(FidoTransportProtocol::kInternal);
    response.transport_used = FidoTransportProtocol::kInternal;

    std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                            std::move(response));
  }

  void OnGetAssertionComplete(GetAssertionCallback callback,
                              ASAuthorization* __strong authorization,
                              NSError* __strong error) {
    if (error) {
      FIDO_LOG(ERROR) << "iCKC: getAssertion failed, domain: "
                      << base::SysNSStringToUTF8(error.domain)
                      << " code: " << error.code
                      << " msg: " << error.localizedDescription.UTF8String;
      // All errors are currently mapped to `kCtap2ErrOperationDenied` because
      // it's not obvious that we want to differentiate them:
      // https://developer.apple.com/documentation/authenticationservices/asauthorizationerror?language=objc
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                              {});
      return;
    }

    FIDO_LOG(DEBUG) << "iCKC: getAssertion completed";
    CHECK([authorization.credential
        conformsToProtocol:@protocol(
                               ASAuthorizationPublicKeyCredentialAssertion)]);
    id<ASAuthorizationPublicKeyCredentialAssertion> result =
        (id<ASAuthorizationPublicKeyCredentialAssertion>)
            authorization.credential;

    absl::optional<AuthenticatorData> authenticator_data =
        AuthenticatorData::DecodeAuthenticatorData(
            ToSpan(result.rawAuthenticatorData));
    if (!authenticator_data) {
      FIDO_LOG(ERROR) << "iCKC: invalid authData";
      std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther, {});
      return;
    }

    AuthenticatorGetAssertionResponse response(
        std::move(*authenticator_data),
        fido_parsing_utils::Materialize(ToSpan(result.signature)));
    response.user_entity = PublicKeyCredentialUserEntity(
        fido_parsing_utils::Materialize(ToSpan(result.userID)));
    response.credential = PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(ToSpan(result.credentialID)));
    response.user_selected = true;
    // The hybrid flow can be offered in the macOS UI, so this may be
    // incorrect, but we've no way of knowing. It's not clear that we can
    // do much about this with the macOS API at the time of writing, short of
    // replacing the system UI completely.
    response.transport_used = FidoTransportProtocol::kInternal;

    std::vector<AuthenticatorGetAssertionResponse> responses;
    responses.emplace_back(std::move(response));
    std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                            std::move(responses));
  }

  NSWindow* const window_;
  base::WeakPtrFactory<Authenticator> weak_factory_{this};
};

class API_AVAILABLE(macos(13.3)) Discovery : public FidoDiscoveryBase {
 public:
  explicit Discovery(NSWindow* window)
      : FidoDiscoveryBase(FidoTransportProtocol::kInternal), window_(window) {}

  // FidoDiscoveryBase:
  void Start() override {
    if (!observer()) {
      return;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Discovery::AddAuthenticator,
                                  weak_factory_.GetWeakPtr()));
  }

 private:
  void AddAuthenticator() {
    authenticator_ = std::make_unique<Authenticator>(window_);
    observer()->DiscoveryStarted(this, /*success=*/true,
                                 {authenticator_.get()});
  }

  NSWindow* const window_;
  std::unique_ptr<Authenticator> authenticator_;
  base::WeakPtrFactory<Discovery> weak_factory_{this};
};

}  // namespace

bool IsSupported() {
  if (@available(macOS 13.3, *)) {
    return GetSystemInterface()->IsAvailable();
  }
  return false;
}

std::unique_ptr<FidoDiscoveryBase> NewDiscovery(uintptr_t ns_window) {
  if (@available(macOS 13.3, *)) {
    NSWindow* window;
    static_assert(sizeof(window) == sizeof(ns_window));
    memcpy((void*)&window, &ns_window, sizeof(ns_window));

    auto discovery = std::make_unique<Discovery>(window);

    // Clear pointer so that ObjC doesn't try to release it.
    memset((void*)&window, 0, sizeof(window));

    return discovery;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace device::fido::icloud_keychain
