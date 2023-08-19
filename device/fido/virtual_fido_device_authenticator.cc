// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_authenticator.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

VirtualFidoDeviceAuthenticator::VirtualFidoDeviceAuthenticator(
    std::unique_ptr<VirtualFidoDevice> virtual_fido_device)
    : FidoDeviceAuthenticator(std::move(virtual_fido_device)) {}

VirtualFidoDeviceAuthenticator::~VirtualFidoDeviceAuthenticator() = default;

void VirtualFidoDeviceAuthenticator::GetPlatformCredentialInfoForRequest(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    GetPlatformCredentialInfoForRequestCallback callback) {
  DCHECK_EQ(device()->DeviceTransport(), FidoTransportProtocol::kInternal);

  VirtualFidoDevice* virtual_device = static_cast<VirtualFidoDevice*>(device());
  std::vector<DiscoverableCredentialMetadata> credentials;
  std::array<uint8_t, kRpIdHashLength> rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(request.rp_id);
  for (const auto& registration :
       virtual_device->mutable_state()->registrations) {
    if (rp_id_hash == registration.second.application_parameter &&
        // Discoverable credentials are found if the allow-list is empty.
        ((request.allow_list.empty() && registration.second.is_resident) ||
         // Otherwise any credentials are found if enumerated in the allowlist.
         base::ranges::any_of(request.allow_list,
                              [&registration](const auto& cred_descriptor) {
                                return cred_descriptor.id == registration.first;
                              }))) {
      credentials.emplace_back(
          AuthenticatorType::kOther, request.rp_id, registration.first,
          registration.second.user.value_or(PublicKeyCredentialUserEntity()));
    }
  }
  FidoRequestHandlerBase::RecognizedCredential has_credentials =
      credentials.empty() ? FidoRequestHandlerBase::RecognizedCredential::
                                kNoRecognizedCredential
                          : FidoRequestHandlerBase::RecognizedCredential::
                                kHasRecognizedCredential;
  std::move(callback).Run(std::move(credentials), has_credentials);
}

}  // namespace device
