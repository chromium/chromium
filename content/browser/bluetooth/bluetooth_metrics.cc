// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_metrics.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

using device::BluetoothUUID;

namespace {

// Generates a hash from a canonical UUID string suitable for
// base::UmaHistogramSparse(positive int).
//
// Hash values can be produced manually using tool: bluetooth_metrics_hash.
int HashUUID(const std::string& canonical_uuid) {
  DCHECK(canonical_uuid.size() == 36) << "HashUUID requires 128 bit UUID "
                                         "strings in canonical format to "
                                         "ensure consistent hash results.";

  // TODO(520284): Other than verifying that |uuid| contains a value, this logic
  // should be migrated to a dedicated histogram macro for hashed strings.
  uint32_t data = base::PersistentHash(canonical_uuid);

  // Strip off the sign bit to make the hash look nicer.
  return static_cast<int>(data & 0x7fffffff);
}

int HashUUID(const base::Optional<BluetoothUUID>& uuid) {
  return uuid ? HashUUID(uuid->canonical_value()) : 0;
}

// The maximum number of devices that needs to be recorded.
const size_t kMaxNumOfDevices = 100;

}  // namespace

namespace content {

// General

// requestDevice()

void RecordRequestDeviceOutcome(UMARequestDeviceOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.RequestDevice.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMARequestDeviceOutcome::COUNT));
}

static void RecordRequestDeviceFilters(
    const std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>& filters) {
  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.Filters.Count",
                           filters.size());
  for (const auto& filter : filters) {
    if (!filter->services) {
      continue;
    }
    UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.FilterSize",
                             filter->services->size());
    for (const BluetoothUUID& service : filter->services.value()) {
      // TODO(ortuno): Use a macro to histogram strings.
      // http://crbug.com/520284
      base::UmaHistogramSparse("Bluetooth.Web.RequestDevice.Filters.Services",
                               HashUUID(service));
    }
  }
}

static void RecordRequestDeviceOptionalServices(
    const std::vector<BluetoothUUID>& optional_services) {
  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.OptionalServices.Count",
                           optional_services.size());
  for (const BluetoothUUID& service : optional_services) {
    // TODO(ortuno): Use a macro to histogram strings.
    // http://crbug.com/520284
    base::UmaHistogramSparse(
        "Bluetooth.Web.RequestDevice.OptionalServices.Services",
        HashUUID(service));
  }
}

static void RecordUnionOfServices(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  std::unordered_set<std::string> union_of_services;
  for (const BluetoothUUID& service : options->optional_services) {
    union_of_services.insert(service.canonical_value());
  }

  if (options->filters) {
    for (const auto& filter : options->filters.value()) {
      if (!filter->services) {
        continue;
      }
      for (const BluetoothUUID& service : filter->services.value()) {
        union_of_services.insert(service.canonical_value());
      }
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.UnionOfServices.Count",
                           union_of_services.size());

  for (const std::string& service : union_of_services) {
    // TODO(ortuno): Use a macro to histogram strings.
    // http://crbug.com/520284
    base::UmaHistogramSparse(
        "Bluetooth.Web.RequestDevice.UnionOfServices.Services",
        HashUUID(service));
  }
}

void RecordRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  UMA_HISTOGRAM_BOOLEAN("Bluetooth.Web.RequestDevice.Options.AcceptAllDevices",
                        options->accept_all_devices);

  if (options->filters) {
    RecordRequestDeviceFilters(options->filters.value());
  }

  RecordRequestDeviceOptionalServices(options->optional_services);
  RecordUnionOfServices(options);
}

// GATTServer.Connect

void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.ConnectGATT.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAConnectGATTOutcome::COUNT));
}

void RecordConnectGATTOutcome(CacheQueryOutcome outcome) {
  DCHECK(outcome == CacheQueryOutcome::NO_DEVICE);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::NO_DEVICE);
}

void RecordConnectGATTTimeSuccess(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Bluetooth.Web.ConnectGATT.TimeSuccess", duration);
}

