// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_usbfs.h"

#include <linux/usbdevice_fs.h>

#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
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
    ON_CALL(*this, Initialize)
        .WillByDefault(
            [this](base::ScopedFD fd, base::ScopedFD lifeline_fd,
                   base::WeakPtr<UsbDeviceHandleUsbfs> device_handle,
                   scoped_refptr<base::SequencedTaskRunner> task_runner) {
              device_handle_ = std::move(device_handle);
              task_runner_ = std::move(task_runner);
            });
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

  // Simulates OnFileCanWriteWithoutBlocking() posting reaped URBs back to
  // |device_handle_| without requiring a real usbfs file descriptor for
  // FileDescriptorWatcher and ioctl(USBDEVFS_REAPURBNDELAY).
  void NotifyReapedUrbs(const std::vector<usbdevfs_urb*>& urbs) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&UsbDeviceHandleUsbfs::ReapedUrbs,
                                          device_handle_, urbs));
  }

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
  MOCK_METHOD(void, DiscardUrb, (UsbDeviceHandleUsbfs::Transfer*), (override));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  MOCK_METHOD(bool,
              DetachInterface,
              (int,
               const CombinedInterfaceInfo& interfaceInfo,
               bool allow_unrestricted_detach),
              (override));
  MOCK_METHOD(bool, ReattachInterface, (int), (override));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

 private:
  base::WeakPtr<UsbDeviceHandleUsbfs> device_handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class TestUsbDeviceHandleUsbfs : public UsbDeviceHandleUsbfs {
 public:
  TestUsbDeviceHandleUsbfs(
      scoped_refptr<UsbDevice> device,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      std::unique_ptr<BlockingTaskRunnerHelper> helper,
      base::OnceClosure on_destroy = base::DoNothing())
      : UsbDeviceHandleUsbfs(std::move(device),
                             /*fd=*/base::ScopedFD(),
                             /*lifeline_fd=*/base::ScopedFD(),
                             /*client_id=*/"",
                             std::move(blocking_task_runner),
                             std::move(helper)),
        on_destroy_(std::move(on_destroy)) {
    ON_CALL(*this, SubmitUrb).WillByDefault(testing::Return(0));
  }

  MOCK_METHOD(int, SubmitUrb, (usbdevfs_urb*), (override));

 protected:
  ~TestUsbDeviceHandleUsbfs() override {
    if (on_destroy_) {
      std::move(on_destroy_).Run();
    }
  }

 private:
  base::OnceClosure on_destroy_;
};

#if BUILDFLAG(IS_ANDROID)
// Simulates UsbDeviceHandleAndroid, which overrides FinishClose() to call
// ReleaseFileDescriptor() instead of closing the file descriptor directly.
class TestAndroidUsbDeviceHandleUsbfs : public TestUsbDeviceHandleUsbfs {
 public:
  TestAndroidUsbDeviceHandleUsbfs(
      scoped_refptr<UsbDevice> device,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      std::unique_ptr<BlockingTaskRunnerHelper> helper,
      base::OnceClosure on_destroy,
      base::OnceClosure release_fd_callback)
      : TestUsbDeviceHandleUsbfs(std::move(device),
                                 std::move(blocking_task_runner),
                                 std::move(helper),
                                 std::move(on_destroy)),
        release_fd_callback_(std::move(release_fd_callback)) {}

 protected:
  ~TestAndroidUsbDeviceHandleUsbfs() override = default;

  void FinishClose() override {
    ReleaseFileDescriptor(std::move(release_fd_callback_));
  }

