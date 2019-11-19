// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/ctap_empty_authenticator_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoDevice::FidoDevice() = default;
FidoDevice::~FidoDevice() = default;

void FidoDevice::TryWink(base::OnceClosure callback) {
  std::move(callback).Run();
}

base::string16 FidoDevice::GetDisplayName() const {
  const auto id = GetId();
  return base::string16(id.begin(), id.end());
}

bool FidoDevice::IsInPairingMode() const {
  NOTREACHED();
  return false;
}

bool FidoDevice::IsPaired() const {
  NOTREACHED();
  return false;
}

bool FidoDevice::RequiresBlePairingPin() const {
  NOTREACHED();
  return true;
}

void FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  // Set the protocol version to CTAP2 for the purpose of sending the GetInfo
  // request. The correct value will be set in the callback based on the
  // device response.
  supported_protocol_ = ProtocolVersion::kCtap2;
  FIDO_LOG(DEBUG)
      << "Sending CTAP2 AuthenticatorGetInfo request to authenticator.";
  DeviceTransact(AuthenticatorGetInfoRequest().Serialize(),
                 base::BindOnce(&FidoDevice::OnDeviceInfoReceived, GetWeakPtr(),
                                std::move(done)));
}

bool FidoDevice::SupportedProtocolIsInitialized() {
  return (supported_protocol_ == ProtocolVersion::kU2f && !device_info_) ||
         (supported_protocol_ == ProtocolVersion::kCtap2 && device_info_);
}

void FidoDevice::OnDeviceInfoReceived(
    base::OnceClosure done,
    base::Optional<std::vector<uint8_t>> response) {
  // TODO(hongjunchoi): Add tests that verify this behavior.
  if (state_ == FidoDevice::State::kDeviceError)
    return;

  state_ = FidoDevice::State::kReady;
  base::Optional<AuthenticatorGetInfoResponse> get_info_response =
      response ? ReadCTAPGetInfoResponse(*response) : base::nullopt;
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

}  // namespace device
