// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_transport_protocol.h"

#include "base/notreached.h"

namespace device {

std::optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    std::string_view protocol) {
  if (protocol == kUsbHumanInterfaceDevice)
    return FidoTransportProtocol::kUsbHumanInterfaceDevice;
  else if (protocol == kNearFieldCommunication)
    return FidoTransportProtocol::kNearFieldCommunication;
  else if (protocol == kBluetoothLowEnergy)
    return FidoTransportProtocol::kBluetoothLowEnergy;
  else if (protocol == kHybrid)
    return FidoTransportProtocol::kHybrid;
  else if (protocol == kInternal)
    return FidoTransportProtocol::kInternal;
  else
    return std::nullopt;
}

std::string_view ToString(FidoTransportProtocol protocol) {
  switch (protocol) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return kUsbHumanInterfaceDevice;
    case FidoTransportProtocol::kNearFieldCommunication:
      return kNearFieldCommunication;
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return kBluetoothLowEnergy;
    case FidoTransportProtocol::kHybrid:
      return kHybrid;
    case FidoTransportProtocol::kInternal:
      return kInternal;
    case FidoTransportProtocol::kDeprecatedAoa:
      NOTREACHED();
  }
}

AuthenticatorAttachment AuthenticatorAttachmentFromTransport(
    FidoTransportProtocol transport) {
  switch (transport) {
    case FidoTransportProtocol::kInternal:
      return AuthenticatorAttachment::kPlatform;
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
    case FidoTransportProtocol::kNearFieldCommunication:
    case FidoTransportProtocol::kBluetoothLowEnergy:
    case FidoTransportProtocol::kHybrid:
      return AuthenticatorAttachment::kCrossPlatform;
    case FidoTransportProtocol::kDeprecatedAoa:
      NOTREACHED();
  }
}

}  // namespace device
