// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/nintendo_data_fetcher.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_service.h"
#include "services/device/device_service_test_base.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/hid/mock_hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

#if defined(OS_MACOSX)
const uint64_t kTestDeviceId = 123;
#else
const char* kTestDeviceId = "123";
#endif

}  // namespace

// Main test fixture
class NintendoDataFetcherTest : public DeviceServiceTestBase {
 public:
  NintendoDataFetcherTest() {}

  void SetUp() override {
    // Set up the fake HID service.
    auto mock_hid_service = std::make_unique<MockHidService>();
    mock_hid_service_ = mock_hid_service.get();
    mock_hid_service_->FirstEnumerationComplete();

    // Transfer the ownership of the |mock_hid_service| to HidManagerImpl.
    // It is safe to use |mock_hid_service_| in this test.
    HidManagerImpl::SetHidServiceForTesting(std::move(mock_hid_service));

    // Initialize the device service and pass a service connector to the gamepad
    // service.
    DeviceServiceTestBase::SetUp();
    GamepadService::GetInstance()->StartUp(connector()->Clone());

    // Create the data fetcher and polling thread.
    auto fetcher = std::make_unique<NintendoDataFetcher>();
    fetcher_ = fetcher.get();
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr, connector()->Clone(),
        std::move(fetcher), std::move(polling_thread));

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

  MockHidService* mock_hid_service_;
  std::unique_ptr<GamepadProvider> provider_;
  NintendoDataFetcher* fetcher_;
  base::Thread* polling_thread_;

  DISALLOW_COPY_AND_ASSIGN(NintendoDataFetcherTest);
};

TEST_F(NintendoDataFetcherTest, UnsupportedDeviceIsIgnored) {
  // Simulate an unsupported, non-Nintendo HID device.
  auto collection = mojom::HidCollectionInfo::New();
  collection->usage = mojom::HidUsageAndPage::New(0, 0);
  scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
      kTestDeviceId, 0x1234, 0xabcd, "Invalipad", "",
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection), 0, 0, 0));

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
  scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
      kTestDeviceId, 0x057e, 0x2009, "Switch Pro Controller", "",
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection), 0, 63, 0));

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
