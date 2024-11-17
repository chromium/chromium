// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
#define DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "device/fido/fido_types.h"

namespace device {

// This enum represents the transport protocols over which Fido WebAuthN API is
// currently supported.
// This enum is used for UMA histograms and the values should not be
// reassigned. New transports added should be reflected in the
// WebAuthenticationFidoTransport enum.
enum class FidoTransportProtocol : uint8_t {
  kUsbHumanInterfaceDevice = 0,
  kNearFieldCommunication = 1,
  kBluetoothLowEnergy = 2,
  kHybrid = 3,
  kInternal = 4,
  kDeprecatedAoa = 5,
  kMaxValue = kDeprecatedAoa,
};

// String representation of above FidoTransportProtocol enum.
inline constexpr std::string_view kUsbHumanInterfaceDevice = "usb";
inline constexpr std::string_view kNearFieldCommunication = "nfc";
inline constexpr std::string_view kBluetoothLowEnergy = "ble";
inline constexpr std::string_view kHybrid = "hybrid";
inline constexpr std::string_view kInternal = "internal";

COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    std::string_view protocol);

COMPONENT_EXPORT(DEVICE_FIDO)
std::string_view ToString(FidoTransportProtocol protocol);

COMPONENT_EXPORT(DEVICE_FIDO)
AuthenticatorAttachment AuthenticatorAttachmentFromTransport(
    FidoTransportProtocol transport);

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
