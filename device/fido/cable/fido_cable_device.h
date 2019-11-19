// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "crypto/aead.h"
#include "device/fido/ble/fido_ble_device.h"

namespace device {

class BluetoothAdapter;
class FidoBleConnection;
class FidoBleFrame;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDevice : public FidoBleDevice {
 public:
  using FrameCallback = FidoBleTransaction::FrameCallback;

  FidoCableDevice(BluetoothAdapter* adapter, std::string address);
  // Constructor used for testing purposes.
  FidoCableDevice(std::unique_ptr<FidoBleConnection> connection);
  ~FidoCableDevice() override;

  // FidoBleDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;
  void OnResponseFrame(FrameCallback callback,
                       base::Optional<FidoBleFrame> frame) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  void SendHandshakeMessage(std::vector<uint8_t> handshake_message,
                            DeviceCallback callback);

  // Configure caBLE v1 keys.
  void SetV1EncryptionData(base::span<const uint8_t, 32> session_key,
                           base::span<const uint8_t, 8> nonce);
  // Configure caBLE v2 keys.
  void SetV2EncryptionData(base::span<const uint8_t, 32> read_key,
                           base::span<const uint8_t, 32> write_key);
  FidoTransportProtocol DeviceTransport() const override;

  // SetCountersForTesting allows tests to set the message counters. Non-test
  // code must not call this function.
  void SetSequenceNumbersForTesting(uint32_t read_counter,
                                    uint32_t write_counter);

 private:
  // Encapsulates state FidoCableDevice maintains to encrypt and decrypt
  // data within FidoBleFrame.
  struct EncryptionData {
    EncryptionData();

    std::array<uint8_t, 32> read_key;
    std::array<uint8_t, 32> write_key;
    std::array<uint8_t, 8> nonce;
    uint32_t write_sequence_num = 0;
    uint32_t read_sequence_num = 0;
    bool is_version_two = false;
  };

  static bool EncryptOutgoingMessage(const EncryptionData& encryption_data,
                                     std::vector<uint8_t>* message_to_encrypt);
  static bool DecryptIncomingMessage(const EncryptionData& encryption_data,
                                     FidoBleFrame* incoming_frame);

  static bool EncryptV1OutgoingMessage(
      const EncryptionData& encryption_data,
      std::vector<uint8_t>* message_to_encrypt);
  static bool DecryptV1IncomingMessage(const EncryptionData& encryption_data,
                                       FidoBleFrame* incoming_frame);

  static bool EncryptV2OutgoingMessage(
      const EncryptionData& encryption_data,
      std::vector<uint8_t>* message_to_encrypt);
  static bool DecryptV2IncomingMessage(const EncryptionData& encryption_data,
                                       FidoBleFrame* incoming_frame);

  base::Optional<EncryptionData> encryption_data_;
  base::WeakPtrFactory<FidoCableDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoCableDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DEVICE_H_
