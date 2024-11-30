// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_android.h"

#include <jni.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/test/bluetooth_scanner_callback.h"
#include "device/bluetooth/test/bluetooth_test_android.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth_test_jni_headers/ChromeBluetoothLeScannerTestUtil_jni.h"
#include "device/bluetooth_test_jni_headers/ChromeBluetoothScanFilter_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace device {

namespace {

constexpr base::TimeDelta kScanDuration = base::Seconds(5);
constexpr int kErrorCode = 3;

class ChromeBluetoothLeScannerTestUtil {
 public:
  ChromeBluetoothLeScannerTestUtil(
      base::android::JavaRef<jobject>* j_fake_bluetooth_adapter,
      BluetoothScannerCallback* bluetooth_scanner_callback);

  bool IsScanning();
  bool StartScan();
  bool ResumeScan();
  bool StopScan();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_scanner_test_util_;
};

ChromeBluetoothLeScannerTestUtil::ChromeBluetoothLeScannerTestUtil(
    base::android::JavaRef<jobject>* j_bluetooth_adapter,
    BluetoothScannerCallback* bluetooth_scanner_callback)
    : j_scanner_test_util_(Java_ChromeBluetoothLeScannerTestUtil_create(
          AttachCurrentThread(),
          *j_bluetooth_adapter,
          reinterpret_cast<intptr_t>(bluetooth_scanner_callback))) {}

bool ChromeBluetoothLeScannerTestUtil::IsScanning() {
  return Java_ChromeBluetoothLeScannerTestUtil_isScanning(AttachCurrentThread(),
                                                          j_scanner_test_util_);
}

bool ChromeBluetoothLeScannerTestUtil::StartScan() {
  return Java_ChromeBluetoothLeScannerTestUtil_startScan(
      AttachCurrentThread(), j_scanner_test_util_,
      kScanDuration.InMilliseconds());
}

bool ChromeBluetoothLeScannerTestUtil::ResumeScan() {
  return Java_ChromeBluetoothLeScannerTestUtil_resumeScan(
      AttachCurrentThread(), j_scanner_test_util_,
      kScanDuration.InMicroseconds());
}

bool ChromeBluetoothLeScannerTestUtil::StopScan() {
  return Java_ChromeBluetoothLeScannerTestUtil_stopScan(AttachCurrentThread(),
                                                        j_scanner_test_util_);
}

}  // namespace

class BluetoothAdapterAndroidTest : public BluetoothTestAndroid {
 public:
  BluetoothAdapterAndroidTest()
      : BluetoothTestAndroid(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // Initializes for running lifecycle tests of |ChromeBluetoothLeScanner|.
  ChromeBluetoothLeScannerTestUtil
  InitChromeBluetoothLeScannerForLifecycleTests();

  // Initializes to run tests of |ChromeBluetoothLeScanner| with the specific
  // |j_bluetooth_adapter|.
  ChromeBluetoothLeScannerTestUtil InitChromeBluetoothLeScanner(
      base::android::JavaRef<jobject>* j_bluetooth_adapter);

  std::unique_ptr<BluetoothScannerCallback> bluetooth_scanner_callback_;
};

ChromeBluetoothLeScannerTestUtil
BluetoothAdapterAndroidTest::InitChromeBluetoothLeScannerForLifecycleTests() {
  InitWithFakeAdapter();

  return InitChromeBluetoothLeScanner(&j_fake_bluetooth_adapter_);
}

ChromeBluetoothLeScannerTestUtil
BluetoothAdapterAndroidTest::InitChromeBluetoothLeScanner(
    base::android::JavaRef<jobject>* j_bluetooth_adapter) {
  bluetooth_scanner_callback_ = std::make_unique<BluetoothScannerCallback>();
  return ChromeBluetoothLeScannerTestUtil(j_bluetooth_adapter,
                                          bluetooth_scanner_callback_.get());
}

TEST_F(BluetoothAdapterAndroidTest, ScanFilterTest) {
  InitWithoutDefaultAdapter();
  BluetoothAdapterAndroid* android_adapter =
      static_cast<BluetoothAdapterAndroid*>(adapter_.get());

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
      android_adapter->CreateAndroidFilter(discovery_filter.get());
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

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerScanWithRealAdapater) {
  InitWithDefaultAdapter();
  if (!adapter_->IsPresent() || !adapter_->IsPowered()) {
    GTEST_SKIP()
        << "Bluetooth adapter not present or not powered; skipping unit test.";
  }

  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScanner(&j_default_bluetooth_adapter_);

  EXPECT_TRUE(scanner_test_util.StartScan());
  EXPECT_TRUE(scanner_test_util.IsScanning());
  EXPECT_TRUE(scanner_test_util.StopScan());
}

TEST_F(BluetoothAdapterAndroidTest, ChromeBluetoothLeScannerStartStopTimeout) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  EXPECT_TRUE(scanner_test_util.StartScan());
  EXPECT_TRUE(scanner_test_util.IsScanning());
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardBy(kScanDuration / 2);
  SimulateLowEnergyDevice(1);
  EXPECT_TRUE(scanner_test_util.StopScan());
  EXPECT_FALSE(scanner_test_util.IsScanning());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 0);
  EXPECT_FALSE(scanner_test_util.IsScanning());
}

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerTimeoutDoesntPauseNextScan) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardBy(kScanDuration / 2);
  SimulateLowEnergyDevice(1);
  scanner_test_util.StopScan();
  // Intentionally not fast forward the task environment so that the timeout
  // callback is called in the next scan cycle.

  EXPECT_TRUE(scanner_test_util.StartScan());
  EXPECT_TRUE(scanner_test_util.IsScanning());
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
}

