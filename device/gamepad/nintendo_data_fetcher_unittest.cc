// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/nintendo_data_fetcher.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_service.h"
#include "services/device/device_service_test_base.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/public/cpp/test/mock_hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

#if BUILDFLAG(IS_MAC)
const uint64_t kTestDeviceId = 123;
#elif BUILDFLAG(IS_WIN)
const wchar_t kTestDeviceId[] = L"123";
#else
const char kTestDeviceId[] = "123";
#endif

const char kPhysicalDeviceId[] = "1";

void BindHidManager(mojom::DeviceService* service,
                    scoped_refptr<base::SequencedTaskRunner> task_runner,
                    mojo::PendingReceiver<mojom::HidManager> receiver) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&mojom::DeviceService::BindHidManager,
                     base::Unretained(service), std::move(receiver)));
}

}  // namespace

// Main test fixture
class NintendoDataFetcherTest : public DeviceServiceTestBase {
 public:
  NintendoDataFetcherTest() = default;
  NintendoDataFetcherTest(const NintendoDataFetcherTest&) = delete;
  NintendoDataFetcherTest& operator=(const NintendoDataFetcherTest&) = delete;

  void SetUp() override {
    // Set up the fake HID service.
    auto mock_hid_service = std::make_unique<MockHidService>();
    mock_hid_service_ = mock_hid_service.get();
    mock_hid_service_->FirstEnumerationComplete();

    // Transfer the ownership of the |mock_hid_service| to HidManagerImpl.
    // It is safe to use |mock_hid_service_| in this test.
    HidManagerImpl::SetHidServiceForTesting(std::move(mock_hid_service));

    // Initialize the device service and pass a HidManager binder to
    // GamepadService.
    DeviceServiceTestBase::SetUp();
    GamepadService::GetInstance()->StartUp(
        base::BindRepeating(&BindHidManager, device_service(),
                            base::SequencedTaskRunner::GetCurrentDefault()));

    // Create the data fetcher and polling thread.
    auto fetcher = std::make_unique<NintendoDataFetcher>();
    fetcher_ = fetcher.get();
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr, std::move(fetcher),
        std::move(polling_thread));

    RunUntilIdle();
  }

  void TearDown() override {
    HidManagerImpl::SetHidServiceForTesting(nullptr);
    GamepadService::SetInstance(nullptr);
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    polling_thread_->FlushForTesting();
  }

  raw_ptr<MockHidService> mock_hid_service_;
  std::unique_ptr<GamepadProvider> provider_;
  raw_ptr<NintendoDataFetcher> fetcher_;
  raw_ptr<base::Thread> polling_thread_;
};

TEST_F(NintendoDataFetcherTest, UnsupportedDeviceIsIgnored) {
  // Simulate an unsupported, non-Nintendo HID device.
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(0, 0);
  auto device_info = base::MakeRefCounted<HidDeviceInfo>(
      kTestDeviceId, kPhysicalDeviceId, "interface id", /*vendor_id=*/0x1234,
      /*product_id=*/0xabcd, "Invalipad", /*serial_number=*/"",
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection),
      /*max_input_report_size=*/0, /*max_output_report_size=*/0,
      /*max_feature_report_size=*/0);

  // Add the device to the mock HID service. The HID service should notify the
  // data fetcher.
  mock_hid_service_->AddDevice(device_info);
  RunUntilIdle();

  // The device should not have been added to the internal device map.
  EXPECT_TRUE(fetcher_->GetControllersForTesting().empty());

  // Remove the device.
  mock_hid_service_->RemoveDevice(kTestDeviceId);
  RunUntilIdle();
}

TEST_F(NintendoDataFetcherTest, IgnoreInvalidNintendoDevice) {
  // Simulate an invalid, non-Nintendo HID device, which reports an invalid
  // `max_output_report_size` of 0.
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(0, 0);
  auto device_info = base::MakeRefCounted<HidDeviceInfo>(
      kTestDeviceId, kPhysicalDeviceId, "interface id", /*vendor_id=*/0x057e,
      /*product_id=*/0x2009, "Switch Pro Controller", /*serial_number=*/"",
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection),
      /*max_input_report_size=*/0, /*max_output_report_size=*/0,
      /*max_feature_report_size=*/0);

  // Add the device to the mock HID service. The HID service should notify the
  // data fetcher.
  mock_hid_service_->AddDevice(device_info);
  RunUntilIdle();

  // The device should not have been added to the internal device map.
  EXPECT_TRUE(fetcher_->GetControllersForTesting().empty());

  // Remove the device.
  mock_hid_service_->RemoveDevice(kTestDeviceId);
  RunUntilIdle();
}

TEST_F(NintendoDataFetcherTest, AddAndRemoveSwitchPro) {
  // Simulate a Switch Pro over USB.
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(0, 0);
  auto device_info = base::MakeRefCounted<HidDeviceInfo>(
      kTestDeviceId, kPhysicalDeviceId, "interface id", /*vendor_id=*/0x057e,
      /*product_id=*/0x2009, "Switch Pro Controller", /*serial_number=*/"",
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection),
      /*max_input_report_size=*/0, /*max_output_report_size=*/63,
      /*max_feature_report_size=*/0);

  // Add the device to the mock HID service. The HID service should notify the
  // data fetcher.
  mock_hid_service_->AddDevice(device_info);
  RunUntilIdle();

  // The fetcher should have added the device to its internal device map.
  EXPECT_EQ(fetcher_->GetControllersForTesting().size(), 1U);

  // Remove the device.
  mock_hid_service_->RemoveDevice(kTestDeviceId);

  RunUntilIdle();

  // Check that the device was removed.
  EXPECT_TRUE(fetcher_->GetControllersForTesting().empty());
}

}  // namespace device
