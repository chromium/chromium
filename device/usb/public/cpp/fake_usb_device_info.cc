// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/public/cpp/fake_usb_device_info.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/public/cpp/usb_utils.h"

namespace device {

void FakeUsbDeviceInfo::Observer::OnDeviceRemoved(
    scoped_refptr<FakeUsbDeviceInfo> device) {}

void FakeUsbDeviceInfo::SetDefault() {
  device_info_.guid = base::GenerateGUID();
  device_info_.usb_version_major = 0x02;
  device_info_.usb_version_minor = 0x00;
  device_info_.usb_version_subminor = 0x00;
  device_info_.class_code = 0xff;
  device_info_.subclass_code = 0xff;
  device_info_.protocol_code = 0xff;
  device_info_.device_version_major = 0x01;
  device_info_.device_version_minor = 0x00;
  device_info_.device_version_subminor = 0x00;
}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t vendor_id, uint16_t product_id)
    : FakeUsbDeviceInfo(vendor_id, product_id, "", "", "") {}

FakeUsbDeviceInfo::FakeUsbDeviceInfo(uint16_t vendor_id,
                                     uint16_t product_id,
                                     const std::string& manufacturer_string,
                                     const std::string& product_string,
                                     const std::string& serial_number) {
  SetDefault();
  device_info_.vendor_id = vendor_id;
  device_info_.product_id = product_id;
  device_info_.manufacturer_name = base::UTF8ToUTF16(manufacturer_string);
  device_info_.product_name = base::UTF8ToUTF16(product_string),
  device_info_.serial_number = base::UTF8ToUTF16(serial_number);
}

FakeUsbDeviceInfo::~FakeUsbDeviceInfo() = default;

void FakeUsbDeviceInfo::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeUsbDeviceInfo::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeUsbDeviceInfo::NotifyDeviceRemoved() {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(this);
}

}  // namespace device
