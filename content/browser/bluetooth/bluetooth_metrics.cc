// Copyright 2015 The Chromium Authors
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

  // TODO(crbug.com/41194594): Other than verifying that |uuid| contains a
  // value, this logic should be migrated to a dedicated histogram macro for
  // hashed strings.
  uint32_t data = base::PersistentHash(canonical_uuid);

  // Strip off the sign bit to make the hash look nicer.
  return static_cast<int>(data & 0x7fffffff);
}

int HashUUID(const std::optional<BluetoothUUID>& uuid) {
  return uuid ? HashUUID(uuid->canonical_value()) : 0;
}

}  // namespace

namespace content {

// General

// requestDevice()

void RecordRequestDeviceOptions(
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

  for (const std::string& service : union_of_services) {
    // TODO(ortuno): Use a macro to histogram strings.
    // http://crbug.com/520284
    base::UmaHistogramSparse(
        "Bluetooth.Web.RequestDevice.UnionOfServices.Services",
        HashUUID(service));
  }
}

// GATTServer.Connect

void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.ConnectGATT.Outcome", outcome);
}

void RecordConnectGATTOutcome(CacheQueryOutcome outcome) {
  DCHECK_EQ(outcome, CacheQueryOutcome::kNoDevice);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::kNoDevice);
}

// getPrimaryService & getPrimaryServices

void RecordGetPrimaryServicesServices(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<BluetoothUUID>& service) {
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

void RecordGetCharacteristicsCharacteristic(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<BluetoothUUID>& characteristic) {
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

// GATT Operations

void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome) {
  switch (operation) {
    case UMAGATTOperation::kCharacteristicRead:
      RecordCharacteristicReadValueOutcome(outcome);
      return;
    case UMAGATTOperation::kCharacteristicWrite:
      RecordCharacteristicWriteValueOutcome(outcome);
      return;
    case UMAGATTOperation::kStartNotifications:
      RecordStartNotificationsOutcome(outcome);
      return;
    case UMAGATTOperation::kDescriptorReadObsolete:
    case UMAGATTOperation::kDescriptorWriteObsolete:
      return;
  }
}

static UMAGATTOperationOutcome TranslateCacheQueryOutcomeToGATTOperationOutcome(
    CacheQueryOutcome outcome) {
  switch (outcome) {
    case CacheQueryOutcome::kSuccess:
    case CacheQueryOutcome::kBadRenderer:
      // No need to record a success or renderer crash.
      NOTREACHED_IN_MIGRATION();
      return UMAGATTOperationOutcome::kNotSupported;
    case CacheQueryOutcome::kNoDevice:
      return UMAGATTOperationOutcome::kNoDevice;
    case CacheQueryOutcome::kNoService:
      return UMAGATTOperationOutcome::kNoService;
    case CacheQueryOutcome::kNoCharacteristic:
      return UMAGATTOperationOutcome::kNoCharacteristic;
    case CacheQueryOutcome::kNoDescriptor:
      return UMAGATTOperationOutcome::kNoDescriptor;
  }
}

// Characteristic.readValue

void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Web.Characteristic.ReadValue.Outcome", outcome);
}

void RecordCharacteristicReadValueOutcome(CacheQueryOutcome outcome) {
  RecordCharacteristicReadValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

// Characteristic.writeValue

void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Web.Characteristic.WriteValue.Outcome", outcome);
}

void RecordCharacteristicWriteValueOutcome(CacheQueryOutcome outcome) {
  RecordCharacteristicWriteValueOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

// Characteristic.startNotifications
void RecordStartNotificationsOutcome(UMAGATTOperationOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Web.Characteristic.StartNotifications.Outcome", outcome);
}

void RecordStartNotificationsOutcome(CacheQueryOutcome outcome) {
  RecordStartNotificationsOutcome(
      TranslateCacheQueryOutcomeToGATTOperationOutcome(outcome));
}

void RecordRSSISignalStrength(int rssi) {
  base::UmaHistogramSparse("Bluetooth.Web.RequestDevice.RSSISignalStrength",
                           rssi);
}

void RecordRSSISignalStrengthLevel(UMARSSISignalStrengthLevel level) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.Web.RequestDevice.RSSISignalStrengthLevel", level);
}

}  // namespace content
