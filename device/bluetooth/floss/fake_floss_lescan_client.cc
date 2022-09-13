// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_lescan_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossLEScanClient::FakeFlossLEScanClient() = default;

FakeFlossLEScanClient::~FakeFlossLEScanClient() = default;

void FakeFlossLEScanClient::Init(dbus::Bus* bus,
                                 const std::string& service_name,
                                 const int adapter_index) {}

void FakeFlossLEScanClient::RegisterScanner(
    ResponseCallback<device::BluetoothUUID> callback) {
  scanners_registered_++;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), device::BluetoothUUID(kTestUuidStr)));
}

void FakeFlossLEScanClient::UnregisterScanner(ResponseCallback<bool> callback,
                                              uint8_t scanner_id) {
  if (scanners_registered_) {
    scanners_registered_--;
  }
  scanner_ids_.clear();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeFlossLEScanClient::StartScan(ResponseCallback<Void> callback,
                                      uint8_t scanner_id,
                                      const ScanSettings& scan_settings,
                                      const std::vector<ScanFilter>& filters) {
  // TODO (b/217274013): filters are currently being ignored
  scanner_ids_.insert(scanner_id);
}

}  // namespace floss
