// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_usbfs.h"

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/usb/mock_usb_device.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class MockBlockingTaskRunnerHelper
    : public UsbDeviceHandleUsbfs::BlockingTaskRunnerHelper {
 public:
  MockBlockingTaskRunnerHelper() {
    ON_CALL(*this, ClaimInterface).WillByDefault(testing::Return(true));
    ON_CALL(*this, ReleaseInterface).WillByDefault(testing::Return(true));
    ON_CALL(*this, SetInterface).WillByDefault(testing::Return(true));
    ON_CALL(*this, ResetDevice).WillByDefault(testing::Return(true));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
    ON_CALL(*this, DetachInterface).WillByDefault(testing::Return(true));
    ON_CALL(*this, ReattachInterface).WillByDefault(testing::Return(true));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  }
  ~MockBlockingTaskRunnerHelper() override = default;

  MOCK_METHOD(void,
              Initialize,
              (base::ScopedFD,
               base::ScopedFD,
               base::WeakPtr<UsbDeviceHandleUsbfs>,
               scoped_refptr<base::SequencedTaskRunner>),
              (override));
  MOCK_METHOD(bool, ClaimInterface, (int), (override));
  MOCK_METHOD(bool, ReleaseInterface, (int), (override));
  MOCK_METHOD(bool, SetInterface, (int, int), (override));
  MOCK_METHOD(bool, ResetDevice, (), (override));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  MOCK_METHOD(bool,
              DetachInterface,
              (int, const CombinedInterfaceInfo& interfaceInfo),
              (override));
  MOCK_METHOD(bool, ReattachInterface, (int), (override));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
};

class UsbDeviceHandleUsbfsTest : public ::testing::Test {
 protected:
  UsbDeviceHandleUsbfsTest()
      : usb_device_(base::MakeRefCounted<MockUsbDevice>(/*bus_number=*/0,
                                                        /*port_number=*/0)) {
    mojom::UsbConfigurationInfoPtr config = BuildUsbConfigurationInfoPtr(
        /*configuration_value=*/0, /*self_powered=*/false,
        /*remote_wakeup=*/false, /*maximum_power=*/0);
    config->interfaces.push_back(CreateInterface(0));
    config->interfaces.push_back(CreateInterface(1));
    config->interfaces.push_back(CreateInterface(2));
    usb_device_->AddMockConfig(std::move(config));
    handle1_ = CreateHandle(usb_device_);
    handle2_ = CreateHandle(usb_device_);
  }
  ~UsbDeviceHandleUsbfsTest() override {
    handle1_->Close();
    handle2_->Close();
  }

  static mojom::UsbInterfaceInfoPtr CreateInterface(int interface_number) {
    return BuildUsbInterfaceInfoPtr(
        interface_number, /*alternate_setting=*/0, /*interface_class=*/0,
        /*interface_subclass=*/0, /*interface_protocol=*/0);
  }

  static scoped_refptr<UsbDeviceHandleUsbfs> CreateHandle(
      scoped_refptr<UsbDevice> usb_device,
      std::unique_ptr<MockBlockingTaskRunnerHelper> helper =
          std::make_unique<MockBlockingTaskRunnerHelper>()) {
    scoped_refptr<UsbDeviceHandleUsbfs> handle =
        base::MakeRefCounted<UsbDeviceHandleUsbfs>(
            usb_device, /*fd=*/base::ScopedFD(),
            /*lifeline_fd=*/base::ScopedFD(),
            /*client_id=*/"", UsbService::CreateBlockingTaskRunner(),
            std::move(helper));
    usb_device->handles().push_back(handle.get());
    return handle;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutomaticUsbDetach};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockUsbDevice> usb_device_;
  scoped_refptr<UsbDeviceHandleUsbfs> handle1_;
  scoped_refptr<UsbDeviceHandleUsbfs> handle2_;
};

