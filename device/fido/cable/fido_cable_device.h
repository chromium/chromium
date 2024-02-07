// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "crypto/aead.h"
#include "device/fido/cable/fido_ble_connection.h"
#include "device/fido/cable/fido_ble_transaction.h"
#include "device/fido/fido_device.h"

namespace device {

class BluetoothAdapter;
class FidoBleFrame;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDevice : public FidoDevice {
 public:
  using FrameCallback = FidoBleTransaction::FrameCallback;

  FidoCableDevice(BluetoothAdapter* adapter, std::string address);
  // Constructor used for testing purposes.
  FidoCableDevice(std::unique_ptr<FidoBleConnection> connection);

  FidoCableDevice(const FidoCableDevice&) = delete;
  FidoCableDevice& operator=(const FidoCableDevice&) = delete;

  ~FidoCableDevice() override;

  // Returns FidoDevice::GetId() for a given FidoBleConnection address.
  static std::string GetIdForAddress(const std::string& ble_address);

  std::string GetAddress();
  void Connect();
  void SendPing(std::vector<uint8_t> data, DeviceCallback callback);
  FidoBleConnection::ReadCallback GetReadCallbackForTesting();

  // FidoDevice:
  void Cancel(CancelToken token) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;

  void SendHandshakeMessage(std::vector<uint8_t> handshake_message,
                            DeviceCallback callback);

  // Configure caBLE v1 keys.
  void SetV1EncryptionData(base::span<const uint8_t, 32> session_key,
                           base::span<const uint8_t, 8> nonce);

  // SetCountersForTesting allows tests to set the message counters. Non-test
  // code must not call this function.
  void SetSequenceNumbersForTesting(uint32_t read_counter,
                                    uint32_t write_counter);

  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  struct PendingFrame {
    PendingFrame(FidoBleFrame frame, FrameCallback callback, CancelToken token);
    PendingFrame(PendingFrame&&);
    ~PendingFrame();

    FidoBleFrame frame;
    FrameCallback callback;
    CancelToken token;
  };

  // Encapsulates state FidoCableDevice maintains to encrypt and decrypt
  // data within FidoBleFrame.
  struct EncryptionData {
    EncryptionData();
    ~EncryptionData();

    std::array<uint8_t, 32> read_key;
    std::array<uint8_t, 32> write_key;
    std::array<uint8_t, 8> nonce;
    uint32_t write_sequence_num = 0;
    uint32_t read_sequence_num = 0;
  };

  void OnResponseFrame(FrameCallback callback,
                       std::optional<FidoBleFrame> frame);
  void Transition();
  CancelToken AddToPendingFrames(FidoBleDeviceCommand cmd,
                                 std::vector<uint8_t> request,
                                 DeviceCallback callback);
  void ResetTransaction();

  void OnConnected(bool success);
  void OnStatusMessage(std::vector<uint8_t> data);

  void OnReadControlPointLength(std::optional<uint16_t> length);

  void SendRequestFrame(FidoBleFrame frame, FrameCallback callback);

  void StartTimeout();
  void StopTimeout();
  void OnTimeout();

  void OnBleResponseReceived(DeviceCallback callback,
                             std::optional<FidoBleFrame> frame);
  void ProcessBleDeviceError(base::span<const uint8_t> data);

  bool EncryptOutgoingMessage(std::vector<uint8_t>* message_to_encrypt);
  bool DecryptIncomingMessage(FidoBleFrame* incoming_frame);

  base::OneShotTimer timer_;

  std::unique_ptr<FidoBleConnection> connection_;
  uint16_t control_point_length_ = 0;

  // pending_frames_ contains frames that have not yet been sent, i.e. the
  // current frame is not included at the head of the list.
  std::list<PendingFrame> pending_frames_;
  // current_token_ contains the cancelation token of the currently running
  // request, or else is empty if no request is currently pending.
  std::optional<CancelToken> current_token_;
  std::optional<FidoBleTransaction> transaction_;

  std::optional<EncryptionData> encryption_data_;
  base::WeakPtrFactory<FidoCableDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_
