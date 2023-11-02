// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_OUTCOME_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_OUTCOME_H_

namespace device {

// This enum is returned by various internal discovery session methods in
// BluetoothAdapter in order to histogram the causes of discovery failures.
enum class UMABluetoothDiscoverySessionOutcome {
  SUCCESS = 0,
  UNKNOWN = 1,  // Used when the platform has more information that isn't
                // reflected in any of these enum values or hasn't been wired
                // through.
  NOT_IMPLEMENTED = 2,
  ADAPTER_NOT_PRESENT = 3,
  ADAPTER_REMOVED = 4,  // Returned if the adapter disappeared during a callback
                        // chain.
  NOT_ACTIVE = 5,
  REMOVE_WITH_PENDING_REQUEST = 6,
  ACTIVE_SESSION_NOT_IN_ADAPTER = 7,
  FAILED = 8,

  // BlueZ-specific failures:
  BLUEZ_DBUS_UNKNOWN_ADAPTER = 9,
  BLUEZ_DBUS_NO_RESPONSE = 10,
  BLUEZ_DBUS_IN_PROGRESS = 11,
  BLUEZ_DBUS_NOT_READY = 12,
  BLUEZ_DBUS_FAILED_MAYBE_UNSUPPORTED_TRANSPORT = 13,
  BLUEZ_DBUS_UNSUPPORTED_DEVICE = 14,

  STOP_IN_PROGRESS = 15,
  // NOTE: Add new outcomes immediately above this line. Make sure to update the
  // enum list in tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_OUTCOME_H_
