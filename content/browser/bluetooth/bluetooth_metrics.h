// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_

#include <optional>

#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace device {
class BluetoothUUID;
}

namespace content {

// General Metrics

// Enumeration for outcomes of querying the bluetooth cache.
enum class CacheQueryOutcome {
  kSuccess = 0,
  kBadRenderer = 1,
  kNoDevice = 2,
  kNoService = 3,
  kNoCharacteristic = 4,
  kNoDescriptor = 5,
};

// requestDevice() Metrics

// Records stats about the arguments used when calling requestDevice.
//  - The union of filtered and optional service UUIDs.
void RecordRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

// GattServer.connect() Metrics

enum class UMAConnectGATTOutcome {
  kSuccess = 0,
  kNoDevice = 1,
  kUnknown = 2,
  kInProgress = 3,
  kFailed = 4,
  kAuthFailed = 5,
  kAuthCanceled = 6,
  kAuthRejected = 7,
  kAuthTimeout = 8,
  kUnsupportedDevice = 9,
  kNotReady = 10,
  kAlreadyConnected = 11,
  kAlreadyExists = 12,
  kNotConnected = 13,
  kDoesNotExist = 14,
  kInvalidArgs = 15,
  kNonAuthTimeout = 16,
  kNoMemory = 17,
  kJniEnvironment = 18,
  kJniThreadAttach = 19,
  kWakelock = 20,
  kUnexpectedState = 21,
  kSocketError = 22,
  // Note: Add new ConnectGATT outcomes immediately above this line. Make sure
  // to update the enum list in
  // tools/metrics/histograms/metadata/bluetooth/enums.xml accordingly.
  kMaxValue = kSocketError,
};

// There should be a call to this function before every
// Send(BluetoothMsg_ConnectGATTSuccess) and
// Send(BluetoothMsg_ConnectGATTError).
void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome);

// Records the outcome of the cache query for connectGATT. Should only be called
// if QueryCacheForDevice fails.
void RecordConnectGATTOutcome(CacheQueryOutcome outcome);

// getPrimaryService() and getPrimaryServices() Metrics

// Records the UUID of the service used when calling getPrimaryService.
void RecordGetPrimaryServicesServices(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<device::BluetoothUUID>& service);

// getCharacteristic() and getCharacteristics() Metrics

enum class UMAGetCharacteristicOutcome {
  kSuccess = 0,
  kNoDevice = 1,
  kNoService = 2,
  kNotFound = 3,
  kBlocklisted = 4,
  kNoCharacteristics = 5,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histogram/histograms.xml accordingly.
  kMaxValue = kNoCharacteristics,
};

enum class UMAGetDescriptorOutcome {
  kSuccess = 0,
  kNoDevice = 1,
  kNoService = 2,
  kNoCharacteristic = 3,
  kNotFound = 4,
  kBlocklisted = 5,
  kNoDescriptors = 6,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histogram/histograms.xml accordingly.
  kMaxValue = kNoDescriptors,
};

// There should be a call to this function whenever
// RemoteServiceGetCharacteristicsCallback is run.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE for
// getCharacteristic.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE for
// getCharacteristics.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetCharacteristicOutcome outcome);

// Records the outcome of the cache query for getCharacteristics. Should only be
// called if QueryCacheForService fails.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// Records the UUID of the characteristic used when calling getCharacteristic.
void RecordGetCharacteristicsCharacteristic(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<device::BluetoothUUID>& characteristic);

// Records the outcome of the cache query for getDescriptors. Should only be
// called if QueryCacheForService fails.
void RecordGetDescriptorsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// GATT Operations Metrics

// These are the possible outcomes when performing GATT operations i.e.
// characteristic.readValue/writeValue descriptor.readValue/writeValue.
enum class UMAGATTOperationOutcome {
  kSuccess = 0,
  kNoDevice = 1,
  kNoService = 2,
  kNoCharacteristic = 3,
  kNoDescriptor = 4,
  kUnknown = 5,
  kFailed = 6,
  kInProgress = 7,
  kInvalidLength = 8,
  kNotPermitted = 9,
  kNotAuthorized = 10,
  kNotPaired = 11,
  kNotSupported = 12,
  kBlocklisted = 13,
  // Note: Add new GATT Outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  kMaxValue = kBlocklisted
};

// Values below do NOT map to UMA metric values.
enum class UMAGATTOperation {
  kCharacteristicRead,
  kCharacteristicWrite,
  kStartNotifications,
  kDescriptorReadObsolete,
  kDescriptorWriteObsolete,
};

// Records the outcome of a GATT operation.
// There should be a call to this function whenever the corresponding operation
// doesn't have a call to Record[Operation]Outcome.
void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome);

// Characteristic.readValue() Metrics:
// There should be a call to this function for every Mojo
// bluetooth.mojom.Device.ReadValueForCharacteristic response.
void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome error);

// Records the outcome of a cache query for readValue. Should only be called if
// QueryCacheForCharacteristic fails.
void RecordCharacteristicReadValueOutcome(CacheQueryOutcome outcome);

// Characteristic.writeValue() Metrics
// There should be a call to this function for every Mojo
// bluetooth.mojom.Device.WriteValueForCharacteristic response.
void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome error);

// Records the outcome of a cache query for writeValue. Should only be called if
// QueryCacheForCharacteristic fails.
void RecordCharacteristicWriteValueOutcome(CacheQueryOutcome outcome);

// Characteristic.startNotifications() Metrics
// There should be a call to this function for every call to the
// blink.mojom.WebBluetoothService.RemoteCharacteristicStartNotifications Mojo
// call.
void RecordStartNotificationsOutcome(UMAGATTOperationOutcome outcome);

// Records the outcome of a cache query for startNotifications. Should only be
// called if QueryCacheForCharacteristic fails.
void RecordStartNotificationsOutcome(CacheQueryOutcome outcome);

enum class UMARSSISignalStrengthLevel {
  kLessThanOrEqualToMinRssi = 0,
  kLevel0 = 1,
  kLevel1 = 2,
  kLevel2 = 3,
  kLevel3 = 4,
  kLevel4 = 5,
  kGreaterThanOrEqualToMaxRssi = 6,
  // Note: Add new RSSI signal strength level immediately above this line.
  kMaxValue = kGreaterThanOrEqualToMaxRssi,
};

// Records the raw RSSI, and processed result displayed to users, when
// content::BluetoothDeviceChooserController::CalculateSignalStrengthLevel() is
// called.
void RecordRSSISignalStrength(int rssi);
void RecordRSSISignalStrengthLevel(UMARSSISignalStrengthLevel level);

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
