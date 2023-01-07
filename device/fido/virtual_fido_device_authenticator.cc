// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_authenticator.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_device_authenticator.h"

namespace device {

VirtualFidoDeviceAuthenticator::VirtualFidoDeviceAuthenticator(
    std::unique_ptr<VirtualFidoDevice> virtual_fido_device)
    : FidoDeviceAuthenticator(std::move(virtual_fido_device)) {}

VirtualFidoDeviceAuthenticator::~VirtualFidoDeviceAuthenticator() = default;

void VirtualFidoDeviceAuthenticator::GetCredentialInformationForRequest(
    const CtapGetAssertionRequest& request,
    GetCredentialInformationForRequestCallback callback) {
  DCHECK_EQ(device()->DeviceTransport(), FidoTransportProtocol::kInternal);

  VirtualFidoDevice* virtual_device = static_cast<VirtualFidoDevice*>(device());
  if (request.allow_list.empty()) {
    // Resident credentials request.
    std::vector<DiscoverableCredentialMetadata> credentials;
    for (const auto& registration :
         virtual_device->mutable_state()->registrations) {
      if (registration.second.is_resident &&
          registration.second.rp->id == request.rp_id) {
        credentials.emplace_back(request.rp_id, registration.first,
                                 *registration.second.user);
      }
    }
    bool has_credentials = credentials.size() > 0;
    std::move(callback).Run(std::move(credentials), has_credentials);
    return;
  }
  // Non resident credentials request.
  for (const auto& registration :
       virtual_device->mutable_state()->registrations) {
    for (const auto& allow_list_credential : request.allow_list) {
      if (allow_list_credential.id == registration.first) {
        std::move(callback).Run(/*credentials=*/{}, true);
        return;
      }
    }
  }
  std::move(callback).Run(/*credentials=*/{}, false);
}

}  // namespace device