namespace {

using ::base::test::TestFuture;

TEST_F(UsbDeviceHandleUsbfsTest, ClaimAndRelease) {
  TestFuture<bool> claim_interface_future;
  handle1_->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  TestFuture<bool> release_interface_future;
  handle1_->ReleaseInterface(1, release_interface_future.GetCallback());
  ASSERT_TRUE(release_interface_future.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ReleaseWrongInterface) {
  TestFuture<bool> claim_interface_future;
  handle1_->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  TestFuture<bool> release_interface_future;
  handle1_->ReleaseInterface(2, release_interface_future.GetCallback());
  ASSERT_FALSE(release_interface_future.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ClaimSameInterfaceConcurrently) {
  TestFuture<bool> claim_interface_future1;
  handle1_->ClaimInterface(1, claim_interface_future1.GetCallback());
  ASSERT_TRUE(claim_interface_future1.Get());

  TestFuture<bool> claim_interface_future2;
  handle2_->ClaimInterface(1, claim_interface_future2.GetCallback());
  ASSERT_FALSE(claim_interface_future2.Get());

  TestFuture<bool> release_interface_future1;
  handle2_->ReleaseInterface(1, release_interface_future1.GetCallback());
  ASSERT_FALSE(release_interface_future1.Get());

  TestFuture<bool> release_interface_future2;
  handle1_->ReleaseInterface(1, release_interface_future2.GetCallback());
  ASSERT_TRUE(release_interface_future2.Get());

  TestFuture<bool> claim_interface_future3;
  handle2_->ClaimInterface(1, claim_interface_future3.GetCallback());
  ASSERT_TRUE(claim_interface_future3.Get());

  TestFuture<bool> release_interface_future3;
  handle2_->ReleaseInterface(1, release_interface_future3.GetCallback());
  ASSERT_TRUE(release_interface_future3.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ClaimDifferentInterfacesConcurrently) {
  TestFuture<bool> claim_interface_future1;
  handle1_->ClaimInterface(1, claim_interface_future1.GetCallback());
  ASSERT_TRUE(claim_interface_future1.Get());

  TestFuture<bool> claim_interface_future2;
  handle2_->ClaimInterface(2, claim_interface_future2.GetCallback());
  ASSERT_TRUE(claim_interface_future2.Get());

  TestFuture<bool> release_interface_future1;
  handle2_->ReleaseInterface(2, release_interface_future1.GetCallback());
  ASSERT_TRUE(release_interface_future1.Get());

  TestFuture<bool> release_interface_future2;
  handle1_->ReleaseInterface(1, release_interface_future2.GetCallback());
  ASSERT_TRUE(release_interface_future2.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ClaimSameInterfaceSequentially) {
  TestFuture<bool> claim_interface_future1;
  handle1_->ClaimInterface(1, claim_interface_future1.GetCallback());
  ASSERT_TRUE(claim_interface_future1.Get());

  TestFuture<bool> release_interface_future1;
  handle1_->ReleaseInterface(1, release_interface_future1.GetCallback());
  ASSERT_TRUE(release_interface_future1.Get());

  TestFuture<bool> claim_interface_future2;
  handle2_->ClaimInterface(1, claim_interface_future2.GetCallback());
  ASSERT_TRUE(claim_interface_future2.Get());

  TestFuture<bool> release_interface_future2;
  handle2_->ReleaseInterface(1, release_interface_future2.GetCallback());
  ASSERT_TRUE(release_interface_future2.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, SetInterfaceAlternateSettingUnclaimed) {
  TestFuture<bool> set_interface_future;
  handle1_->SetInterfaceAlternateSetting(1, 0,
                                         set_interface_future.GetCallback());
  ASSERT_FALSE(set_interface_future.Get());

  TestFuture<bool> claim_interface_future;
  handle1_->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  TestFuture<bool> set_interface_future2;
  handle1_->SetInterfaceAlternateSetting(1, 0,
                                         set_interface_future2.GetCallback());
  ASSERT_TRUE(set_interface_future2.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ResetDevice) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUsbResetInvalidatesConfiguration);

  mojom::UsbConfigurationInfoPtr config = BuildUsbConfigurationInfoPtr(
      /*configuration_value=*/1, /*self_powered=*/false,
      /*remote_wakeup=*/false, /*maximum_power=*/0);
  config->interfaces.push_back(CreateInterface(0));
  config->interfaces.push_back(CreateInterface(1));
  config->interfaces.push_back(CreateInterface(2));
  usb_device_->AddMockConfig(std::move(config));

  // Set the active configuration to 1.
  usb_device_->ActiveConfigurationChanged(1);
  EXPECT_NE(usb_device_->GetActiveConfiguration(), nullptr);

  // Claim interface 1.
  TestFuture<bool> claim_interface_future;
  handle1_->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  // Reset the device.
  TestFuture<bool> reset_future;
  handle1_->ResetDevice(reset_future.GetCallback());
  ASSERT_TRUE(reset_future.Get());

  // Resetting the device invalidates active configuration (sets it back to 0)
  // and clears claimed interfaces.
  EXPECT_EQ(usb_device_->device_info().active_configuration, 0);

  // Setting alternate setting on the previously claimed interface now fails.
  TestFuture<bool> set_interface_future;
  handle1_->SetInterfaceAlternateSetting(1, 0,
                                         set_interface_future.GetCallback());
  ASSERT_FALSE(set_interface_future.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ResetDeviceFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kWebUsbResetInvalidatesConfiguration);

  mojom::UsbConfigurationInfoPtr config = BuildUsbConfigurationInfoPtr(
      /*configuration_value=*/1, /*self_powered=*/false,
      /*remote_wakeup=*/false, /*maximum_power=*/0);
  config->interfaces.push_back(CreateInterface(0));
  config->interfaces.push_back(CreateInterface(1));
  config->interfaces.push_back(CreateInterface(2));
  usb_device_->AddMockConfig(std::move(config));

  // Set the active configuration to 1.
  usb_device_->ActiveConfigurationChanged(1);
  EXPECT_NE(usb_device_->GetActiveConfiguration(), nullptr);

  // Claim interface 1.
  TestFuture<bool> claim_interface_future;
  handle1_->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  // Reset the device.
  TestFuture<bool> reset_future;
  handle1_->ResetDevice(reset_future.GetCallback());
  ASSERT_TRUE(reset_future.Get());

  // When the feature is disabled, active configuration is preserved.
  EXPECT_EQ(usb_device_->device_info().active_configuration, 1);

  // Setting alternate setting on the claimed interface still succeeds.
  TestFuture<bool> set_interface_future;
  handle1_->SetInterfaceAlternateSetting(1, 0,
                                         set_interface_future.GetCallback());
  ASSERT_TRUE(set_interface_future.Get());
}

TEST_F(UsbDeviceHandleUsbfsTest, ResetDeviceFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUsbResetInvalidatesConfiguration);

  auto helper = std::make_unique<MockBlockingTaskRunnerHelper>();
  ON_CALL(*helper, ResetDevice).WillByDefault(testing::Return(false));
  scoped_refptr<UsbDeviceHandleUsbfs> handle =
      CreateHandle(usb_device_, std::move(helper));

  mojom::UsbConfigurationInfoPtr config = BuildUsbConfigurationInfoPtr(
      /*configuration_value=*/1, /*self_powered=*/false,
      /*remote_wakeup=*/false, /*maximum_power=*/0);
  config->interfaces.push_back(CreateInterface(0));
  config->interfaces.push_back(CreateInterface(1));
  config->interfaces.push_back(CreateInterface(2));
  usb_device_->AddMockConfig(std::move(config));

  // Set the active configuration to 1.
  usb_device_->ActiveConfigurationChanged(1);
  EXPECT_NE(usb_device_->GetActiveConfiguration(), nullptr);

  // Claim interface 1.
  TestFuture<bool> claim_interface_future;
  handle->ClaimInterface(1, claim_interface_future.GetCallback());
  ASSERT_TRUE(claim_interface_future.Get());

  // Reset the device (which fails).
  TestFuture<bool> reset_future;
  handle->ResetDevice(reset_future.GetCallback());
  ASSERT_FALSE(reset_future.Get());

  // Even if reset fails, active configuration is invalidated (set back to 0)
  // and claimed interfaces are cleared.
  EXPECT_EQ(usb_device_->device_info().active_configuration, 0);

  // Setting alternate setting on the previously claimed interface now fails.
  TestFuture<bool> set_interface_future;
  handle->SetInterfaceAlternateSetting(1, 0,
                                       set_interface_future.GetCallback());
  ASSERT_FALSE(set_interface_future.Get());

  handle->Close();
}

}  // namespace

}  // namespace device
