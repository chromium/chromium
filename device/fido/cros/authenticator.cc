// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/authenticator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/opaque_attestation_statement.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace device {

ChromeOSAuthenticator::ChromeOSAuthenticator(
    base::RepeatingCallback<std::string()> generate_request_id_callback,
    ChromeOSAuthenticator::Config config)
    : generate_request_id_callback_(std::move(generate_request_id_callback)),
      config_(config),
      weak_factory_(this) {}

ChromeOSAuthenticator::~ChromeOSAuthenticator() {}

AuthenticatorType ChromeOSAuthenticator::GetType() const {
  return AuthenticatorType::kChromeOS;
}

std::string ChromeOSAuthenticator::GetId() const {
  return "ChromeOSAuthenticator";
}

namespace {

AuthenticatorSupportedOptions ChromeOSAuthenticatorOptions(bool u2f_enabled) {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  // TODO(yichengli): change supports_resident_key to true once it's supported.
  options.supports_resident_key = false;
  // Even if the user has no fingerprints enrolled, we will have password
  // as fallback.
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  // Enterprise attestation is enabled in the authenticator if its U2F/G2F mode
  // is enabled.
  options.enterprise_attestation = u2f_enabled;
  return options;
}

}  // namespace

const AuthenticatorSupportedOptions& ChromeOSAuthenticator::Options() const {
  static const base::NoDestructor<AuthenticatorSupportedOptions> options(
      ChromeOSAuthenticatorOptions(u2f_enabled_));
  return *options;
}

std::optional<FidoTransportProtocol>
ChromeOSAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

void ChromeOSAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  FIDO_LOG(DEBUG) << "ChromeOSAuthenticator::InitializeAuthenticator()";
  auto barrier = base::BarrierClosure(2, std::move(callback));

  u2f::GetAlgorithmsRequest request;
  chromeos::U2FClient::Get()->GetAlgorithms(
      request, base::BindOnce(&ChromeOSAuthenticator::OnGetAlgorithmsResponse,
                              weak_factory_.GetWeakPtr(), barrier));

  IsPowerButtonModeEnabled(
      base::BindOnce(&ChromeOSAuthenticator::OnIsPowerButtonModeEnabled,
                     weak_factory_.GetWeakPtr(), barrier));
}

void ChromeOSAuthenticator::OnGetAlgorithmsResponse(
    base::OnceClosure callback,
    std::optional<u2f::GetAlgorithmsResponse> response) {
  FIDO_LOG(DEBUG) << "ChromeOSAuthenticator::OnGetAlgorithmsResponse()";
  if (response && response->status() ==
                      u2f::GetAlgorithmsResponse_GetAlgorithmsStatus_SUCCESS) {
    supported_algorithms_ = std::vector<int32_t>();
    for (int i = 0; i < response->algorithm_size(); i++) {
      supported_algorithms_->push_back(response->algorithm(i));
    }
  } else {
    // Keep `supported_algorithms_` as nullopt if fetching supported algorithms
    // from u2fd failed, since the caller of `GetAlgorithms` method might want
    // to provide defaults.
    supported_algorithms_ = std::nullopt;
  }

  std::move(callback).Run();
}

void ChromeOSAuthenticator::OnIsPowerButtonModeEnabled(
    base::OnceClosure callback,
    bool enabled) {
  FIDO_LOG(DEBUG) << "ChromeOSAuthenticator::OnIsPowerButtonModeEnabled()="
                  << enabled;
  u2f_enabled_ = enabled;
  std::move(callback).Run();
}

std::optional<base::span<const int32_t>>
ChromeOSAuthenticator::GetAlgorithms() {
  if (supported_algorithms_) {
    return base::span<const int32_t>(*supported_algorithms_);
  }
  return std::nullopt;
}

