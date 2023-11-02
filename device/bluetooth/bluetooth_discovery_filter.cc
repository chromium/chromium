// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_filter.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_common.h"

namespace device {

BluetoothDiscoveryFilter::BluetoothDiscoveryFilter() {
  SetTransport(BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL);
}

BluetoothDiscoveryFilter::BluetoothDiscoveryFilter(
    BluetoothTransport transport) {
  SetTransport(transport);
}

BluetoothDiscoveryFilter::~BluetoothDiscoveryFilter() = default;

BluetoothDiscoveryFilter::DeviceInfoFilter::DeviceInfoFilter() = default;
BluetoothDiscoveryFilter::DeviceInfoFilter::DeviceInfoFilter(
    const DeviceInfoFilter& other) = default;
BluetoothDiscoveryFilter::DeviceInfoFilter::~DeviceInfoFilter() = default;
bool BluetoothDiscoveryFilter::DeviceInfoFilter::operator==(
    const BluetoothDiscoveryFilter::DeviceInfoFilter& other) const {
  return uuids == other.uuids && name == other.name;
}

bool BluetoothDiscoveryFilter::DeviceInfoFilter::operator<(
    const BluetoothDiscoveryFilter::DeviceInfoFilter& other) const {
  if (name == other.name)
    return uuids < other.uuids;

  return name < other.name;
}

bool BluetoothDiscoveryFilter::GetRSSI(int16_t* out_rssi) const {
  DCHECK(out_rssi);
  if (!rssi_)
    return false;

  *out_rssi = *rssi_;
  return true;
}

void BluetoothDiscoveryFilter::SetRSSI(int16_t rssi) {
  rssi_ = rssi;
}

bool BluetoothDiscoveryFilter::GetPathloss(uint16_t* out_pathloss) const {
  DCHECK(out_pathloss);
  if (!pathloss_)
    return false;

  *out_pathloss = *pathloss_;
  return true;
}

void BluetoothDiscoveryFilter::SetPathloss(uint16_t pathloss) {
  pathloss_ = pathloss;
}

BluetoothTransport BluetoothDiscoveryFilter::GetTransport() const {
  return transport_;
}

void BluetoothDiscoveryFilter::SetTransport(BluetoothTransport transport) {
  DCHECK(transport != BLUETOOTH_TRANSPORT_INVALID);
  transport_ = transport;
}

void BluetoothDiscoveryFilter::GetUUIDs(
    std::set<device::BluetoothUUID>& out_uuids) const {
  out_uuids.clear();
  for (const auto& device_filter : device_filters_) {
    for (const auto& uuid : device_filter.uuids) {
      out_uuids.insert(uuid);
    }
  }
}

void BluetoothDiscoveryFilter::AddDeviceFilter(
    const BluetoothDiscoveryFilter::DeviceInfoFilter& device_filter) {
  device_filters_.insert(device_filter);
}

const base::flat_set<BluetoothDiscoveryFilter::DeviceInfoFilter>*
BluetoothDiscoveryFilter::GetDeviceFilters() const {
  return &device_filters_;
}

void BluetoothDiscoveryFilter::ClearDeviceFilters() {
  device_filters_.clear();
}

void BluetoothDiscoveryFilter::CopyFrom(
    const BluetoothDiscoveryFilter& filter) {
  transport_ = filter.transport_;

  device_filters_.clear();
  for (const auto& device_filter : filter.device_filters_)
    AddDeviceFilter(device_filter);

  rssi_ = filter.rssi_;
  pathloss_ = filter.pathloss_;
}

std::unique_ptr<device::BluetoothDiscoveryFilter>
BluetoothDiscoveryFilter::Merge(
    const device::BluetoothDiscoveryFilter* filter_a,
    const device::BluetoothDiscoveryFilter* filter_b) {
  std::unique_ptr<BluetoothDiscoveryFilter> result;

  if (!filter_a && !filter_b) {
    return result;
  }

  result = std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_DUAL);

  if (!filter_a || !filter_b || filter_a->IsDefault() ||
      filter_b->IsDefault()) {
    return result;
  }

  // both filters are not empty, so they must have transport set.
  result->SetTransport(static_cast<BluetoothTransport>(filter_a->transport_ |
                                                       filter_b->transport_));

  // if both filters have uuids, them merge them. Otherwise uuids filter should
  // be left empty
  if (!filter_a->device_filters_.empty() &&
      !filter_b->device_filters_.empty()) {
    for (const auto& device_filter : filter_a->device_filters_)
      result->AddDeviceFilter(device_filter);

    for (const auto& device_filter : filter_b->device_filters_)
      result->AddDeviceFilter(device_filter);
  }

  if ((filter_a->rssi_ && filter_b->pathloss_) ||
      (filter_a->pathloss_ && filter_b->rssi_)) {
    // if both rssi and pathloss filtering is enabled in two different
    // filters, we can't tell which filter is more generic, and we don't set
    // proximity filtering on merged filter.
    return result;
  }

  if (filter_a->rssi_ && filter_b->rssi_) {
    result->SetRSSI(std::min(*filter_a->rssi_, *filter_b->rssi_));
  } else if (filter_a->pathloss_ && filter_b->pathloss_) {
    result->SetPathloss(std::max(*filter_a->pathloss_, *filter_b->pathloss_));
  }

  return result;
}

bool BluetoothDiscoveryFilter::Equals(
    const BluetoothDiscoveryFilter& other) const {
  if ((rssi_.has_value() != other.rssi_.has_value()) ||
      (rssi_ && other.rssi_ && *rssi_ != *other.rssi_))
    return false;

  if ((pathloss_.has_value() != other.pathloss_.has_value()) ||
      (pathloss_ && other.pathloss_ && *pathloss_ != *other.pathloss_)) {
    return false;
  }

  if (transport_ != other.transport_)
    return false;

  if (device_filters_ != other.device_filters_)
    return false;

  return true;
}

bool BluetoothDiscoveryFilter::IsDefault() const {
  return !(rssi_ || pathloss_ || !device_filters_.empty() ||
           transport_ != BLUETOOTH_TRANSPORT_DUAL);
}

}  // namespace device
