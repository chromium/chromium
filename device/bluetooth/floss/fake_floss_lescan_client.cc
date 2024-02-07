// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_lescan_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossLEScanClient::FakeFlossLEScanClient() = default;

FakeFlossLEScanClient::~FakeFlossLEScanClient() = default;

void FakeFlossLEScanClient::Init(dbus::Bus* bus,
                                 const std::string& service_name,
                                 const int adapter_index,
                                 base::Version version,
                                 base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossLEScanClient::RegisterScanner(
    ResponseCallback<device::BluetoothUUID> callback) {
  scanners_registered_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), next_scanner_uuid_));
}

void FakeFlossLEScanClient::UnregisterScanner(ResponseCallback<bool> callback,
                                              uint8_t scanner_id) {
  if (scanners_registered_) {
    scanners_registered_--;
  }
  scanner_ids_.clear();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeFlossLEScanClient::StartScan(
    ResponseCallback<BtifStatus> callback,
    uint8_t scanner_id,
    const std::optional<ScanSettings>& scan_settings,
    const std::optional<ScanFilter>& filters) {
  scanner_ids_.insert(scanner_id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BtifStatus::kSuccess));
}

}  // namespace floss
