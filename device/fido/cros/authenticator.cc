// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "device/fido/cros/authenticator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/opaque_attestation_statement.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace device {

ChromeOSAuthenticator::ChromeOSAuthenticator(
    base::RepeatingCallback<uint32_t()> generate_request_id_callback)
    : generate_request_id_callback_(std::move(generate_request_id_callback)),
      weak_factory_(this) {}

ChromeOSAuthenticator::~ChromeOSAuthenticator() {}

std::string ChromeOSAuthenticator::GetId() const {
  return "ChromeOSAuthenticator";
}

namespace {

AuthenticatorSupportedOptions ChromeOSAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device = true;
  // TODO(yichengli): change supports_resident_key to true once it's supported.
  options.supports_resident_key = false;
  // Even if the user has no fingerprints enrolled, we will have password
  // as fallback.
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  return options;
}

}  // namespace

const base::Optional<AuthenticatorSupportedOptions>&
ChromeOSAuthenticator::Options() const {
  static const base::Optional<AuthenticatorSupportedOptions> options =
      ChromeOSAuthenticatorOptions();
  return options;
}

base::Optional<FidoTransportProtocol>
ChromeOSAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

void ChromeOSAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void ChromeOSAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                           MakeCredentialCallback callback) {
  u2f::MakeCredentialRequest req;
  // Requests with UserPresence get upgraded to UserVerification unless
  // verification is explicitly discouraged.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged)
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
  if (request.user.display_name.has_value())
    req.set_user_display_name(request.user.display_name.value());
  req.set_resident_credential(request.resident_key_required);
  DCHECK(generate_request_id_callback_);
  DCHECK_EQ(current_request_id_, 0u);
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.exclude_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_excluded_credential_id(std::string(id.begin(), id.end()));
  }
  if (request.app_id) {
    req.set_app_id_exclude(*request.app_id);
  }

  chromeos::U2FClient::Get()->MakeCredential(
      req, base::BindOnce(&ChromeOSAuthenticator::OnMakeCredentialResponse,
                          weak_factory_.GetWeakPtr(), std::move(request),
                          std::move(callback)));
}

void ChromeOSAuthenticator::OnMakeCredentialResponse(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback,
    base::Optional<u2f::MakeCredentialResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "MakeCredential dbus call failed";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  FIDO_LOG(DEBUG) << "Make credential status: " << response->status();
  if (response->status() !=
      u2f::MakeCredentialResponse_MakeCredentialStatus_SUCCESS) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                            base::nullopt);
    return;
  }

  base::Optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(response->authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  base::Optional<cbor::Value> statement_map = cbor::Reader::Read(
      base::as_bytes(base::make_span(response->attestation_statement())));
  if (!statement_map ||
      statement_map.value().type() != cbor::Value::Type::MAP) {
    FIDO_LOG(ERROR) << "Attestation statement is not a CBOR map.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }
  auto statement = std::make_unique<OpaqueAttestationStatement>(
      response->attestation_format(), std::move(*statement_map));

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          AuthenticatorMakeCredentialResponse(
                              FidoTransportProtocol::kInternal,
                              AttestationObject(std::move(*authenticator_data),
                                                std::move(statement))));
}

void ChromeOSAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                         CtapGetAssertionOptions options,
                                         GetAssertionCallback callback) {
  u2f::GetAssertionRequest req;
  // Requests with UserPresence get upgraded to UserVerification unless
  // verification is explicitly discouraged.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged)
          ? u2f::VERIFICATION_USER_PRESENCE
          : u2f::VERIFICATION_USER_VERIFICATION);
  req.set_rp_id(request.rp_id);
  if (request.app_id) {
    req.set_app_id(*request.app_id);
  }
  req.set_client_data_hash(std::string(request.client_data_hash.begin(),
                                       request.client_data_hash.end()));
  DCHECK(generate_request_id_callback_);
  DCHECK_EQ(current_request_id_, 0u);
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_allowed_credential_id(std::string(id.begin(), id.end()));
  }

  chromeos::U2FClient::Get()->GetAssertion(
      req, base::BindOnce(&ChromeOSAuthenticator::OnGetAssertionResponse,
                          weak_factory_.GetWeakPtr(), std::move(request),
                          std::move(callback)));
}