void RecordConnectGATTTimeFailed(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Bluetooth.Web.ConnectGATT.TimeFailed", duration);
}

// getPrimaryService & getPrimaryServices

void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetPrimaryServiceOutcome outcome) {
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetPrimaryService.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetPrimaryServiceOutcome::COUNT));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetPrimaryServices.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetPrimaryServiceOutcome::COUNT));
      return;
  }
}

void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome) {
  DCHECK(outcome == CacheQueryOutcome::NO_DEVICE);
  RecordGetPrimaryServicesOutcome(quantity,
                                  UMAGetPrimaryServiceOutcome::NO_DEVICE);
}

void RecordGetPrimaryServicesServices(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& service) {
  // TODO(ortuno): Use a macro to histogram strings.
  // http://crbug.com/520284
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      base::UmaHistogramSparse("Bluetooth.Web.GetPrimaryService.Services",
                               HashUUID(service));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      base::UmaHistogramSparse("Bluetooth.Web.GetPrimaryServices.Services",
                               HashUUID(service));
      return;
  }
}

// getCharacteristic & getCharacteristics

void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetCharacteristicOutcome outcome) {
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetCharacteristic.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetCharacteristicOutcome::COUNT));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetCharacteristics.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetCharacteristicOutcome::COUNT));
      return;
  }
}

void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome) {
  switch (outcome) {
    case CacheQueryOutcome::SUCCESS:
    case CacheQueryOutcome::BAD_RENDERER:
      // No need to record a success or renderer crash.
      NOTREACHED();
      return;
    case CacheQueryOutcome::NO_DEVICE:
      RecordGetCharacteristicsOutcome(quantity,
                                      UMAGetCharacteristicOutcome::NO_DEVICE);
      return;
    case CacheQueryOutcome::NO_SERVICE:
      RecordGetCharacteristicsOutcome(quantity,
                                      UMAGetCharacteristicOutcome::NO_SERVICE);
      return;
    case CacheQueryOutcome::NO_CHARACTERISTIC:
    case CacheQueryOutcome::NO_DESCRIPTOR:
      NOTREACHED();
      return;
  }
}

void RecordGetCharacteristicsCharacteristic(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& characteristic) {
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      base::UmaHistogramSparse("Bluetooth.Web.GetCharacteristic.Characteristic",
                               HashUUID(characteristic));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      base::UmaHistogramSparse(
          "Bluetooth.Web.GetCharacteristics.Characteristic",
          HashUUID(characteristic));
      return;
  }
}

void RecordGetDescriptorsDescriptor(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& descriptor) {
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      base::UmaHistogramSparse("Bluetooth.Web.GetDescriptor.Descriptor",
                               HashUUID(descriptor));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      base::UmaHistogramSparse("Bluetooth.Web.GetDescriptors.Descriptor",
                               HashUUID(descriptor));
      return;
  }
}

void RecordGetDescriptorsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetDescriptorOutcome outcome) {
  switch (quantity) {
    case blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetDescriptor.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetDescriptorOutcome::COUNT));
      return;
    case blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Bluetooth.Web.GetDescriptors.Outcome", static_cast<int>(outcome),
          static_cast<int>(UMAGetDescriptorOutcome::COUNT));
      return;
  }
}

void RecordGetDescriptorsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome) {
  switch (outcome) {
    case CacheQueryOutcome::SUCCESS:
    case CacheQueryOutcome::BAD_RENDERER:
      // No need to record a success or renderer crash.
      NOTREACHED();
      return;
    case CacheQueryOutcome::NO_DEVICE:
      RecordGetDescriptorsOutcome(quantity, UMAGetDescriptorOutcome::NO_DEVICE);
      return;
    case CacheQueryOutcome::NO_SERVICE:
      RecordGetDescriptorsOutcome(quantity,
                                  UMAGetDescriptorOutcome::NO_SERVICE);
      return;
    case CacheQueryOutcome::NO_CHARACTERISTIC:
      RecordGetDescriptorsOutcome(quantity,
                                  UMAGetDescriptorOutcome::NO_CHARACTERISTIC);
      return;
    case CacheQueryOutcome::NO_DESCRIPTOR:
      NOTREACHED();
      return;
  }
}