void ChromeOSAuthenticator::MakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions request_options,
    MakeCredentialCallback callback) {
  u2f::MakeCredentialRequest req;
  // Only allow skipping user verification if user presence checks via power
  // button press have been configured. This is only the case when running with
  // the "DeviceSecondFactorAuthentication" enterprise policy.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged &&
       config_.power_button_enabled)
          ? u2f::VERIFICATION_USER_PRESENCE
          : u2f::VERIFICATION_USER_VERIFICATION);
  req.set_rp_id(request.rp.id);
  req.set_client_data_hash(std::string(request.client_data_hash.begin(),
                                       request.client_data_hash.end()));

  // The ChromeOS platform authenticator supports attestation only for
  // credentials created through the legacy, enterprise-policy-controlled power
  // button authenticator. It has two modes, regular U2F attestation and and
  // individually identifying mode called G2F that needs to be explicitly
  // configured in the enterprise policy.
  switch (request.attestation_preference) {
    case AttestationConveyancePreference::kNone:
    case AttestationConveyancePreference::kIndirect:
      req.set_attestation_conveyance_preference(
          u2f::MakeCredentialRequest_AttestationConveyancePreference_NONE);
      break;
    case AttestationConveyancePreference::kDirect:
      req.set_attestation_conveyance_preference(
          u2f::MakeCredentialRequest_AttestationConveyancePreference_U2F);
      break;
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
      // There is no separate mechanism for allowing individual RPs to use
      // individual G2F attestation. (Same as with regular U2F authenticators.)
      req.set_attestation_conveyance_preference(
          u2f::MakeCredentialRequest_AttestationConveyancePreference_U2F);
      break;
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      req.set_attestation_conveyance_preference(
          u2f::MakeCredentialRequest_AttestationConveyancePreference_G2F);
      break;
  }

  req.set_user_id(std::string(request.user.id.begin(), request.user.id.end()));
  if (request.user.display_name.has_value()) {
    req.set_user_display_name(request.user.display_name.value());
  }
  req.set_resident_credential(request.resident_key_required);
  DCHECK(generate_request_id_callback_);
  DCHECK(current_request_id_.empty());
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id_str(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.exclude_list) {
    req.add_excluded_credential_id(
        std::string(descriptor.id.begin(), descriptor.id.end()));
  }
  if (request.app_id_exclude) {
    req.set_app_id_exclude(*request.app_id_exclude);
  }

  chromeos::U2FClient::Get()->MakeCredential(
      req, base::BindOnce(&ChromeOSAuthenticator::OnMakeCredentialResponse,
                          weak_factory_.GetWeakPtr(), std::move(request),
                          std::move(callback)));
}

void ChromeOSAuthenticator::OnMakeCredentialResponse(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback,
    std::optional<u2f::MakeCredentialResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "MakeCredential dbus call failed";
    std::move(callback).Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
                            std::nullopt);
    return;
  }

  FIDO_LOG(DEBUG) << "Make credential status: " << response->status();
  if (response->status() !=
      u2f::MakeCredentialResponse_MakeCredentialStatus_SUCCESS) {
    std::move(callback).Run(MakeCredentialStatus::kUserConsentDenied,
                            std::nullopt);
    return;
  }

  std::optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(response->authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
                            std::nullopt);
    return;
  }

  std::optional<cbor::Value> statement_map = cbor::Reader::Read(
      base::as_bytes(base::make_span(response->attestation_statement())));
  if (!statement_map ||
      statement_map.value().type() != cbor::Value::Type::MAP) {
    FIDO_LOG(ERROR) << "Attestation statement is not a CBOR map.";
    std::move(callback).Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
                            std::nullopt);
    return;
  }
  auto statement = std::make_unique<OpaqueAttestationStatement>(
      response->attestation_format(), std::move(*statement_map));

  AuthenticatorMakeCredentialResponse fido_response(
      FidoTransportProtocol::kInternal,
      AttestationObject(std::move(*authenticator_data), std::move(statement)));
  fido_response.transports.emplace();
  fido_response.transports->insert(FidoTransportProtocol::kInternal);

  std::move(callback).Run(MakeCredentialStatus::kSuccess,
                          std::move(fido_response));
}

void ChromeOSAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                         CtapGetAssertionOptions options,
                                         GetAssertionCallback callback) {
  u2f::GetAssertionRequest req;
  // Only allow skipping user verification if user presence checks via power
  // button press have been configured. This is only the case when running with
  // the "DeviceSecondFactorAuthentication" enterprise policy.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged &&
       config_.power_button_enabled)
          ? u2f::VERIFICATION_USER_PRESENCE
          : u2f::VERIFICATION_USER_VERIFICATION);
  req.set_rp_id(request.rp_id);
  if (request.app_id) {
    req.set_app_id(*request.app_id);
  }
  req.set_client_data_hash(std::string(request.client_data_hash.begin(),
                                       request.client_data_hash.end()));
  DCHECK(generate_request_id_callback_);
  DCHECK(current_request_id_.empty());
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id_str(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    req.add_allowed_credential_id(
        std::string(descriptor.id.begin(), descriptor.id.end()));
  }

  chromeos::U2FClient::Get()->GetAssertion(
      req, base::BindOnce(&ChromeOSAuthenticator::OnGetAssertionResponse,
                          weak_factory_.GetWeakPtr(), std::move(request),
                          std::move(callback)));
}

void ChromeOSAuthenticator::GetPlatformCredentialInfoForRequest(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    GetPlatformCredentialInfoForRequestCallback callback) {
  FIDO_LOG(DEBUG)
      << "ChromeOSAuthenticator::GetPlatformCredentialInfoForRequest()";
  u2f::HasCredentialsRequest req;
  req.set_rp_id(request.rp_id);
  if (request.app_id) {
    req.set_app_id(*request.app_id);
  }

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    req.add_credential_id(
        std::string(descriptor.id.begin(), descriptor.id.end()));
  }

  chromeos::U2FClient::Get()->HasCredentials(
      req, base::BindOnce(
               &ChromeOSAuthenticator::OnHasCredentialInformationForRequest,
               weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSAuthenticator::OnHasCredentialInformationForRequest(
    GetPlatformCredentialInfoForRequestCallback callback,
    std::optional<u2f::HasCredentialsResponse> response) {
  FIDO_LOG(DEBUG)
      << "ChromeOSAuthenticator::OnHasCredentialInformationForRequest() status="
      << static_cast<int>(response->status())
      << " size=" << response->credential_id().size();
  std::move(callback).Run(
      /*credentials=*/{},
      response &&
              response->status() ==
                  u2f::HasCredentialsResponse_HasCredentialsStatus_SUCCESS &&
              response->credential_id().size() > 0
          ? FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential
          : FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential);
}

void ChromeOSAuthenticator::OnGetAssertionResponse(
    CtapGetAssertionRequest request,
    GetAssertionCallback callback,
    std::optional<u2f::GetAssertionResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "GetAssertion dbus call failed";
    std::move(callback).Run(GetAssertionStatus::kAuthenticatorResponseInvalid,
                            {});
    return;
  }

  FIDO_LOG(DEBUG) << "GetAssertion status: " << response->status();
  if (response->status() !=
          u2f::GetAssertionResponse_GetAssertionStatus_SUCCESS ||
      response->assertion_size() < 1) {
    std::move(callback).Run(GetAssertionStatus::kUserConsentDenied, {});
    return;
  }

  u2f::Assertion assertion = response->assertion(0);

  std::optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(assertion.authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(GetAssertionStatus::kAuthenticatorResponseInvalid,
                            {});
    return;
  }

  std::vector<uint8_t> signature(assertion.signature().begin(),
                                 assertion.signature().end());
  std::vector<AuthenticatorGetAssertionResponse> authenticator_response;
  authenticator_response.push_back(AuthenticatorGetAssertionResponse(
      std::move(*authenticator_data), std::move(signature),
      FidoTransportProtocol::kInternal));
  const std::string& credential_id = assertion.credential_id();
  authenticator_response.at(0).credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      std::vector<uint8_t>(credential_id.begin(), credential_id.end()));
  std::move(callback).Run(GetAssertionStatus::kSuccess,
                          std::move(authenticator_response));
}

