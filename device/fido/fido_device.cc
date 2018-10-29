// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "device/base/features.h"
#include "device/fido/ctap_empty_authenticator_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoDevice::FidoDevice() = default;
FidoDevice::~FidoDevice() = default;

base::string16 FidoDevice::GetDisplayName() const {
  const auto id = GetId();
  return base::string16(id.begin(), id.end());
}

bool FidoDevice::IsInPairingMode() const {
  return false;
}

void FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  if (base::FeatureList::IsEnabled(kNewCtap2Device)) {
    // Set the protocol version to CTAP2 for the purpose of sending the GetInfo
    // request. The correct value will be set in the callback based on the
    // device response.
    supported_protocol_ = ProtocolVersion::kCtap;
    DeviceTransact(AuthenticatorGetInfoRequest().Serialize(),
                   base::BindOnce(&FidoDevice::OnDeviceInfoReceived,
                                  GetWeakPtr(), std::move(done)));
  } else {
    supported_protocol_ = ProtocolVersion::kU2f;
    std::move(done).Run();
  }
}

bool FidoDevice::SupportedProtocolIsInitialized() {
  return (supported_protocol_ == ProtocolVersion::kU2f && !device_info_) ||
         (supported_protocol_ == ProtocolVersion::kCtap && device_info_);
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
  if (!get_info_response || !base::ContainsKey(get_info_response->versions(),
                                               ProtocolVersion::kCtap)) {
    supported_protocol_ = ProtocolVersion::kU2f;
  } else {
    supported_protocol_ = ProtocolVersion::kCtap;
    device_info_ = std::move(*get_info_response);
  }
  std::move(done).Run();
}

void FidoDevice::SetDeviceInfo(AuthenticatorGetInfoResponse device_info) {
  device_info_ = std::move(device_info);
}

}  // namespace device
