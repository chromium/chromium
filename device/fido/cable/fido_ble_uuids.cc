// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_ble_uuids.h"

#include "build/build_config.h"

namespace device {

const char kFidoServiceUUID[] = "0000fffd-0000-1000-8000-00805f9b34fb";
const char kFidoControlPointUUID[] = "f1d0fff1-deaa-ecee-b42f-c9ba7ed623bb";
const char kFidoStatusUUID[] = "f1d0fff2-deaa-ecee-b42f-c9ba7ed623bb";
const char kFidoControlPointLengthUUID[] =
    "f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb";
const char kFidoServiceRevisionUUID[] = "00002a28-0000-1000-8000-00805f9b34fb";
const char kFidoServiceRevisionBitfieldUUID[] =
    "f1d0fff4-deaa-ecee-b42f-c9ba7ed623bb";

const char kGoogleCableUUID128[] = "0000fde2-0000-1000-8000-00805f9b34fb";
const char kGoogleCableUUID16[] = "fde2";
const uint8_t kGoogleCableUUID[16] = {
    0x00, 0x00, 0xfd, 0xe2, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
};

const char kFIDOCableUUID128[] = "0000fff9-0000-1000-8000-00805f9b34fb";
const uint8_t kFIDOCableUUID[16] = {
    0x00, 0x00, 0xff, 0xf9, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
};

}  // namespace device