void ChromeOSAuthenticator::OnGetAssertionResponse(
    CtapGetAssertionRequest request,
    GetAssertionCallback callback,
    base::Optional<u2f::GetAssertionResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "GetAssertion dbus call failed";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  FIDO_LOG(DEBUG) << "GetAssertion status: " << response->status();
  if (response->status() !=
          u2f::GetAssertionResponse_GetAssertionStatus_SUCCESS ||
      response->assertion_size() < 1) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                            base::nullopt);
    return;
  }

  u2f::Assertion assertion = response->assertion(0);

  base::Optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(assertion.authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  std::vector<uint8_t> signature(assertion.signature().begin(),
                                 assertion.signature().end());
  AuthenticatorGetAssertionResponse authenticator_response(
      std::move(*authenticator_data), std::move(signature));
  const std::string& credential_id = assertion.credential_id();
  authenticator_response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      std::vector<uint8_t>(credential_id.begin(), credential_id.end())));
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(authenticator_response));
}

void ChromeOSAuthenticator::HasCredentialForGetAssertionRequest(
    const CtapGetAssertionRequest& request,
    base::OnceCallback<void(bool has_credential)> callback) {
  u2f::HasCredentialsRequest req;
  req.set_rp_id(request.rp_id);
  if (request.app_id) {
    req.set_app_id(*request.app_id);
  }

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_credential_id(std::string(id.begin(), id.end()));
  }

  chromeos::U2FClient::Get()->HasCredentials(
      req,
      base::BindOnce(
          [](base::OnceCallback<void(bool has_credential)> callback,
             base::Optional<u2f::HasCredentialsResponse> response) {
            std::move(callback).Run(
                response &&
                response->status() ==
                    u2f::HasCredentialsResponse_HasCredentialsStatus_SUCCESS &&
                response->credential_id().size() > 0);
          },
          std::move(callback)));
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
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_credential_id(std::string(id.begin(), id.end()));
  }

  chromeos::U2FClient::Get()->HasLegacyU2FCredentials(
      req,
      base::BindOnce(
          [](base::OnceCallback<void(bool has_credential)> callback,
             base::Optional<u2f::HasCredentialsResponse> response) {
            std::move(callback).Run(
                response &&
                response->status() ==
                    u2f::HasCredentialsResponse_HasCredentialsStatus_SUCCESS &&
                response->credential_id().size() > 0);
          },
          std::move(callback)));
}

void ChromeOSAuthenticator::Cancel() {
  if (current_request_id_ == 0u)
    return;

  u2f::CancelWebAuthnFlowRequest req;
  req.set_request_id(current_request_id_);
  chromeos::U2FClient::Get()->CancelWebAuthnFlow(
      req, base::BindOnce(&ChromeOSAuthenticator::OnCancelResponse,
                          weak_factory_.GetWeakPtr()));
}

void ChromeOSAuthenticator::OnCancelResponse(
    base::Optional<u2f::CancelWebAuthnFlowResponse> response) {
  current_request_id_ = 0u;

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
    base::OnceCallback<void(bool is_available)> callback) {
  chromeos::U2FClient::Get()->IsUvpaa(
      u2f::IsUvpaaRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(bool is_available)> callback,
             base::Optional<u2f::IsUvpaaResponse> response) {
            std::move(callback).Run(response && response->available());
          },
          std::move(callback)));
}

void ChromeOSAuthenticator::IsPowerButtonModeEnabled(
    base::OnceCallback<void(bool is_enabled)> callback) {
  chromeos::U2FClient::Get()->IsU2FEnabled(
      u2f::IsUvpaaRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(bool is_enabled)> callback,
             base::Optional<u2f::IsUvpaaResponse> response) {
            std::move(callback).Run(response && response->available());
          },
          std::move(callback)));
}

bool ChromeOSAuthenticator::IsInPairingMode() const {
  return false;
}

bool ChromeOSAuthenticator::IsPaired() const {
  return false;
}

bool ChromeOSAuthenticator::RequiresBlePairingPin() const {
  return false;
}

bool ChromeOSAuthenticator::IsChromeOSAuthenticator() const {
  return true;
}

base::WeakPtr<FidoAuthenticator> ChromeOSAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
