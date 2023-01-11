// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/hid/fido_hid_device.h"

namespace device {

namespace {

// Checks that the supported report sizes of |device| are sufficient for at
// least one byte of non-header data per report and not larger than our maximum
// size.
bool ReportSizesSufficient(const device::mojom::HidDeviceInfo& device) {
  return device.max_input_report_size > kHidInitPacketHeaderSize &&
         device.max_input_report_size <= kHidMaxPacketSize &&
         device.max_output_report_size > kHidInitPacketHeaderSize &&
         device.max_output_report_size <= kHidMaxPacketSize;
}

FidoHidDiscovery::HidManagerBinder& GetHidManagerBinder() {
  static base::NoDestructor<FidoHidDiscovery::HidManagerBinder> binder;
  return *binder;
}

}  // namespace

bool operator==(const VidPid& lhs, const VidPid& rhs) {
  return lhs.vid == rhs.vid && lhs.pid == rhs.pid;
}

bool operator<(const VidPid& lhs, const VidPid& rhs) {
  return lhs.vid < rhs.vid || (lhs.vid == rhs.vid && lhs.pid < rhs.pid);
}

FidoHidDiscovery::FidoHidDiscovery(base::flat_set<VidPid> ignore_list)
    : FidoDeviceDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      ignore_list_(std::move(ignore_list)) {
  constexpr uint16_t kFidoUsagePage = 0xf1d0;
  filter_.SetUsagePage(kFidoUsagePage);
}

FidoHidDiscovery::~FidoHidDiscovery() = default;

// static
void FidoHidDiscovery::SetHidManagerBinder(HidManagerBinder binder) {
  GetHidManagerBinder() = std::move(binder);
}

void FidoHidDiscovery::StartInternal() {
  const auto& binder = GetHidManagerBinder();
  if (!binder)
    return;

  binder.Run(hid_manager_.BindNewPipeAndPassReceiver());
  hid_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&FidoHidDiscovery::OnGetDevices,
                     weak_factory_.GetWeakPtr()));
}

void FidoHidDiscovery::DeviceAdded(
    device::mojom::HidDeviceInfoPtr device_info) {
  // The init packet header is the larger of the headers so we only compare
  // against it below.
  static_assert(
      kHidInitPacketHeaderSize >= kHidContinuationPacketHeaderSize,
      "init header is expected to be larger than continuation header");

  if (!filter_.Matches(*device_info) || !ReportSizesSufficient(*device_info)) {
    return;
  }

  const VidPid vid_pid{device_info->vendor_id, device_info->product_id};
  if (base::Contains(ignore_list_, vid_pid)) {
    FIDO_LOG(EVENT) << "Ignoring HID device " << vid_pid.vid << ":"
                    << vid_pid.pid;
    return;
  }

  AddDevice(std::make_unique<FidoHidDevice>(std::move(device_info),
                                            hid_manager_.get()));
}

void FidoHidDiscovery::DeviceRemoved(
    device::mojom::HidDeviceInfoPtr device_info) {
  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info)) {
    RemoveDevice(FidoHidDevice::GetIdForDevice(*device_info));
  }
}

void FidoHidDiscovery::DeviceChanged(
    device::mojom::HidDeviceInfoPtr device_info) {
  // The changed |device_info| may affect how the device should be filtered.
  // For instance, it may have been updated from a device with no FIDO U2F
  // capabilities to a device with FIDO U2F capabilities.
  //
  // Try adding it again. If the device is already present in |authenticators_|
  // then the updated device will be detected as a duplicate and will not be
  // added.
  //
  // The FidoHidDevice object will retain the old device info. This is fine
  // since it does not rely on any HidDeviceInfo members that could change.
  DeviceAdded(std::move(device_info));
}

void FidoHidDiscovery::OnGetDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> device_infos) {
  for (auto& device_info : device_infos)
    DeviceAdded(std::move(device_info));

  NotifyDiscoveryStarted(true);
}

}  // namespace device
