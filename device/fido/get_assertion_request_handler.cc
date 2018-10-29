// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/get_assertion_task.h"

namespace device {

namespace {

// PublicKeyUserEntity field in GetAssertion response is optional with the
// following constraints:
// - If assertion has been made without user verification, user identifiable
//   information must not be included.
// - For resident key credentials, user id of the user entity is mandatory.
// - When multiple accounts exist for specified RP ID, user entity is
//   mandatory.
// TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
// published.
bool CheckRequirementsOnResponseUserEntity(
    const CtapGetAssertionRequest& request,
    const AuthenticatorGetAssertionResponse& response) {
  // If assertion has been made without user verification, user identifiable
  // information must not be included.
  const auto& user_entity = response.user_entity();
  const bool has_user_identifying_info =
      user_entity && (user_entity->user_display_name() ||
                      user_entity->user_name() || user_entity->user_icon_url());
  if (!response.auth_data().obtained_user_verification() &&
      has_user_identifying_info) {
    return false;
  }

  // For resident key credentials, user id of the user entity is mandatory.
  if ((!request.allow_list() || request.allow_list()->empty()) &&
      !user_entity) {
    return false;
  }

  // When multiple accounts exist for specified RP ID, user entity is mandatory.
  if (response.num_credentials().value_or(0u) > 1 && !user_entity) {
    return false;
  }

  return true;
}

// Checks whether credential ID returned from the authenticator and transport
// type used matches the transport type and credential ID defined in
// PublicKeyCredentialDescriptor of the allowed list. If the device has resident
// key support, returned credential ID may be resident credential. Thus,
// returned credential ID need not be in allowed list.
// TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
// published.
bool CheckResponseCredentialIdMatchesRequestAllowList(
    const FidoAuthenticator& authenticator,
    const CtapGetAssertionRequest request,
    const AuthenticatorGetAssertionResponse& response) {
  const auto& allow_list = request.allow_list();
  if (!allow_list || allow_list->empty()) {
    // Allow list can't be empty for authenticators w/o resident key support.
    return authenticator.Options().supports_resident_key();
  }
  // Credential ID may be omitted if allow list has size 1. Otherwise, it needs
  // to match.
  const auto transport_used = authenticator.AuthenticatorTransport();
  return (allow_list->size() == 1 && !response.credential()) ||
         std::any_of(allow_list->cbegin(), allow_list->cend(),
                     [&response, transport_used](const auto& credential) {
                       return credential.id() == response.raw_credential_id() &&
                              base::ContainsKey(credential.transports(),
                                                transport_used);
                     });
}

// Checks UserVerificationRequirement enum passed from the relying party is
// compatible with the authenticator, and updates the request to the
// "effective" user verification requirement.
// https://w3c.github.io/webauthn/#effective-user-verification-requirement-for-assertion
bool CheckUserVerificationCompatible(FidoAuthenticator* authenticator,
                                     CtapGetAssertionRequest* request) {
  const auto uv_availability =
      authenticator->Options().user_verification_availability();

  switch (request->user_verification()) {
    case UserVerificationRequirement::kRequired:
      return uv_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured;

    case UserVerificationRequirement::kDiscouraged:
      return true;

    case UserVerificationRequirement::kPreferred:
      if (uv_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
        request->SetUserVerification(UserVerificationRequirement::kRequired);
      } else {
        request->SetUserVerification(UserVerificationRequirement::kDiscouraged);
      }
      return true;
  }

  NOTREACHED();
  return false;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const CtapGetAssertionRequest& request) {
  const base::flat_set<FidoTransportProtocol> kAllTransports = {
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};

  // TODO(https://crbug.com/874479): |allowed_list| will |has_value| even if the
  // WebAuthn request has `allowCredential` undefined.
  const auto& allowed_list = request.allow_list();
  if (!allowed_list || allowed_list->empty()) {
    return kAllTransports;
  }

  base::flat_set<FidoTransportProtocol> transports;
  for (const auto credential : *allowed_list) {
    if (credential.transports().empty())
      return kAllTransports;
    transports.insert(credential.transports().begin(),
                      credential.transports().end());
  }

  return transports;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedAndConfiguredByRP(
    const CtapGetAssertionRequest& request) {
  auto transports = GetTransportsAllowedByRP(request);
  if (!request.cable_extension())
    transports.erase(FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  return transports;
}

}  // namespace

GetAssertionRequestHandler::GetAssertionRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapGetAssertionRequest request,
    SignResponseCallback completion_callback)
    : FidoRequestHandler(
          connector,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedAndConfiguredByRP(request)),
          std::move(completion_callback)),
      request_(std::move(request)),
      weak_factory_(this) {
  transport_availability_info().rp_id = request_.rp_id();
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kGetAssertion;

  if (base::ContainsKey(
          transport_availability_info().available_transports,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)) {
    DCHECK(request_.cable_extension());
    auto discovery =
        FidoDeviceDiscovery::CreateCable(*request_.cable_extension());
    discovery->set_observer(this);
    discoveries().push_back(std::move(discovery));
  }

  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

void GetAssertionRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  // The user verification field of the request may be adjusted to the
  // authenticator, so we need to make a copy.
  CtapGetAssertionRequest request_copy = request_;
  if (!CheckUserVerificationCompatible(authenticator, &request_copy)) {
    return;
  }

  authenticator->GetAssertion(
      std::move(request_copy),
      base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void GetAssertionRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    OnAuthenticatorResponse(authenticator, response_code, base::nullopt);
    return;
  }

  if (!response || !request_.CheckResponseRpIdHash(response->GetRpIdHash()) ||
      !CheckResponseCredentialIdMatchesRequestAllowList(*authenticator,
                                                        request_, *response) ||
      !CheckRequirementsOnResponseUserEntity(request_, *response)) {
    OnAuthenticatorResponse(
        authenticator, CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  OnAuthenticatorResponse(authenticator, response_code, std::move(response));
}

}  // namespace device
