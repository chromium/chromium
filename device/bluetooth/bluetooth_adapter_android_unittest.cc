// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/test/bluetooth_test_android.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth_test_jni_headers/ChromeBluetoothScanFilter_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace device {

class BluetoothAdapterAndroidTest : public BluetoothTestAndroid {
 public:
  BluetoothAdapterAndroidTest() {
    InitWithoutDefaultAdapter();
    android_adapter_ = static_cast<BluetoothAdapterAndroid*>(adapter_.get());
  }

 protected:
  raw_ptr<BluetoothAdapterAndroid> android_adapter_;
};

TEST_F(BluetoothAdapterAndroidTest, ScanFilterTest) {
  auto discovery_filter =
      std::make_unique<BluetoothDiscoveryFilter>(BLUETOOTH_TRANSPORT_LE);
  std::string test_uuid = "00000000-0000-0000-8000-000000000001";
  std::string test_uuid2 = "00000000-0000-0000-8000-000000000002";
  BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(BluetoothUUID(test_uuid));
  device_filter.uuids.insert(BluetoothUUID(test_uuid2));
  discovery_filter->AddDeviceFilter(device_filter);
  std::string test_uuid3 = "00000000-0000-0000-8000-000000000003";
  BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
  std::string test_name = "test name";
  device_filter2.name = test_name;
  device_filter2.uuids.insert(BluetoothUUID(test_uuid3));
  discovery_filter->AddDeviceFilter(device_filter2);
  auto scan_filter_list_java_object =
      android_adapter_->CreateAndroidFilter(discovery_filter.get());
  auto scan_filter_java_object = Java_ChromeBluetoothScanFilter_getFromList(
      AttachCurrentThread(), scan_filter_list_java_object, /*index=*/0);
  auto scan_filter_java_object_2 = Java_ChromeBluetoothScanFilter_getFromList(
      AttachCurrentThread(), scan_filter_list_java_object, /*index=*/1);
  std::string uuid = ConvertJavaStringToUTF8(
      AttachCurrentThread(),
      Java_ChromeBluetoothScanFilter_getServiceUuid(AttachCurrentThread(),
                                                    scan_filter_java_object));
  EXPECT_EQ(uuid, test_uuid);
  std::string uuid3 = ConvertJavaStringToUTF8(
      AttachCurrentThread(),
      Java_ChromeBluetoothScanFilter_getServiceUuid(AttachCurrentThread(),
                                                    scan_filter_java_object_2));
  EXPECT_EQ(uuid3, test_uuid3);
  std::string name = ConvertJavaStringToUTF8(
      AttachCurrentThread(),
      Java_ChromeBluetoothScanFilter_getDeviceName(AttachCurrentThread(),
                                                   scan_filter_java_object_2));
  EXPECT_EQ(name, test_name);
}

}  // namespace device
