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
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/base/features.h"
#include "device/bluetooth/android/wrappers.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/test/bluetooth_scanner_callback.h"
#include "device/bluetooth/test/bluetooth_test_android.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
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

TEST_F(BluetoothAdapterAndroidTest, GetPairedDevices) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  TestBluetoothAdapterObserver observer(adapter_.get());

  SimulatePairedClassicDevice(1);
  SimulatePairedClassicDevice(2);

  BluetoothAdapter::DeviceList list = adapter_->GetDevices();
  std::unordered_set<std::string> addresses;
  for (auto* device : list) {
    addresses.insert(device->GetAddress());
  }
  EXPECT_EQ(addresses.size(), 2u);
  EXPECT_NE(addresses.find(kTestDeviceAddress1), addresses.end());
  EXPECT_NE(addresses.find(kTestDeviceAddress2), addresses.end());

  // We explicitly omit observer notifications for new paired devices found
  // during GetDevices.
  EXPECT_EQ(observer.device_added_count(), 0);

  EXPECT_TRUE(adapter_->GetDevice(kTestDeviceAddress1));
  EXPECT_TRUE(adapter_->GetDevice(kTestDeviceAddress2));
}

TEST_F(BluetoothAdapterAndroidTest, NotifyObserversForNewPairedDevices) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  TestBluetoothAdapterObserver observer(adapter_.get());

  adapter_->GetDevices();

  SimulatePairedClassicDevice(1, /*notify_callback=*/true);
  ASSERT_EQ(observer.device_added_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);

  SimulatePairedClassicDevice(2, /*notify_callback=*/true);
  ASSERT_EQ(observer.device_added_count(), 2);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress2);
}

TEST_F(BluetoothAdapterAndroidTest, IsConnectedAfterNewDevicePaired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  TestBluetoothAdapterObserver observer(adapter_.get());

  adapter_->GetDevices();

  SimulatePairedClassicDevice(1, /*notify_callback=*/true);
  EXPECT_EQ(observer.device_added_count(), 1);
  EXPECT_EQ(observer.device_changed_count(), 1);
  EXPECT_TRUE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, ExposeUuidFromPairedDevices) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  adapter_->GetDevices();

  SimulatePairedClassicDevice(1);
  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  ASSERT_EQ(uuids.size(), 1u);
  EXPECT_EQ(uuids.begin()->canonical_value(),
            "00001101-0000-1000-8000-00805f9b34fb");

  SimulatePairedClassicDevice(2);
  device = adapter_->GetDevice(kTestDeviceAddress2);
  uuids = device->GetUUIDs();
  ASSERT_EQ(uuids.size(), 1u);
  EXPECT_EQ(uuids.begin()->canonical_value(),
            "00001101-0000-1000-8000-00805f9b34fb");
}

TEST_F(BluetoothAdapterAndroidTest, RemoveExpiredDevicesOnUnpaired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);
  SimulatePairedClassicDevice(2);

  TestBluetoothAdapterObserver observer(adapter_.get());

  UnpairDevice(kTestDeviceAddress1);
  ASSERT_EQ(observer.device_removed_count(), 1);
  EXPECT_EQ(observer.last_device_address(), kTestDeviceAddress1);

  EXPECT_FALSE(adapter_->GetDevice(kTestDeviceAddress1));
  EXPECT_TRUE(adapter_->GetDevice(kTestDeviceAddress2));
}

TEST_F(BluetoothAdapterAndroidTest, RemoveUnpairedDevicesAfterTimeOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);
  SimulatePairedClassicDevice(2);

  TestBluetoothAdapterObserver observer(adapter_.get());

  // Simulate situations where these devices were found in scanning as well.
  for (BluetoothDevice* device : adapter_->GetDevices()) {
    device->UpdateTimestamp();
  }

  UnpairDevice(kTestDeviceAddress1);
  EXPECT_EQ(observer.device_removed_count(), 0);
  EXPECT_TRUE(adapter_->GetDevice(kTestDeviceAddress1));

  // RemoveTimedOutDevices uses base::Time::NowFromSystemTime, so we have to
  // explicitly set devices to be expired.
  for (BluetoothDevice* device : adapter_->GetDevices()) {
    device->SetAsExpiredForTesting();
  }

  task_environment_.FastForwardBy(BluetoothAdapter::timeoutSec +
                                  base::Seconds(1));

  ASSERT_EQ(observer.device_removed_count(), 1);
  EXPECT_EQ(observer.last_device_address(), kTestDeviceAddress1);

  EXPECT_FALSE(adapter_->GetDevice(kTestDeviceAddress1));
  EXPECT_TRUE(adapter_->GetDevice(kTestDeviceAddress2));
}

