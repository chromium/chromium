// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blocklist.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using device::BluetoothUUID;

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

void BluetoothBlocklist::Add(base::StringPiece blocklist_string) {
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

bool BluetoothBlocklist::IsExcluded(const BluetoothUUID& uuid) const {
  CHECK(uuid.IsValid());
  const auto& it = blocklisted_uuids_.find(uuid);
  if (it == blocklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE;
}

bool BluetoothBlocklist::IsExcluded(
    const std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>& filters) {
  for (const blink::mojom::WebBluetoothLeScanFilterPtr& filter : filters) {
    if (!filter->services) {
      continue;
    }
    for (const BluetoothUUID& service : filter->services.value()) {
      if (IsExcluded(service)) {
        return true;
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
}

void BluetoothBlocklist::PopulateWithServerProvidedValues() {
  Add(GetContentClient()->browser()->GetWebBluetoothBlocklist());
}

}  // namespace content
