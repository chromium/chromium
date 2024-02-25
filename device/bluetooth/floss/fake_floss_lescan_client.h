// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LESCAN_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LESCAN_CLIENT_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_lescan_client.h"

namespace floss {

constexpr char kTestUuidStr[] = "00010203-0405-0607-0809-0a0b0c0d0e0f";
constexpr char kTestUuidStr2[] = "02010203-0405-0607-0809-0a0b0c0d0e0f";

class DEVICE_BLUETOOTH_EXPORT FakeFlossLEScanClient : public FlossLEScanClient {
 public:
  FakeFlossLEScanClient();
  ~FakeFlossLEScanClient() override;

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;
  void RegisterScanner(
      ResponseCallback<device::BluetoothUUID> callback) override;
  void UnregisterScanner(ResponseCallback<bool> callback,
                         uint8_t scanner_id) override;
  void StartScan(ResponseCallback<BtifStatus> callback,
                 uint8_t scanner_id,
                 const std::optional<ScanSettings>& scan_settings,
                 const std::optional<ScanFilter>& filters) override;

  void SetNextScannerUUID(const device::BluetoothUUID& uuid) {
    next_scanner_uuid_ = uuid;
  }

  // For test observation
  int scanners_registered_ = 0;
  std::unordered_set<uint8_t> scanner_ids_;

 private:
  // Next UUID for registered scanner.
  // TODO(b/271318036): Replace fake with mocks for easier control of fake
  // values.
  device::BluetoothUUID next_scanner_uuid_ =
      device::BluetoothUUID(kTestUuidStr);

  base::WeakPtrFactory<FakeFlossLEScanClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LESCAN_CLIENT_H_