TEST_F(BluetoothAdapterAndroidTest, UnknownDeviceUnpaired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  adapter_->GetDevices();

  UnpairDevice(kTestDeviceAddress1);
}

TEST_F(BluetoothAdapterAndroidTest, AclConnected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);
  EXPECT_FALSE(device->IsConnected());

  TestBluetoothAdapterObserver observer(adapter_.get());
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/true);
  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_TRUE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, AclDisconnected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  adapter_->GetDevices();

  SimulatePairedClassicDevice(1, /*notify_callback=*/true);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);

  TestBluetoothAdapterObserver observer(adapter_.get());
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/false);
  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_FALSE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, AclConnectedWithDualTransport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);

  TestBluetoothAdapterObserver observer(adapter_.get());
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/true);
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_LE,
                                /*connected=*/true);
  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_TRUE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, AclDisconnectedWithDualTransport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);

  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/true);
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_LE,
                                /*connected=*/true);

  TestBluetoothAdapterObserver observer(adapter_.get());

  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/false);
  EXPECT_EQ(observer.device_changed_count(), 0);
  EXPECT_TRUE(device->IsConnected());

  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_LE,
                                /*connected=*/false);

  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_FALSE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, AclDisconnectedOnAdapterOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);
  std::optional<std::string> device_name = device->GetName();
  int device_type = device->GetType();
  BluetoothDevice::UUIDSet uuids = device->GetUUIDs();
  uint32_t bluetooth_class = device->GetBluetoothClass();

  EXPECT_NE(bluetooth_class, 0x1F00u);

  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_CLASSIC,
                                /*connected=*/true);
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_LE,
                                /*connected=*/true);

  TestBluetoothAdapterObserver observer(adapter_.get());

  adapter_->SetPowered(false, GetCallback(Call::EXPECTED),
                       GetCallback(Call::NOT_EXPECTED));
  task_environment_.FastForwardUntilNoTasksRemain();

  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetName(), device_name);
  EXPECT_EQ(observer.last_device()->GetType(), device_type);
  EXPECT_EQ(observer.last_device()->GetUUIDs(), uuids);
  EXPECT_EQ(observer.last_device()->GetBluetoothClass(), bluetooth_class);
  EXPECT_TRUE(observer.last_device()->IsPaired());
}

// The transport extra of ACL connected/disconnected broadcasts was added in API
// level 33 (Android 13/T). On devices where the extra was not provided, we
// use BluetoothDevice#TRANSPORT_AUTO (0) as the default value, and later
// assign an arbitrary non-zero value (BR/EDR) so that the bit-wise flag takes
// effect. Write unit tests to ensure it works fine.
TEST_F(BluetoothAdapterAndroidTest, AclConnectedWithoutTransport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SimulatePairedClassicDevice(1);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);

  TestBluetoothAdapterObserver observer(adapter_.get());
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_INVALID,
                                /*connected=*/true);
  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_TRUE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, AclDisconnectedWithoutTransport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  adapter_->GetDevices();

  SimulatePairedClassicDevice(1, /*notify_callback=*/true);

  BluetoothDevice* device = adapter_->GetDevice(kTestDeviceAddress1);

  TestBluetoothAdapterObserver observer(adapter_.get());
  SimulateAclConnectStateChange(device, BLUETOOTH_TRANSPORT_INVALID,
                                /*connected=*/false);
  ASSERT_EQ(observer.device_changed_count(), 1);
  EXPECT_EQ(observer.last_device()->GetAddress(), kTestDeviceAddress1);
  EXPECT_FALSE(observer.last_device()->IsConnected());
}

TEST_F(BluetoothAdapterAndroidTest, ScanFailsWithoutLeSupport) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kBluetoothRfcommAndroid);

  InitWithFakeAdapter();

  SetEnabledDeviceTransport(BLUETOOTH_TRANSPORT_CLASSIC);

  base::RunLoop loop;
  adapter_->StartDiscoverySession(
      /*client_name=*/std::string(), base::DoNothing(),
      base::BindLambdaForTesting([&]() { loop.Quit(); }));
  loop.Run();
}

}  // namespace device
