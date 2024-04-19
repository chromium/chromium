// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blocklist.h"

#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "content/browser/bluetooth/bluetooth_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using device::BluetoothUUID;
using ManufacturerId = device::BluetoothDevice::ManufacturerId;

namespace {

static base::LazyInstance<content::BluetoothBlocklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

BluetoothBlocklist::~BluetoothBlocklist() {}

// static
BluetoothBlocklist& BluetoothBlocklist::Get() {
  return g_singleton.Get();
}

void BluetoothBlocklist::Add(const BluetoothUUID& uuid, Value value) {
  CHECK(uuid.IsValid());
  auto insert_result = blocklisted_uuids_.insert(std::make_pair(uuid, value));
  bool inserted = insert_result.second;
  if (!inserted) {
    Value& stored = insert_result.first->second;
    if (stored != value)
      stored = Value::EXCLUDE;
  }
}

// TODO(crbug.com/40855069): Support |blocklist_string| for manufacturer data
// prefix.
void BluetoothBlocklist::Add(std::string_view blocklist_string) {
  if (blocklist_string.empty())
    return;
  base::StringPairs kv_pairs;
  base::SplitStringIntoKeyValuePairs(blocklist_string,
                                     ':',  // Key-value delimiter
                                     ',',  // Key-value pair delimiter
                                     &kv_pairs);
  for (const auto& pair : kv_pairs) {
    BluetoothUUID uuid(pair.first);
    if (uuid.IsValid() && pair.second.size() == 1u) {
      switch (pair.second[0]) {
        case 'e':
          Add(uuid, Value::EXCLUDE);
          continue;
        case 'r':
          Add(uuid, Value::EXCLUDE_READS);
          continue;
        case 'w':
          Add(uuid, Value::EXCLUDE_WRITES);
          continue;
      }
    }
  }
}

void BluetoothBlocklist::Add(
    const device::BluetoothDevice::ManufacturerId& company_identifier,
    const std::vector<blink::mojom::WebBluetoothDataFilter>& prefix) {
  DataPrefix data_prefix;
  for (const auto& byte : prefix) {
    data_prefix.push_back(
        blink::mojom::WebBluetoothDataFilter::New(byte.data, byte.mask));
  }
  blocklisted_manufacturer_data_prefix_[company_identifier].push_back(
      std::move(data_prefix));
}

bool BluetoothBlocklist::IsExcluded(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blocklisted_uuids_.find(uuid);
  if (it == blocklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE;
}

bool BluetoothBlocklist::IsExcluded(
    const blink::mojom::WebBluetoothCompanyPtr& company_identifier,
    const std::vector<blink::mojom::WebBluetoothDataFilterPtr>& filter_data)
    const {
  auto it = blocklisted_manufacturer_data_prefix_.find(company_identifier->id);
  if (it == blocklisted_manufacturer_data_prefix_.end()) {
    return false;
  }
  // Check if |filter_data| is a strict subset of any blocked record in
  // |blocklisted_manufacturer_data_prefix_| in order to provide developers
  // with feedback if they request data they will not be allowed to receive.
  //
  // We don't want to exclude |filter_data| if it only has a non-empty
  // intersection with the blocked data as the filter might also match other
  // unblocked data. This means that filters which match blocked data will be
  // allowed in a call to requestDevice(). The blocked data will still be
  // filtered out when constructing the BluetoothAdvertisementEvent.
  for (const auto& blocked_data_prefix : it->second) {
    // If |filter_data| length is shorter, it can't be subset of
    // |blocked_data_prefix|. For example:
    // - blocked_data_prefix = {{0x01, 0xff}, {0x00, 0x00}}
    // - filter_data = {{0x01, 0xff}}
    // data like {0x1} is matched by |filter_data| but not by
    // |blocked_data_prefix|.
    if (blocked_data_prefix.size() > filter_data.size()) {
      continue;
    }
    size_t i = 0;
    for (const auto& byte : blocked_data_prefix) {
      if ((byte->mask & filter_data.at(i)->mask) != byte->mask) {
        break;
      }
      if ((byte->data & byte->mask) != (filter_data.at(i)->data & byte->mask)) {
        break;
      }
      i += 1;
    }
    if (i == blocked_data_prefix.size()) {
      return true;
    }
  }
  return false;
}

bool BluetoothBlocklist::IsExcluded(
    const device::BluetoothDevice::ManufacturerId& company_identifier,
    const device::BluetoothDevice::ManufacturerData& manufacturer_data) const {
  auto it = blocklisted_manufacturer_data_prefix_.find(company_identifier);
  if (it == blocklisted_manufacturer_data_prefix_.end()) {
    return false;
  }

  for (const auto& blocked_data_prefix : it->second) {
    if (MatchesBluetoothDataFilter(blocked_data_prefix, manufacturer_data))
      return true;
  }
  return false;
}

bool BluetoothBlocklist::IsExcluded(
    const std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>& filters) {
  for (const blink::mojom::WebBluetoothLeScanFilterPtr& filter : filters) {
    if (filter->services) {
      for (const BluetoothUUID& service : filter->services.value()) {
        if (IsExcluded(service)) {
          return true;
        }
      }
    }
    if (filter->manufacturer_data) {
      for (const auto& [company_id, data_filter] :
           filter->manufacturer_data.value()) {
        if (IsExcluded(company_id, data_filter)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool BluetoothBlocklist::IsExcludedFromReads(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blocklisted_uuids_.find(uuid);
  if (it == blocklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_READS;
}

bool BluetoothBlocklist::IsExcludedFromWrites(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blocklisted_uuids_.find(uuid);
  if (it == blocklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_WRITES;
}

void BluetoothBlocklist::RemoveExcludedUUIDs(
    blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  std::vector<device::BluetoothUUID> optional_services_blocklist_filtered;
  for (const BluetoothUUID& uuid : options->optional_services) {
    if (!IsExcluded(uuid)) {
      optional_services_blocklist_filtered.push_back(uuid);
    }
  }
  options->optional_services = std::move(optional_services_blocklist_filtered);
}

void BluetoothBlocklist::ResetToDefaultValuesForTest() {
  blocklisted_uuids_.clear();
  blocklisted_manufacturer_data_prefix_.clear();
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

BluetoothBlocklist::BluetoothBlocklist() {
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

void BluetoothBlocklist::PopulateWithDefaultValues() {
  blocklisted_uuids_.clear();

  DCHECK(BluetoothUUID("00001800-0000-1000-8000-00805f9b34fb") ==
         BluetoothUUID("1800"));

  // Blocklist UUIDs updated 2021-01-06 from:
  // https://github.com/WebBluetoothCG/registries/blob/master/gatt_blocklist.txt
  // Short UUIDs are used for readability of this list.
  //
  // Services:
  Add(BluetoothUUID("1812"), Value::EXCLUDE);
  Add(BluetoothUUID("00001530-1212-efde-1523-785feabcd123"), Value::EXCLUDE);
  Add(BluetoothUUID("f000ffc0-0451-4000-b000-000000000000"), Value::EXCLUDE);
  Add(BluetoothUUID("00060000"), Value::EXCLUDE);
  Add(BluetoothUUID("fff9"), Value::EXCLUDE);
  Add(BluetoothUUID("fffd"), Value::EXCLUDE);
  Add(BluetoothUUID("fde2"), Value::EXCLUDE);
  // Characteristics:
  Add(BluetoothUUID("2a02"), Value::EXCLUDE_WRITES);
  Add(BluetoothUUID("2a03"), Value::EXCLUDE);
  Add(BluetoothUUID("2a25"), Value::EXCLUDE);
  // Descriptors:
  Add(BluetoothUUID("2902"), Value::EXCLUDE_WRITES);
  Add(BluetoothUUID("2903"), Value::EXCLUDE_WRITES);

  // Testing from Web Tests Note:
  //
  // Random UUIDs for object & exclude permutations that do not exist in the
  // standard blocklist are included to facilitate integration testing from
  // Web Tests.  Unit tests can dynamically modify the blocklist, but don't
  // offer the full integration test to the Web Bluetooth Javascript bindings.
  //
  // This is done for simplicity as opposed to exposing a testing API that can
  // add to the blocklist over time, which would be over engineered.
  //
  // Remove testing UUIDs if the specified blocklist is updated to include UUIDs
  // that match the specific permutations.
  //
  // Characteristics for Web Tests:
  Add(BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135"),
      Value::EXCLUDE_READS);
  // Descriptors for Web Tests:
  Add(BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c"), Value::EXCLUDE);
  Add(BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114"),
      Value::EXCLUDE_READS);

  // TODO(crbug.com/40740004): To fill below when manufacturer blocklist spec
  // patch is done.
  // Blocklist manufacturer data prefix updated [TBD date] from: [TBD
  // blocklist link].
  // iBeacon's proximity UUID might reveal user's location information. See
  // https://en.wikipedia.org/wiki/IBeacon for detail.
  Add(0x4c, {{0x02, 0xff}});
}

void BluetoothBlocklist::PopulateWithServerProvidedValues() {
  Add(GetContentClient()->browser()->GetWebBluetoothBlocklist());
}

}  // namespace content
