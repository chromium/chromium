// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_MOCK_FIDO_BLE_CONNECTION_H_
#define DEVICE_FIDO_CABLE_MOCK_FIDO_BLE_CONNECTION_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "device/fido/cable/fido_ble_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothAdapter;

class MockFidoBleConnection : public FidoBleConnection {
 public:
  MockFidoBleConnection(BluetoothAdapter* adapter, std::string device_address);

  MockFidoBleConnection(const MockFidoBleConnection&) = delete;
  MockFidoBleConnection& operator=(const MockFidoBleConnection&) = delete;

  ~MockFidoBleConnection() override;

  // GMock cannot mock a method taking a move-only type.
  // TODO(crbug.com/40524294): Remove these workarounds once support for
  // move-only types is added to GMock.
  MOCK_METHOD1(ConnectPtr, void(ConnectionCallback* cb));
  MOCK_METHOD1(ReadControlPointLengthPtr, void(ControlPointLengthCallback* cb));
  MOCK_METHOD2(WriteControlPointPtr,
               void(const std::vector<uint8_t>& data, WriteCallback* cb));

  void Connect(ConnectionCallback cb) override;
  void ReadControlPointLength(ControlPointLengthCallback cb) override;
  void WriteControlPoint(const std::vector<uint8_t>& data,
                         WriteCallback cb) override;

  ReadCallback& read_callback() { return read_callback_; }
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_MOCK_FIDO_BLE_CONNECTION_H_
