// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_connection_logger.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace device {
namespace {

// This will point to the singleton instance upon initialization.
BluetoothConnectionLogger* g_instance = nullptr;
const char kBaseHistogramName[] = "Bluetooth.ChromeOS.DeviceConnected";

}  // namespace

// static
void BluetoothConnectionLogger::RecordDeviceConnected(
    const std::string& device_identifier,
    BluetoothDeviceType device_type) {
  if (!g_instance) {
    g_instance = new BluetoothConnectionLogger();
  }
  g_instance->RecordDeviceConnectedMetric(device_identifier, device_type);
}

// static
void BluetoothConnectionLogger::Shutdown() {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }
}

void BluetoothConnectionLogger::RecordDeviceConnectedMetric(
    const std::string& device_identifier,
    BluetoothDeviceType device_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({kBaseHistogramName, ".AllConnections"}), device_type);

  if (base::Contains(device_ids_logged_this_session_, device_identifier)) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({kBaseHistogramName, ".UniqueConnectionsInSession"}),
      device_type);
  device_ids_logged_this_session_.insert(device_identifier);
}

BluetoothConnectionLogger::BluetoothConnectionLogger() = default;

BluetoothConnectionLogger::~BluetoothConnectionLogger() = default;

}  // namespace device