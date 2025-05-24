// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_bluetooth_delegate.h"

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace headless {

using ::blink::WebBluetoothDeviceId;
using ::device::BluetoothUUID;

class HeadlessBluetoothDelegateTest : public testing::Test {
 protected:
  HeadlessBluetoothDelegate delegate_;
};

TEST_F(HeadlessBluetoothDelegateTest, RunBluetoothChooser) {
  auto chooser =
      delegate_.RunBluetoothChooser(/*frame=*/nullptr, /*event_handler=*/{});
  EXPECT_NE(chooser, nullptr);
}

TEST_F(HeadlessBluetoothDelegateTest, ShowBluetoothScanningPrompt) {
  auto prompt = delegate_.ShowBluetoothScanningPrompt(/*frame=*/nullptr,
                                                      /*event_handler=*/{});
  EXPECT_EQ(prompt, nullptr);
}

TEST_F(HeadlessBluetoothDelegateTest, GetWebBluetoothDeviceId) {
  auto id =
      delegate_.GetWebBluetoothDeviceId(/*frame=*/nullptr, "12:34:56:78:90:AB");
  EXPECT_FALSE(id.IsValid());
}

TEST_F(HeadlessBluetoothDelegateTest, GetDeviceAddress) {
  WebBluetoothDeviceId id;
  auto address = delegate_.GetDeviceAddress(/*frame=*/nullptr, id);
  EXPECT_TRUE(address.empty());
}

TEST_F(HeadlessBluetoothDelegateTest, AddScannedDevice) {
  auto id = delegate_.AddScannedDevice(/*frame=*/nullptr, "12:34:56:78:90:AB");
  EXPECT_FALSE(id.IsValid());
}

TEST_F(HeadlessBluetoothDelegateTest, GrantServiceAccessPermission) {
  auto id = delegate_.GrantServiceAccessPermission(
      /*frame=*/nullptr, /*device=*/nullptr, /*options=*/nullptr);
  EXPECT_FALSE(id.IsValid());
}

TEST_F(HeadlessBluetoothDelegateTest, HasDevicePermission) {
  EXPECT_FALSE(
      delegate_.HasDevicePermission(/*frame=*/nullptr, WebBluetoothDeviceId()));
}

TEST_F(HeadlessBluetoothDelegateTest, MayUseBluetooth) {
  EXPECT_TRUE(delegate_.MayUseBluetooth(/*rfh=*/nullptr));
}

TEST_F(HeadlessBluetoothDelegateTest, IsAllowedToAccessService) {
  EXPECT_FALSE(delegate_.IsAllowedToAccessService(
      /*frame=*/nullptr, WebBluetoothDeviceId(),
      BluetoothUUID("12345678-1234-5678-9abc-def123456789")));
}

TEST_F(HeadlessBluetoothDelegateTest, IsAllowedToAccessAtLeastOneService) {
  EXPECT_FALSE(delegate_.IsAllowedToAccessAtLeastOneService(
      /*frame=*/nullptr, WebBluetoothDeviceId()));
}

TEST_F(HeadlessBluetoothDelegateTest, IsAllowedToAccessManufacturerData) {
  EXPECT_FALSE(delegate_.IsAllowedToAccessManufacturerData(
      /*frame=*/nullptr, WebBluetoothDeviceId(), /*manufacturer_code=*/0));
}

TEST_F(HeadlessBluetoothDelegateTest, GetPermittedDevices) {
  EXPECT_TRUE(delegate_.GetPermittedDevices(/*frame=*/nullptr).empty());
}

}  // namespace headless