 private:
  base::OnceClosure release_fd_callback_;
};
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static scoped_refptr<TestUsbDeviceHandleUsbfs> CreateTestHandle(
      scoped_refptr<UsbDevice> usb_device,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      std::unique_ptr<MockBlockingTaskRunnerHelper> helper,
      base::OnceClosure on_destroy = base::DoNothing()) {
    auto handle = base::MakeRefCounted<TestUsbDeviceHandleUsbfs>(
        usb_device, std::move(blocking_task_runner), std::move(helper),
        std::move(on_destroy));
    usb_device->handles().push_back(handle.get());
    return handle;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  static scoped_refptr<TestAndroidUsbDeviceHandleUsbfs> CreateAndroidTestHandle(
      scoped_refptr<UsbDevice> usb_device,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      std::unique_ptr<MockBlockingTaskRunnerHelper> helper,
      base::OnceClosure on_destroy,
      base::OnceClosure release_fd_callback) {
    auto handle = base::MakeRefCounted<TestAndroidUsbDeviceHandleUsbfs>(
        usb_device, std::move(blocking_task_runner), std::move(helper),
        std::move(on_destroy), std::move(release_fd_callback));
    usb_device->handles().push_back(handle.get());
    return handle;
  }
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
TEST_F(UsbDeviceHandleUsbfsTest, ClaimForwardsAllowUnrestrictedDetach) {
  auto helper1 = std::make_unique<MockBlockingTaskRunnerHelper>();
  EXPECT_CALL(*helper1,
              DetachInterface(1, testing::_, /*allow_unrestricted_detach=*/false))
      .WillOnce(testing::Return(true));
  scoped_refptr<UsbDeviceHandleUsbfs> restricted_handle =
      CreateHandle(usb_device_, std::move(helper1));

  TestFuture<bool> claim_restricted_future;
  restricted_handle->ClaimInterface(1, claim_restricted_future.GetCallback());
  ASSERT_TRUE(claim_restricted_future.Get());
  restricted_handle->Close();

  auto helper2 = std::make_unique<MockBlockingTaskRunnerHelper>();
  EXPECT_CALL(*helper2,
              DetachInterface(2, testing::_, /*allow_unrestricted_detach=*/true))
      .WillOnce(testing::Return(true));
  scoped_refptr<UsbDeviceHandleUsbfs> unrestricted_handle =
      CreateHandle(usb_device_, std::move(helper2));
  unrestricted_handle->set_allow_unrestricted_detach(true);

  TestFuture<bool> claim_unrestricted_future;
  unrestricted_handle->ClaimInterface(
      2, claim_unrestricted_future.GetCallback());
  ASSERT_TRUE(claim_unrestricted_future.Get());
  unrestricted_handle->Close();
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(UsbDeviceHandleUsbfsTest, CloseWithUnreapedTimedOutTransfers) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  auto helper =
      std::make_unique<testing::NiceMock<MockBlockingTaskRunnerHelper>>();
  MockBlockingTaskRunnerHelper* helper_ptr = helper.get();

  TestFuture<void> destroyed_future;
  auto handle =
      CreateTestHandle(usb_device_, blocking_task_runner, std::move(helper),
                       destroyed_future.GetCallback());

  usbdevfs_urb* urb1 = nullptr;
  usbdevfs_urb* urb2 = nullptr;
  EXPECT_CALL(*handle, SubmitUrb)
      .WillOnce([&urb1](usbdevfs_urb* urb) {
        urb1 = urb;
        return 0;
      })
      .WillOnce([&urb2](usbdevfs_urb* urb) {
        urb2 = urb;
        return 0;
      });

  using TransferResultFuture =
      TestFuture<mojom::UsbTransferStatus, scoped_refptr<base::RefCountedBytes>,
                 size_t>;

  TransferResultFuture transfer_future1;
  auto buffer1 = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<uint8_t>{1, 2, 3, 4});
  handle->ControlTransfer(mojom::UsbTransferDirection::INBOUND,
                          mojom::UsbControlTransferType::STANDARD,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0, /*value=*/0, /*index=*/0,
                          std::move(buffer1),
                          /*timeout=*/1, transfer_future1.GetCallback());

  TransferResultFuture transfer_future2;
  auto buffer2 = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<uint8_t>{5, 6, 7, 8});
  handle->ControlTransfer(mojom::UsbTransferDirection::OUTBOUND,
                          mojom::UsbControlTransferType::STANDARD,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0, /*value=*/0, /*index=*/0,
                          std::move(buffer2),
                          /*timeout=*/1, transfer_future2.GetCallback());

  ASSERT_NE(urb1, nullptr);
  ASSERT_NE(urb2, nullptr);

  // Wait for both transfers to time out.
  EXPECT_EQ(transfer_future1.Get<0>(), mojom::UsbTransferStatus::TIMEOUT);
  EXPECT_EQ(transfer_future2.Get<0>(), mojom::UsbTransferStatus::TIMEOUT);

  // Flush the blocking task runner and main sequence so that DiscardUrb() and
  // UrbDiscarded() have completed and released their references to |handle|.
  TestFuture<void> discard_flushed;
  blocking_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         discard_flushed.GetCallback());
  ASSERT_TRUE(discard_flushed.Wait());

  // Queue a task on the blocking task runner that simulates the kernel/reaper
  // accessing the URBs right before
  // BlockingTaskRunnerHelper::CloseFileDescriptor() runs:
  // - |urb1| is reaped before CloseFileDescriptor() runs (cleaned up by
  //   ReapedUrbs()).
  // - |urb2| remains un-reaped when CloseFileDescriptor() runs (cleaned up by
  //   FinishCloseComplete()).
  base::WaitableEvent close_called_event;
  blocking_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&close_called_event, helper_ptr, urb1, urb2]() {
                       base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
                       close_called_event.Wait();

                       urb1->status = -ENOENT;
                       urb1->actual_length = 0;

                       urb2->status = -ENOENT;
                       urb2->actual_length = 0;

                       helper_ptr->NotifyReapedUrbs({urb1});
                     }));

  // Close the handle and drop the caller's last reference while the URBs are
  // still pending on the blocking task runner.
  handle->Close();
  handle.reset();

  // The handle and its transfers must remain alive until
  // BlockingTaskRunnerHelper::CloseFileDescriptor() and FinishCloseComplete()
  // have run.
  EXPECT_FALSE(destroyed_future.IsReady());

  close_called_event.Signal();
  ASSERT_TRUE(destroyed_future.Wait());
}

