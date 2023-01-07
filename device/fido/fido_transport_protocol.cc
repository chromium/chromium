// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_transport_protocol.h"

namespace device {

const char kUsbHumanInterfaceDevice[] = "usb";
const char kNearFieldCommunication[] = "nfc";
const char kBluetoothLowEnergy[] = "ble";
const char kCable[] = "hybrid";
const char kHybrid[] = "hybrid";
const char kInternal[] = "internal";

absl::optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    base::StringPiece protocol) {
  if (protocol == kUsbHumanInterfaceDevice)
    return FidoTransportProtocol::kUsbHumanInterfaceDevice;
  else if (protocol == kNearFieldCommunication)
    return FidoTransportProtocol::kNearFieldCommunication;
  else if (protocol == kBluetoothLowEnergy)
    return FidoTransportProtocol::kBluetoothLowEnergy;
  else if (protocol == kHybrid)
    return FidoTransportProtocol::kHybrid;
  else if (protocol == kCable)
    // This is the old name for "hybrid".
    return FidoTransportProtocol::kHybrid;
  else if (protocol == kInternal)
    return FidoTransportProtocol::kInternal;
  else
    return absl::nullopt;
}

base::StringPiece ToString(FidoTransportProtocol protocol) {
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
    case FidoTransportProtocol::kAndroidAccessory:
      // The Android accessory transport is not exposed to the outside world and
      // is considered a flavour of caBLE.
      return kHybrid;
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
    case FidoTransportProtocol::kAndroidAccessory:
      return AuthenticatorAttachment::kCrossPlatform;
  }
}

}  // namespace device
