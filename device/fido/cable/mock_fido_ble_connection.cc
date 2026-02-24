// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/mock_fido_ble_connection.h"

#include "base/functional/callback_helpers.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/cable/fido_ble_uuids.h"

#include <utility>

namespace device {

MockFidoBleConnection::MockFidoBleConnection(BluetoothAdapter* adapter,
                                             std::string device_address)
    : FidoBleConnection(adapter,
                        std::move(device_address),
                        BluetoothUUID(kFidoServiceUUID),
                        base::DoNothing()) {}

MockFidoBleConnection::~MockFidoBleConnection() = default;

void MockFidoBleConnection::Connect(ConnectionCallback callback) {
  ConnectPtr(&callback);
}

void MockFidoBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  ReadControlPointLengthPtr(&callback);
}

void MockFidoBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                              WriteCallback callback) {
  WriteControlPointPtr(data, &callback);
}

}  // namespace device