TEST_F(UsbDeviceHandleUsbfsTest, CloseCalledReentrantlyFromTransferCallback) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  auto helper =
      std::make_unique<testing::NiceMock<MockBlockingTaskRunnerHelper>>();

  TestFuture<void> destroyed_future;
  auto handle =
      CreateTestHandle(usb_device_, blocking_task_runner, std::move(helper),
                       destroyed_future.GetCallback());

  TestFuture<mojom::UsbTransferStatus> status_future;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<uint8_t>{1, 2, 3, 4});
  handle->ControlTransfer(mojom::UsbTransferDirection::INBOUND,
                          mojom::UsbControlTransferType::STANDARD,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0, /*value=*/0, /*index=*/0,
                          std::move(buffer),
                          /*timeout=*/0,
                          base::BindLambdaForTesting(
                              [&](mojom::UsbTransferStatus status,
                                  scoped_refptr<base::RefCountedBytes> buffer,
                                  size_t bytes_transferred) {
                                handle->Close();
                                status_future.SetValue(status);
                              }));

  handle->Close();
  handle.reset();

  EXPECT_EQ(status_future.Get(), mojom::UsbTransferStatus::CANCELLED);
  ASSERT_TRUE(destroyed_future.Wait());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
TEST_F(UsbDeviceHandleUsbfsTest,
       CloseViaReleaseFileDescriptorWithUnreapedTimedOutTransfers) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  auto helper =
      std::make_unique<testing::NiceMock<MockBlockingTaskRunnerHelper>>();
  MockBlockingTaskRunnerHelper* helper_ptr = helper.get();

  TestFuture<void> destroyed_future;
  TestFuture<void> release_fd_future;
  auto handle = CreateAndroidTestHandle(
      usb_device_, blocking_task_runner, std::move(helper),
      destroyed_future.GetCallback(), release_fd_future.GetCallback());

  usbdevfs_urb* urb1 = nullptr;
  usbdevfs_urb* urb2 = nullptr;
  EXPECT_CALL(*handle, SubmitUrb)
      .WillOnce([&urb1](usbdevfs_urb* urb) {
        urb1 = urb;
        return 0;
      })
      .WillOnce([&urb2](usbdevfs_urb* urb) {
        urb2 = urb;
        return 0;
      });

  using TransferResultFuture =
      TestFuture<mojom::UsbTransferStatus, scoped_refptr<base::RefCountedBytes>,
                 size_t>;

  TransferResultFuture transfer_future1;
  auto buffer1 = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<uint8_t>{1, 2, 3, 4});
  handle->ControlTransfer(mojom::UsbTransferDirection::INBOUND,
                          mojom::UsbControlTransferType::STANDARD,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0, /*value=*/0, /*index=*/0,
                          std::move(buffer1),
                          /*timeout=*/1, transfer_future1.GetCallback());

  TransferResultFuture transfer_future2;
  auto buffer2 = base::MakeRefCounted<base::RefCountedBytes>(
      std::vector<uint8_t>{5, 6, 7, 8});
  handle->ControlTransfer(mojom::UsbTransferDirection::OUTBOUND,
                          mojom::UsbControlTransferType::STANDARD,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0, /*value=*/0, /*index=*/0,
                          std::move(buffer2),
                          /*timeout=*/1, transfer_future2.GetCallback());

  ASSERT_NE(urb1, nullptr);
  ASSERT_NE(urb2, nullptr);
  EXPECT_EQ(transfer_future1.Get<0>(), mojom::UsbTransferStatus::TIMEOUT);
  EXPECT_EQ(transfer_future2.Get<0>(), mojom::UsbTransferStatus::TIMEOUT);

  // Flush the blocking task runner and main sequence so that DiscardUrb() and
  // UrbDiscarded() have completed and released their references to |handle|.
  TestFuture<void> discard_flushed;
  blocking_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         discard_flushed.GetCallback());
  ASSERT_TRUE(discard_flushed.Wait());

  // Queue a task on the blocking task runner that simulates the kernel/reaper
  // accessing the URBs right before
  // BlockingTaskRunnerHelper::ReleaseFileDescriptor() runs:
  // - |urb1| is reaped before ReleaseFileDescriptor() runs (cleaned up by
  //   ReapedUrbs()).
  // - |urb2| remains un-reaped when ReleaseFileDescriptor() runs (cleaned up by
  //   FinishCloseComplete()).
  base::WaitableEvent close_called_event;
  blocking_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&close_called_event, helper_ptr, urb1, urb2]() {
                       base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
                       close_called_event.Wait();

                       urb1->status = -ENOENT;
                       urb1->actual_length = 0;

                       urb2->status = -ENOENT;
                       urb2->actual_length = 0;

                       helper_ptr->NotifyReapedUrbs({urb1});
                     }));

  // Close the handle (via ReleaseFileDescriptor) and drop the caller's last
  // reference while the URBs are still pending on the blocking task runner.
  handle->Close();
  handle.reset();

  // The handle and its transfers must remain alive until
  // BlockingTaskRunnerHelper::ReleaseFileDescriptor(), its callback, and
  // FinishCloseComplete() have run.
  EXPECT_FALSE(release_fd_future.IsReady());
  EXPECT_FALSE(destroyed_future.IsReady());

  close_called_event.Signal();
  ASSERT_TRUE(release_fd_future.Wait());
  ASSERT_TRUE(destroyed_future.Wait());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace device
