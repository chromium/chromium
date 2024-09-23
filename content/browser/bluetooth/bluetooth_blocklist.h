// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLOCKLIST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLOCKLIST_H_

#include <map>
#include <string_view>
#include <vector>

#include "base/lazy_instance.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

// Implements the Web Bluetooth Blocklist policy as defined in the Web Bluetooth
// specification:
// https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blocklist
//
// Client code may query UUIDs to determine if they are excluded from use by the
// blocklist.
//
// Singleton access via Get() enforces only one copy of blocklist.
class CONTENT_EXPORT BluetoothBlocklist final {
 public:
  // Blocklist value terminology from Web Bluetooth specification:
  // https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blocklist
  enum class Value {
    EXCLUDE,        // Implies EXCLUDE_READS and EXCLUDE_WRITES.
    EXCLUDE_READS,  // Excluded from read operations.
    EXCLUDE_WRITES  // Excluded from write operations.
  };

  using DataPrefix = std::vector<blink::mojom::WebBluetoothDataFilterPtr>;
  using BlocklistedManufacturerDataMap =
      std::map<device::BluetoothDevice::ManufacturerId,
               std::vector<DataPrefix>>;

  BluetoothBlocklist(const BluetoothBlocklist&) = delete;
  BluetoothBlocklist& operator=(const BluetoothBlocklist&) = delete;

  ~BluetoothBlocklist();

  // Returns a singleton instance of the blocklist.
  static BluetoothBlocklist& Get();

  // Adds a UUID to the blocklist to be excluded from operations, merging with
  // any previous value and resulting in the strictest exclusion rule from the
  // combination of the two, E.G.:
  //   Add(uuid, EXCLUDE_READS);
  //   Add(uuid, EXCLUDE_WRITES);
  //   IsExcluded(uuid);  // true.
  // Requires UUID to be valid.
  void Add(const device::BluetoothUUID&, Value);

  // Adds UUIDs to the blocklist by parsing a blocklist string and calling
  // Add(uuid, value).
  //
  // The blocklist string format is defined at
  // ContentBrowserClient::GetWebBluetoothBlocklist().
  //
  // Malformed pairs in the string are ignored, including invalid UUID or
  // exclusion values. Duplicate UUIDs follow Add()'s merging rule.
  void Add(std::string_view blocklist_string);

  // Adds a manufacturer data prefix to |blocklisted_manufacturer_data_prefix_|
  // so that any manufacturer data in the device's advertisement matched
  // |prefix| will be excluded from device's advertisements.
  void Add(const device::BluetoothDevice::ManufacturerId& company_identifier,
           const std::vector<blink::mojom::WebBluetoothDataFilter>& prefix);

  // Returns if a UUID is excluded from all operations. UUID must be valid.
  bool IsExcluded(const device::BluetoothUUID&) const;

  // Returns if the filter of |company_identifier| and |data_filter| pair
  // is a strict subset of any blocked records in
  // |blocklisted_manufacturer_data_prefix_| hence should be excluded.
  bool IsExcluded(
      const blink::mojom::WebBluetoothCompanyPtr& company_identifier,
      const std::vector<blink::mojom::WebBluetoothDataFilterPtr>& data_filter)
      const;

  // Return if the |company_identifier| and |manufacturer_data| should be
  // excluded according to |blocklisted_manufacturer_data_prefix_|
  bool IsExcluded(
      const device::BluetoothDevice::ManufacturerId& company_identifier,
      const device::BluetoothDevice::ManufacturerData& manufacturer_data) const;

  // Returns if any UUID in a set of filters is excluded from all operations.
  // UUID must be valid.
  bool IsExcluded(
      const std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>& filters);

  // Returns if a UUID is excluded from read operations. UUID must be valid.
  bool IsExcludedFromReads(const device::BluetoothUUID&) const;

  // Returns if a UUID is excluded from write operations. UUID must be valid.
  bool IsExcludedFromWrites(const device::BluetoothUUID&) const;

  // Modifies |options->optional_services|, removing any UUIDs with
  // Value::EXCLUDE.
  void RemoveExcludedUUIDs(
      blink::mojom::WebBluetoothRequestDeviceOptions* options);

  // Size of blocklist.
  size_t size() { return blocklisted_uuids_.size(); }

  void ResetToDefaultValuesForTest();

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::LazyInstanceTraitsBase<BluetoothBlocklist>;

  BluetoothBlocklist();

  void PopulateWithDefaultValues();

  // Populates blocklist with values obtained dynamically from a server, able
  // to be updated without shipping new executable versions.
  void PopulateWithServerProvidedValues();

  // Map of UUID to blocklisted value.
  std::map<device::BluetoothUUID, Value> blocklisted_uuids_;

  BlocklistedManufacturerDataMap blocklisted_manufacturer_data_prefix_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLOCKLIST_H_
