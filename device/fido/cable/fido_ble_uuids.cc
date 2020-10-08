// Copyright 2017 The Chromium Authors. All rights reserved.
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

const char kCableAdvertisementUUID16[] = "fde2";
const char kCableAdvertisementUUID128[] =
    "0000fde2-0000-1000-8000-00805f9b34fb";

const uint8_t kCableAdvertisementUUID[16] = {
    0x00, 0x00, 0xfd, 0xe2, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
};

}  // namespace device
