// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
#define DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  kAndroidAccessory = 5,
  kMaxValue = kAndroidAccessory,
};

// String representation of above FidoTransportProtocol enum.
extern const char kUsbHumanInterfaceDevice[];
extern const char kNearFieldCommunication[];
extern const char kBluetoothLowEnergy[];
extern const char kHybrid[];
extern const char kInternal[];

COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    base::StringPiece protocol);

COMPONENT_EXPORT(DEVICE_FIDO)
base::StringPiece ToString(FidoTransportProtocol protocol);

COMPONENT_EXPORT(DEVICE_FIDO)
AuthenticatorAttachment AuthenticatorAttachmentFromTransport(
    FidoTransportProtocol transport);

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
