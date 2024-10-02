// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoDevice::FidoDevice() = default;
FidoDevice::~FidoDevice() = default;

void FidoDevice::TryWink(base::OnceClosure callback) {
  std::move(callback).Run();
}

std::string FidoDevice::GetDisplayName() const {
  return GetId();
}

cablev2::FidoTunnelDevice* FidoDevice::GetTunnelDevice() {
  return nullptr;
}

void FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  // Set the protocol version to CTAP2 for the purpose of sending the GetInfo
  // request. The correct value will be set in the callback based on the
  // device response.
  supported_protocol_ = ProtocolVersion::kCtap2;
  FIDO_LOG(DEBUG)
      << "Sending CTAP2 AuthenticatorGetInfo request to authenticator.";
  DeviceTransact(
      {static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetInfo)},
      base::BindOnce(&FidoDevice::OnDeviceInfoReceived, GetWeakPtr(),
                     std::move(done)));
}

bool FidoDevice::SupportedProtocolIsInitialized() {
  return (supported_protocol_ == ProtocolVersion::kU2f && !device_info_) ||
         (supported_protocol_ == ProtocolVersion::kCtap2 && device_info_);
}

void FidoDevice::OnDeviceInfoReceived(
    base::OnceClosure done,
    std::optional<std::vector<uint8_t>> response) {
  // TODO(hongjunchoi): Add tests that verify this behavior.
  if (state_ == FidoDevice::State::kDeviceError) {
    return;
  }

  state_ = FidoDevice::State::kReady;
  std::optional<AuthenticatorGetInfoResponse> get_info_response =
      response ? ReadCTAPGetInfoResponse(*response) : std::nullopt;
  if (!get_info_response ||
      !base::Contains(get_info_response->versions, ProtocolVersion::kCtap2)) {
    supported_protocol_ = ProtocolVersion::kU2f;
    needs_explicit_wink_ = true;
    FIDO_LOG(DEBUG) << "The device only supports the U2F protocol.";
  } else {
    supported_protocol_ = ProtocolVersion::kCtap2;
    device_info_ = std::move(*get_info_response);
    FIDO_LOG(DEBUG) << "The device supports the CTAP2 protocol.";
  }
  std::move(done).Run();
}

void FidoDevice::SetDeviceInfo(AuthenticatorGetInfoResponse device_info) {
  device_info_ = std::move(device_info);
}

bool FidoDevice::NoSilentRequests() const {
  // caBLE devices do not support silent requests.
  const auto transport = DeviceTransport();
  return transport == FidoTransportProtocol::kHybrid;
}

// static
bool FidoDevice::IsStatusForUnrecognisedCredentialID(
    CtapDeviceResponseCode status) {
  return status == CtapDeviceResponseCode::kCtap2ErrInvalidCredential ||
         status == CtapDeviceResponseCode::kCtap2ErrNoCredentials ||
         status == CtapDeviceResponseCode::kCtap2ErrLimitExceeded ||
         status == CtapDeviceResponseCode::kCtap2ErrRequestTooLarge ||
         // Some alwaysUv devices return this, even for up=false. See
         // crbug.com/1443039.
         status == CtapDeviceResponseCode::kCtap2ErrPinRequired;
}

}  // namespace device
