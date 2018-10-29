// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_DEVICE_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_DEVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "device/fido/ble/fido_ble_connection.h"
#include "device/fido/ble/fido_ble_transaction.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

class BluetoothAdapter;
class FidoBleFrame;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleDevice : public FidoDevice {
 public:
  using FrameCallback = FidoBleTransaction::FrameCallback;
  FidoBleDevice(BluetoothAdapter* adapter, std::string address);
  explicit FidoBleDevice(std::unique_ptr<FidoBleConnection> connection);
  ~FidoBleDevice() override;

  void Connect();
  void SendPing(std::vector<uint8_t> data, DeviceCallback callback);
  static std::string GetId(base::StringPiece address);

  // FidoDevice:
  void TryWink(WinkCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  FidoTransportProtocol DeviceTransport() const override;

  // Returns whether or not the underlying BLE device is currently in pairing
  // mode by investigating the advertisement payload.
  bool IsInPairingMode() const override;

  FidoBleConnection::ReadCallback GetReadCallbackForTesting();

 protected:
  // FidoDevice:
  void DeviceTransact(std::vector<uint8_t> command,
                      DeviceCallback callback) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  virtual void OnResponseFrame(FrameCallback callback,
                               base::Optional<FidoBleFrame> frame);
  void Transition();
  void AddToPendingFrames(FidoBleDeviceCommand cmd,
                          std::vector<uint8_t> request,
                          DeviceCallback callback);
  void ResetTransaction();

 private:
  void OnConnected(bool success);
  void OnStatusMessage(std::vector<uint8_t> data);

  void ReadControlPointLength();
  void OnReadControlPointLength(base::Optional<uint16_t> length);

  void SendPendingRequestFrame();
  void SendRequestFrame(FidoBleFrame frame, FrameCallback callback);

  void StartTimeout();
  void StopTimeout();
  void OnTimeout();

  void OnBleResponseReceived(DeviceCallback callback,
                             base::Optional<FidoBleFrame> frame);
  void ProcessBleDeviceError(base::span<const uint8_t> data);

  base::OneShotTimer timer_;

  std::unique_ptr<FidoBleConnection> connection_;
  uint16_t control_point_length_ = 0;

  base::queue<std::pair<FidoBleFrame, FrameCallback>> pending_frames_;
  base::Optional<FidoBleTransaction> transaction_;

  base::WeakPtrFactory<FidoBleDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoBleDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_DEVICE_H_
