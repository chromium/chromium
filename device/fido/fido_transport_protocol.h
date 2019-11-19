// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
#define DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"

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
  kCloudAssistedBluetoothLowEnergy = 3,
  kInternal = 4,
  kMaxValue = kInternal,
};

// String representation of above FidoTransportProtocol enum.
extern const char kUsbHumanInterfaceDevice[];
extern const char kNearFieldCommunication[];
extern const char kBluetoothLowEnergy[];
extern const char kCloudAssistedBluetoothLowEnergy[];
extern const char kInternal[];

COMPONENT_EXPORT(DEVICE_FIDO)
base::flat_set<FidoTransportProtocol> GetAllTransportProtocols();

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    base::StringPiece protocol);

COMPONENT_EXPORT(DEVICE_FIDO)
std::string ToString(FidoTransportProtocol protocol);

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TRANSPORT_PROTOCOL_H_