void ChromeOSAuthenticator::HasLegacyU2fCredentialForGetAssertionRequest(
    const CtapGetAssertionRequest& request,
    base::OnceCallback<void(bool has_credential)> callback) {
  u2f::HasCredentialsRequest req;
  req.set_rp_id(request.rp_id);
  if (request.app_id) {
    req.set_app_id(*request.app_id);
  }

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    req.add_credential_id(
        std::string(descriptor.id.begin(), descriptor.id.end()));
  }

  chromeos::U2FClient::Get()->HasLegacyU2FCredentials(
      req,
      base::BindOnce(
          [](base::OnceCallback<void(bool has_credential)> callback,
             std::optional<u2f::HasCredentialsResponse> response) {
            std::move(callback).Run(
                response &&
                response->status() ==
                    u2f::HasCredentialsResponse_HasCredentialsStatus_SUCCESS &&
                response->credential_id().size() > 0);
          },
          std::move(callback)));
}

void ChromeOSAuthenticator::Cancel() {
  if (current_request_id_.empty()) {
    return;
  }

  u2f::CancelWebAuthnFlowRequest req;
  req.set_request_id_str(current_request_id_);
  chromeos::U2FClient::Get()->CancelWebAuthnFlow(
      req, base::BindOnce(&ChromeOSAuthenticator::OnCancelResponse,
                          weak_factory_.GetWeakPtr()));
}

void ChromeOSAuthenticator::OnCancelResponse(
    std::optional<u2f::CancelWebAuthnFlowResponse> response) {
  current_request_id_.clear();

  if (!response) {
    FIDO_LOG(ERROR)
        << "CancelWebAuthnFlow dbus call had no response or timed out";
    return;
  }

  if (!response->canceled()) {
    FIDO_LOG(ERROR) << "Failed to cancel WebAuthn request";
  }
}

void ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailable(
    base::OnceCallback<void(bool is_uvpaa)> callback) {
  chromeos::U2FClient::IsU2FServiceAvailable(base::BindOnce(
      [](base::OnceCallback<void(bool is_uvpaa)> callback,
         bool is_u2f_service_available) {
        if (!is_u2f_service_available) {
          FIDO_LOG(DEBUG) << "CrOS::IsUVPAA() !is_u2f_service_available";
          std::move(callback).Run(false);
          return;
        }

        chromeos::U2FClient::Get()->IsUvpaa(
            u2f::IsUvpaaRequest(),
            base::BindOnce(
                [](base::OnceCallback<void(bool is_available)> callback,
                   std::optional<u2f::IsUvpaaResponse> response) {
                  const bool is_uvpaa = response && !response->not_available();
                  FIDO_LOG(DEBUG) << "CrOS::IsUVPAA()=" << is_uvpaa;
                  std::move(callback).Run(is_uvpaa);
                },
                std::move(callback)));
      },
      std::move(callback)));
}

void ChromeOSAuthenticator::IsPowerButtonModeEnabled(
    base::OnceCallback<void(bool is_enabled)> callback) {
  chromeos::U2FClient::Get()->IsU2FEnabled(
      u2f::IsU2fEnabledRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(bool is_enabled)> callback,
             std::optional<u2f::IsU2fEnabledResponse> response) {
            const bool enabled = response && response->enabled();
            FIDO_LOG(DEBUG) << "CrOS::U2fEnabled()=" << enabled;
            std::move(callback).Run(enabled);
          },
          std::move(callback)));
}

void ChromeOSAuthenticator::IsLacrosSupported(
    base::OnceCallback<void(bool supported)> callback) {
  chromeos::U2FClient::Get()->GetSupportedFeatures(
      u2f::GetSupportedFeaturesRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(bool is_enabled)> callback,
             std::optional<u2f::GetSupportedFeaturesResponse> response) {
            std::move(callback).Run(response && response->support_lacros());
          },
          std::move(callback)));
}

base::WeakPtr<FidoAuthenticator> ChromeOSAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
