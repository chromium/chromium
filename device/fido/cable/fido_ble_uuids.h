// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_BLE_UUIDS_H_
#define DEVICE_FIDO_CABLE_FIDO_BLE_UUIDS_H_

#include <stdint.h>

#include "base/component_export.h"

namespace device {

// FIDO GATT Service's UUIDs as defined by the standard:
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#gatt-service-description
//
// For details on how the short UUIDs for FIDO Service (0xFFFD) and FIDO Service
// Revision (0x2A28) were converted to the long canonical ones, see
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoServiceUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoControlPointUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoStatusUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoControlPointLengthUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoServiceRevisionUUID[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kFidoServiceRevisionBitfieldUUID[];

// kGoogleCableUUID128 is a 16-bit UUID assigned to Google that we use for
// caBLE.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kGoogleCableUUID128[];
// kGoogleCableUUID16 is the 16-bit version of |kGoogleCableUUID128|.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kGoogleCableUUID16[];
// kGoogleCableUUID is the binary form of
// |kGoogleCableUUID128|, the UUID allocated to Google for caBLE adverts.
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kGoogleCableUUID[16];

// kFIDOCableUUID128 is a 16-bit UUID assigned to FIDO that is used for
// caBLEv2. (For now, caBLEv2 devices can also use |kGoogleCableUUID|.)
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFIDOCableUUID128[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kFIDOCableUUID[16];

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_BLE_UUIDS_H_