TEST_F(BluetoothAdapterAndroidTest, ChromeBluetoothLeScannerStartTimeOutStop) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
  EXPECT_FALSE(scanner_test_util.IsScanning());
  scanner_test_util.StopScan();
  EXPECT_FALSE(scanner_test_util.IsScanning());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
}

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerStartTimeoutResumeStop) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
  EXPECT_TRUE(scanner_test_util.ResumeScan());
  EXPECT_TRUE(scanner_test_util.IsScanning());
  SimulateLowEnergyDevice(1);
  EXPECT_TRUE(scanner_test_util.StopScan());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
  EXPECT_FALSE(scanner_test_util.IsScanning());
}

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerTimeOutAfterResumeDoesntPauseNextScan) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  scanner_test_util.ResumeScan();
  SimulateLowEnergyDevice(1);
  scanner_test_util.StopScan();
  // Intentionally not fast forward the task environment so that the timeout
  // callback is called in the next scan cycle.

  EXPECT_TRUE(scanner_test_util.StartScan());
  EXPECT_TRUE(scanner_test_util.IsScanning());
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 2);
}

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerStartTimeOutResumeTimeOutStop) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  scanner_test_util.ResumeScan();
  SimulateLowEnergyDevice(1);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 2);
  EXPECT_FALSE(scanner_test_util.IsScanning());
  EXPECT_TRUE(scanner_test_util.StopScan());
  EXPECT_FALSE(scanner_test_util.IsScanning());
}

TEST_F(BluetoothAdapterAndroidTest, ChromeBluetoothLeScannerStartErrorOut) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();
  ASSERT_NE(bluetooth_scanner_callback_->GetLastErrorCode(), kErrorCode);

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  FailCurrentLeScan(kErrorCode);
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 0);
  EXPECT_FALSE(scanner_test_util.IsScanning());
  EXPECT_EQ(bluetooth_scanner_callback_->GetLastErrorCode(), kErrorCode);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 0);
}

TEST_F(BluetoothAdapterAndroidTest,
       ChromeBluetoothLeScannerStartTimeOutResumeErrorOut) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();
  ASSERT_NE(bluetooth_scanner_callback_->GetLastErrorCode(), kErrorCode);

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  scanner_test_util.ResumeScan();
  FailCurrentLeScan(kErrorCode);
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
  EXPECT_FALSE(scanner_test_util.IsScanning());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
}

TEST_F(BluetoothAdapterAndroidTest, ChromeBluetoothLeScannerFailToStart) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  ForceIllegalStateException();
  EXPECT_FALSE(scanner_test_util.StartScan());
  EXPECT_FALSE(scanner_test_util.IsScanning());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 0);
}

TEST_F(BluetoothAdapterAndroidTest, ChromeBluetoothLeScannerFailToResume) {
  ChromeBluetoothLeScannerTestUtil scanner_test_util =
      InitChromeBluetoothLeScannerForLifecycleTests();

  scanner_test_util.StartScan();
  SimulateLowEnergyDevice(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  ForceIllegalStateException();
  EXPECT_FALSE(scanner_test_util.ResumeScan());
  EXPECT_FALSE(scanner_test_util.IsScanning());
  EXPECT_EQ(bluetooth_scanner_callback_->GetScanFinishCount(), 1);
}

}  // namespace device
