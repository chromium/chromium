// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/icloud_keychain.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
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
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/icloud_keychain_sys.h"

using base::apple::NSDataToSpan;

namespace device::fido::icloud_keychain {

namespace {

std::vector<uint8_t> ToVector(NSData* data) {
  auto span = NSDataToSpan(data);
  return {span.begin(), span.end()};
}

AuthenticatorSupportedOptions AuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  if (@available(macOS 15.0, *)) {
    options.supports_prf =
        base::FeatureList::IsEnabled(kWebAuthniCloudKeychainPrf);
  }
  return options;
}

// This enum is used in a histogram. Never change assigned values and only add
// new entries at the end.
enum class PasskeyPermissionMetric {
  kRequestedDuringCreate = 0,
  kApprovedDuringCreate = 1,
  kDeniedDuringCreate = 2,

  kRequestedDuringGet = 3,
  kApprovedDuringGet = 4,
  kDeniedDuringGet = 5,

  kMaxValue = 5,
};

template <typename T>
API_AVAILABLE(macos(15.0))
std::optional<std::vector<uint8_t>> PrfOutputToBytes(T* output) {
  if (!output.first) {
    return std::nullopt;
  }

  base::span<const uint8_t> first = NSDataToSpan(output.first);
  CHECK_EQ(first.size(), 32u);
  std::vector<uint8_t> result(first.begin(), first.end());

  if (output.second) {
    base::span<const uint8_t> second = NSDataToSpan(output.second);
    CHECK_EQ(second.size(), 32u);
    result.insert(result.end(), second.begin(), second.end());
  }

  return result;
}

constexpr char kMetricName[] = "WebAuthentication.MacOS.PasskeyPermission";

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
        base::UmaHistogramEnumeration(
            kMetricName, PasskeyPermissionMetric::kRequestedDuringCreate);
        sys_interface->AuthorizeAndContinue(
            base::BindOnce(&Authenticator::MakeCredentialAfterPermissionRequest,
                           weak_factory_.GetWeakPtr(), std::move(request),
                           std::move(continuation)));
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

  void MakeCredentialAfterPermissionRequest(
      CtapMakeCredentialRequest request,
      base::OnceCallback<void(ASAuthorization* authorization, NSError* error)>
          continuation) {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();
    if (sys_interface->GetAuthState() != SystemInterface::kAuthAuthorized) {
      base::UmaHistogramEnumeration(
          kMetricName, PasskeyPermissionMetric::kDeniedDuringCreate);
    } else {
      base::UmaHistogramEnumeration(
          kMetricName, PasskeyPermissionMetric::kApprovedDuringCreate);
    }

    sys_interface->MakeCredential(window_, std::move(request),
                                  std::move(continuation));
  }

  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();

