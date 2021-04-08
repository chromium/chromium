// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_transport_protocol.h"

namespace device {

const char kUsbHumanInterfaceDevice[] = "usb";
const char kNearFieldCommunication[] = "nfc";
const char kBluetoothLowEnergy[] = "ble";
const char kCloudAssistedBluetoothLowEnergy[] = "cable";
const char kInternal[] = "internal";

base::Optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    base::StringPiece protocol) {
  if (protocol == kUsbHumanInterfaceDevice)
    return FidoTransportProtocol::kUsbHumanInterfaceDevice;
  else if (protocol == kNearFieldCommunication)
    return FidoTransportProtocol::kNearFieldCommunication;
  else if (protocol == kBluetoothLowEnergy)
    return FidoTransportProtocol::kBluetoothLowEnergy;
  else if (protocol == kCloudAssistedBluetoothLowEnergy)
    return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
  else if (protocol == kInternal)
    return FidoTransportProtocol::kInternal;
  else
    return base::nullopt;
}

COMPONENT_EXPORT(DEVICE_FIDO)
base::StringPiece ToString(FidoTransportProtocol protocol) {
  switch (protocol) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return kUsbHumanInterfaceDevice;
    case FidoTransportProtocol::kNearFieldCommunication:
      return kNearFieldCommunication;
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return kBluetoothLowEnergy;
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      return kCloudAssistedBluetoothLowEnergy;
    case FidoTransportProtocol::kInternal:
      return kInternal;
    case FidoTransportProtocol::kAndroidAccessory:
      // The Android accessory transport is not exposed to the outside world and
      // is considered a flavour of caBLE.
      return kCloudAssistedBluetoothLowEnergy;
  }
}

}  // namespace device