// GATT Operations

void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome) {
  switch (operation) {
    case UMAGATTOperation::CHARACTERISTIC_READ:
      RecordCharacteristicReadValueOutcome(outcome);
      return;
    case UMAGATTOperation::CHARACTERISTIC_WRITE:
      RecordCharacteristicWriteValueOutcome(outcome);
      return;
    case UMAGATTOperation::START_NOTIFICATIONS:
      RecordStartNotificationsOutcome(outcome);
      return;
    case UMAGATTOperation::DESCRIPTOR_READ:
      RecordDescriptorReadValueOutcome(outcome);
      return;
    case UMAGATTOperation::DESCRIPTOR_WRITE:
      RecordDescriptorWriteValueOutcome(outcome);
      return;
    case UMAGATTOperation::COUNT:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

static UMAGATTOperationOutcome TranslateCacheQueryOutcomeToGATTOperationOutcome(
    CacheQueryOutcome outcome) {
  switch (outcome) {
    case CacheQueryOutcome::SUCCESS:
    case CacheQueryOutcome::BAD_RENDERER:
      // No need to record a success or renderer crash.
      NOTREACHED();
      return UMAGATTOperationOutcome::NOT_SUPPORTED;
    case CacheQueryOutcome::NO_DEVICE:
      return UMAGATTOperationOutcome::NO_DEVICE;
    case CacheQueryOutcome::NO_SERVICE:
      return UMAGATTOperationOutcome::NO_SERVICE;
    case CacheQueryOutcome::NO_CHARACTERISTIC:
      return UMAGATTOperationOutcome::NO_CHARACTERISTIC;
    case CacheQueryOutcome::NO_DESCRIPTOR:
      return UMAGATTOperationOutcome::NO_DESCRIPTOR;
  }
  NOTREACHED() << "No need to record success or renderer crash";
  return UMAGATTOperationOutcome::NOT_SUPPORTED;
}

// Characteristic.readValue

void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Characteristic.ReadValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

void RecordCharacteristicReadValueOutcome(CacheQueryOutcome outcome) {
  RecordCharacteristicReadValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

// Characteristic.writeValue

void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Characteristic.WriteValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

void RecordCharacteristicWriteValueOutcome(CacheQueryOutcome outcome) {
  RecordCharacteristicWriteValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

// Characteristic.startNotifications
void RecordStartNotificationsOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.Web.Characteristic.StartNotifications.Outcome",
      static_cast<int>(outcome),
      static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

void RecordStartNotificationsOutcome(CacheQueryOutcome outcome) {
  RecordStartNotificationsOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

void RecordRSSISignalStrength(int rssi) {
  base::UmaHistogramSparse("Bluetooth.Web.RequestDevice.RSSISignalStrength",
                           rssi);
}

// Descriptor.readValue

void RecordDescriptorReadValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Descriptor.ReadValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

void RecordDescriptorReadValueOutcome(CacheQueryOutcome outcome) {
  RecordDescriptorReadValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

// Descriptor.writeValue

void RecordDescriptorWriteValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Descriptor.WriteValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

void RecordDescriptorWriteValueOutcome(CacheQueryOutcome outcome) {
  RecordDescriptorWriteValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

void RecordRSSISignalStrengthLevel(UMARSSISignalStrengthLevel level) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.Web.RequestDevice.RSSISignalStrengthLevel",
      static_cast<int>(level),
      static_cast<int>(UMARSSISignalStrengthLevel::COUNT));
}

void RecordNumOfDevices(bool accept_all_devices, size_t num_of_devices) {
  if (!accept_all_devices) {
    base::UmaHistogramSparse(
        "Bluetooth.Web.RequestDevice."
        "NumOfDevicesInChooserWhenNotAcceptingAllDevices",
        std::min(num_of_devices, kMaxNumOfDevices));
  }
}

}  // namespace content
