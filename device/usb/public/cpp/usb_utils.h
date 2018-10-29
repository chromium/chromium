// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_USB_UTILS_H_
#define DEVICE_USB_PUBLIC_CPP_USB_UTILS_H_

#include <vector>

#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"

namespace device {

class UsbDevice;

bool UsbDeviceFilterMatches(const mojom::UsbDeviceFilter& filter,
                            const UsbDevice& device);

bool UsbDeviceFilterMatches(const mojom::UsbDeviceFilter& filter,
                            const mojom::UsbDeviceInfo& device_info);

bool UsbDeviceFilterMatchesAny(
    const std::vector<mojom::UsbDeviceFilterPtr>& filters,
    const UsbDevice& device);

bool UsbDeviceFilterMatchesAny(
    const std::vector<mojom::UsbDeviceFilterPtr>& filters,
    const mojom::UsbDeviceInfo& device_info);

std::vector<mojom::UsbIsochronousPacketPtr> BuildIsochronousPacketArray(
    const std::vector<uint32_t>& packet_lengths,
    mojom::UsbTransferStatus status);

}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_USB_UTILS_H_