    // Authentication is not required for this operation, but it's a moment
    // when we can reasonably ask for it. If the user authorizes Chromium then
    // `platformCredentialsForRelyingParty` will start working.
    switch (sys_interface->GetAuthState()) {
      case SystemInterface::kAuthNotAuthorized:
        FIDO_LOG(DEBUG) << "iCKC: requesting permission";
        base::UmaHistogramEnumeration(
            kMetricName, PasskeyPermissionMetric::kRequestedDuringGet);
        sys_interface->AuthorizeAndContinue(
            base::BindOnce(&Authenticator::GetAssertionAfterPermissionRequest,
                           weak_factory_.GetWeakPtr(), std::move(request),
                           std::move(callback)));
        break;
      case SystemInterface::kAuthDenied:
        // The operation continues even if the user denied access. See above.
        FIDO_LOG(DEBUG) << "iCKC: passkeys permission is denied";
        [[fallthrough]];
      case SystemInterface::kAuthAuthorized:
        auto continuation =
            base::BindOnce(&Authenticator::OnGetAssertionComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback));
        sys_interface->GetAssertion(window_, std::move(request),
                                    std::move(continuation));
        break;
    }
  }

  void GetAssertionAfterPermissionRequest(CtapGetAssertionRequest request,
                                          GetAssertionCallback callback) {
    scoped_refptr<SystemInterface> sys_interface = GetSystemInterface();
    if (sys_interface->GetAuthState() != SystemInterface::kAuthAuthorized) {
      base::UmaHistogramEnumeration("WebAuthentication.MacOS.PasskeyPermission",
                                    PasskeyPermissionMetric::kDeniedDuringGet);
    } else {
      base::UmaHistogramEnumeration(
          "WebAuthentication.MacOS.PasskeyPermission",
          PasskeyPermissionMetric::kApprovedDuringGet);
    }

    auto continuation =
        base::BindOnce(&Authenticator::OnGetAssertionComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback));
    sys_interface->GetAssertion(window_, std::move(request),
                                std::move(continuation));
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
        return;
      case SystemInterface::kAuthAuthorized:
        break;
    }

    scoped_refptr<base::SequencedTaskRunner> origin_task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    __block auto internal_callback = std::move(callback);
    const std::vector<PublicKeyCredentialDescriptor> allow_list =
        request.allow_list;
    const std::string rp_id = request.rp_id;
    auto handler = ^(
        NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*
            credentials) {
      std::vector<DiscoverableCredentialMetadata> ret;
      for (NSUInteger i = 0; i < credentials.count; i++) {
        const auto& cred = credentials[i];
        std::vector<uint8_t> cred_id = ToVector(cred.credentialID);
        if (!allow_list.empty() &&
            base::ranges::none_of(
                allow_list,
                [&cred_id](const PublicKeyCredentialDescriptor& allow_list_cred)
                    -> bool { return allow_list_cred.id == cred_id; })) {
          continue;
        }
        ret.emplace_back(AuthenticatorType::kICloudKeychain, rp_id,
                         std::move(cred_id),
                         PublicKeyCredentialUserEntity(
                             ToVector(cred.userHandle), cred.name.UTF8String,
                             /* iCloud Keychain does not store
                                a displayName for passkeys */
                             std::nullopt));
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

  void Cancel() override {
    cancelled_ = true;
    GetSystemInterface()->Cancel();
    // If a request was outstanding, `OnMakeCredentialComplete` or
    // `OnGetAssertionComplete` will be called with a generic error.
  }

  AuthenticatorType GetType() const override {
    return AuthenticatorType::kICloudKeychain;
  }

  std::string GetId() const override { return "iCloudKeychain"; }

  const AuthenticatorSupportedOptions& Options() const override {
    static const base::NoDestructor<AuthenticatorSupportedOptions> options(
        AuthenticatorOptions());
    return *options;
  }

  std::optional<FidoTransportProtocol> AuthenticatorTransport() const override {
    return FidoTransportProtocol::kInternal;
  }

  void GetTouch(base::OnceClosure callback) override { NOTREACHED(); }

  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnMakeCredentialComplete(MakeCredentialCallback callback,
                                ASAuthorization* authorization,
                                NSError* error) {
    if (cancelled_) {
      cancelled_ = false;
      std::move(callback).Run(
          MakeCredentialStatus::kAuthenticatorResponseInvalid, {});
      return;
    }

    if (error) {
      const std::string domain = base::SysNSStringToUTF8(error.domain);
      FIDO_LOG(ERROR) << "iCKC: makeCredential failed, domain: " << domain
                      << " code: " << error.code
                      << " msg: " << error.localizedDescription.UTF8String;
      if ((domain == "WKErrorDomain" && error.code == 8) ||
          // As of macOS 15, this error is expressed differently. The value
          // 1006 is ASAuthorizationErrorMatchedExcludedCredential but this
          // change is being made before the macOS 15 SDK is available in
          // Chromium.
          (error.domain != nil &&
           [error.domain isEqualToString:ASAuthorizationErrorDomain] &&
           error.code == 1006)) {
        std::move(callback).Run(
            MakeCredentialStatus::kUserConsentButCredentialExcluded,
            std::nullopt);
      } else {
        // All other errors are currently mapped to `kUserConsentDenied`
        // because it's not obvious that we want to differentiate them:
        // https://developer.apple.com/documentation/authenticationservices/asauthorizationerror?language=objc
        //
        std::move(callback).Run(MakeCredentialStatus::kUserConsentDenied,
                                std::nullopt);
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

    std::optional<cbor::Value> attestation_object_value =
        cbor::Reader::Read(NSDataToSpan(result.rawAttestationObject));
    if (!attestation_object_value || !attestation_object_value->is_map()) {
      FIDO_LOG(ERROR) << "iCKC: failed to parse attestation CBOR";
      std::move(callback).Run(
          MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
      return;
    }

    std::optional<AttestationObject> attestation_object =
        AttestationObject::Parse(*attestation_object_value);
    if (!attestation_object) {
      FIDO_LOG(ERROR) << "iCKC: failed to parse attestation object";
      std::move(callback).Run(
          MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
      return;
    }

    AuthenticatorMakeCredentialResponse response(
        FidoTransportProtocol::kInternal, std::move(*attestation_object));

    std::vector<uint8_t> credential_id_from_auth_data =
        response.attestation_object.authenticator_data().GetCredentialId();
    base::span<const uint8_t> credential_id = NSDataToSpan(result.credentialID);
    if (!base::ranges::equal(credential_id_from_auth_data, credential_id)) {
      FIDO_LOG(ERROR) << "iCKC: credential ID mismatch: "
                      << base::HexEncode(credential_id_from_auth_data) << " vs "
                      << base::HexEncode(credential_id);
      std::move(callback).Run(
          MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
      return;
    }

    response.is_resident_key = true;
    response.transports.emplace();
    response.transports->insert(FidoTransportProtocol::kHybrid);
    response.transports->insert(FidoTransportProtocol::kInternal);
    response.transport_used = FidoTransportProtocol::kInternal;

    if (@available(macOS 15.0, *)) {
      if ([result isKindOfClass:
                      [ASAuthorizationPlatformPublicKeyCredentialRegistration
                          class]]) {
        ASAuthorizationPlatformPublicKeyCredentialRegistration*
            platform_result =
                (ASAuthorizationPlatformPublicKeyCredentialRegistration*)result;
        if (platform_result.prf != nil) {
          response.prf_enabled = platform_result.prf.isSupported;
          response.prf_results = PrfOutputToBytes(platform_result.prf);
        }
      }
    }

    std::move(callback).Run(MakeCredentialStatus::kSuccess,
                            std::move(response));
  }

  void OnGetAssertionComplete(GetAssertionCallback callback,
                              ASAuthorization* authorization,
                              NSError* error) {
    if (cancelled_) {
      cancelled_ = false;
      std::move(callback).Run(GetAssertionStatus::kAuthenticatorResponseInvalid,
                              {});
      return;
    }

    if (error) {
      const std::string_view description =
          error.localizedDescription.UTF8String;
      FIDO_LOG(ERROR) << "iCKC: getAssertion failed, domain: "
                      << base::SysNSStringToUTF8(error.domain)
                      << " code: " << error.code << " msg: " << description;
      // The underlying code sets `shouldShowHybridTransport` to false, which
      // will cause this error to be returned if there are no credentials. We
      // have asked Apple that, if they change this error string, they should
      // please have macOS show its own error dialog.
      GetAssertionStatus response;
      if (error.code == 1001 &&
          base::Contains(description, "No credentials available for login")) {
        response = GetAssertionStatus::kICloudKeychainNoCredentials;
      } else {
        // All other errors are currently mapped to
        // `kUserConsentDenied` because it's not obvious that we
        // want to differentiate them:
        // https://developer.apple.com/documentation/authenticationservices/asauthorizationerror?language=objc
        response = GetAssertionStatus::kUserConsentDenied;
      }
      std::move(callback).Run(response, {});
      return;
    }

    FIDO_LOG(DEBUG) << "iCKC: getAssertion completed";
    CHECK([authorization.credential
        conformsToProtocol:@protocol(
                               ASAuthorizationPublicKeyCredentialAssertion)]);
    id<ASAuthorizationPublicKeyCredentialAssertion> result =
        (id<ASAuthorizationPublicKeyCredentialAssertion>)
            authorization.credential;

    std::optional<AuthenticatorData> authenticator_data =
        AuthenticatorData::DecodeAuthenticatorData(
            NSDataToSpan(result.rawAuthenticatorData));
    if (!authenticator_data) {
      FIDO_LOG(ERROR) << "iCKC: invalid authData";
      std::move(callback).Run(GetAssertionStatus::kAuthenticatorResponseInvalid,
                              {});
      return;
    }

    // The hybrid flow can be offered in the macOS UI, so this may be
    // incorrect, but we've no way of knowing. It's not clear that we can
    // do much about this with the macOS API at the time of writing, short of
    // replacing the system UI completely.
    constexpr auto transport_used = FidoTransportProtocol::kInternal;

    AuthenticatorGetAssertionResponse response(
        std::move(*authenticator_data),
        fido_parsing_utils::Materialize(NSDataToSpan(result.signature)),
        transport_used);
    response.user_entity = PublicKeyCredentialUserEntity(
        fido_parsing_utils::Materialize(NSDataToSpan(result.userID)));
    response.credential = PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(NSDataToSpan(result.credentialID)));
    response.user_selected = true;

    if (@available(macOS 15.0, *)) {
      if ([result
              isKindOfClass:[ASAuthorizationPlatformPublicKeyCredentialAssertion
                                class]]) {
        ASAuthorizationPlatformPublicKeyCredentialAssertion* platform_result =
            (ASAuthorizationPlatformPublicKeyCredentialAssertion*)result;
        if (platform_result.prf != nil) {
          response.hmac_secret = PrfOutputToBytes(platform_result.prf);
        }
      }
    }

    std::vector<AuthenticatorGetAssertionResponse> responses;
    responses.emplace_back(std::move(response));
    std::move(callback).Run(GetAssertionStatus::kSuccess, std::move(responses));
  }

  NSWindow* __strong window_;
  bool cancelled_ = false;
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

  NSWindow* __strong window_;
  std::unique_ptr<Authenticator> authenticator_;
  base::WeakPtrFactory<Discovery> weak_factory_{this};
};

}  // namespace

bool IsSupported() {
  // Here, and in `NewDiscovery`, macOS 13.5 is required. But the rest of the
  // version tests in this code are only for 13.3. That's because the
  // functions used are available in 13.3 but we don't want to launch for
  // 13.3 and 13.4 so that we can updated to require 13.5 in the future without
  // removing functionality for anyone.
  if (@available(macOS 13.5, *)) {
    return GetSystemInterface()->IsAvailable();
  }
  return false;
}

std::unique_ptr<FidoDiscoveryBase> NewDiscovery(uintptr_t ns_window) {
  if (@available(macOS 13.5, *)) {
    NSWindow* window = nullptr;
    if (ns_window != kFakeNSWindowForTesting) {
      window = (__bridge NSWindow*)(void*)ns_window;
      static_assert(sizeof(window) == sizeof(ns_window));
    }

    return std::make_unique<Discovery>(window);
  }

  NOTREACHED();
}

std::optional<bool> HasPermission() {
  if (@available(macOS 13.5, *)) {
    switch (GetSystemInterface()->GetAuthState()) {
      case SystemInterface::kAuthNotAuthorized:
        return std::nullopt;
      case SystemInterface::kAuthDenied:
        return false;
      case SystemInterface::kAuthAuthorized:
        return true;
    }
  }
  return std::nullopt;
}

}  // namespace device::fido::icloud_keychain
