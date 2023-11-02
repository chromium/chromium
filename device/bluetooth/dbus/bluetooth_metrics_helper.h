// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_METRICS_HELPER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_METRICS_HELPER_H_

#include "base/time/time.h"

namespace dbus {
class ErrorResponse;
}

namespace bluez {

// These metrics are defined under
// tools/metrics/histograms/metadata/bluetooth/histogramx.xml.
constexpr char kLatencyMetric[] = "Bluetooth.BlueZ.DBus.%s.Latency";
constexpr char kResultMetric[] = "Bluetooth.BlueZ.DBus.%s.Result";

// Any new methods must be defined in the 'BlueZDBusMethodName' variant set in
// tools/metrics/histograms/metadata/bluetooth/histograms.xml.
constexpr char kConnectDeviceMethod[] = "ConnectDevice";
constexpr char kRegisterProfileMethod[] = "RegisterProfile";
constexpr char kUnregisterProfileMethod[] = "UnregisterProfile";
constexpr char kDisconnectProfileMethod[] = "DisconnectProfile";
constexpr char kGetServiceRecordsMethod[] = "GetServiceRecords";

void RecordSuccess(const std::string& method_name, base::Time start_time);
void RecordFailure(const std::string& method_name,
                   dbus::ErrorResponse* response);

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_METRICS_HELPER_H_
