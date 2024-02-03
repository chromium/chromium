// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/public/cpp/test/mock_hid_connection.h"
#include "services/device/public/cpp/test/mock_hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::ElementsAreArray;

#if BUILDFLAG(IS_MAC)
const uint64_t kTestDeviceIds[] = {0, 1, 2, 3};
#elif BUILDFLAG(IS_WIN)
const wchar_t* const kTestDeviceIds[] = {L"0", L"1", L"2", L"3"};
#else
const char* const kTestDeviceIds[] = {"0", "1", "2", "3"};
#endif

class MockHidManagerClient : public mojom::HidManagerClient {
 public:
  MockHidManagerClient() = default;
  MockHidManagerClient(const MockHidManagerClient&) = delete;
  MockHidManagerClient& operator=(const MockHidManagerClient&) = delete;
  ~MockHidManagerClient() override = default;

  void Bind(mojo::PendingAssociatedReceiver<mojom::HidManagerClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD1(DeviceAdded, void(mojom::HidDeviceInfoPtr device_info));
  MOCK_METHOD1(DeviceRemoved, void(mojom::HidDeviceInfoPtr device_info));
  MOCK_METHOD1(DeviceChanged, void(mojom::HidDeviceInfoPtr device_info));

  void SetConnection(mojo::PendingRemote<mojom::HidConnection> connection) {
    hid_connection_.Bind(std::move(connection));
  }

  mojom::HidConnection* GetConnection() { return hid_connection_.get(); }

 private:
  mojo::AssociatedReceiver<mojom::HidManagerClient> receiver_{this};
  mojo::Remote<mojom::HidConnection> hid_connection_;
};

class HidManagerTest : public DeviceServiceTestBase {
 public:
  HidManagerTest() = default;
  HidManagerTest(const HidManagerTest&) = delete;
  HidManagerTest& operator=(const HidManagerTest&) = delete;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    auto mock_hid_service = std::make_unique<MockHidService>();
    mock_hid_service_ = mock_hid_service.get();
    // Transfer the ownership of the |mock_hid_service| to HidManagerImpl.
    // It is safe to use the |mock_hid_service_| in this test.
    HidManagerImpl::SetHidServiceForTesting(std::move(mock_hid_service));
    device_service()->BindHidManager(hid_manager_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { HidManagerImpl::SetHidServiceForTesting(nullptr); }

  scoped_refptr<HidDeviceInfo> AddTestDevice0() {
    // Construct a minimal HidDeviceInfo.
    auto device_info = base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceIds[0], "physical id 0", /*vendor_id=*/0, /*product_id=*/0,
        "Hid Service Unit Test", "HidDevice-0",
        mojom::HidBusType::kHIDBusTypeUSB,
        /*report_descriptor=*/std::vector<uint8_t>());
    mock_hid_service_->AddDevice(device_info);
    return device_info;
  }

  scoped_refptr<HidDeviceInfo> AddTestDevice1() {
    // Construct a minimal HidDeviceInfo with a different device ID than above.
    auto device_info = base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceIds[1], "physical id 1", /*vendor_id=*/0, /*product_id=*/0,
        "Hid Service Unit Test", "HidDevice-1",
        mojom::HidBusType::kHIDBusTypeUSB,
        /*report_descriptor=*/std::vector<uint8_t>());
    mock_hid_service_->AddDevice(device_info);
    return device_info;
  }

  scoped_refptr<HidDeviceInfo> AddTestDeviceWithTopLevelCollection() {
    // Construct a HidDeviceInfo with a top-level collection. The collection has
    // a usage ID from the FIDO usage page.
    auto collection_info = mojom::HidCollectionInfo::New();
    collection_info->usage = mojom::HidUsageAndPage::New(1, 0xf1d0);
    auto device_info = base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceIds[2], "physical id 2", "interface id 2", /*vendor_id=*/0,
        /*product_id=*/0, "Hid Service Unit Test", "HidDevice-2",
        mojom::HidBusType::kHIDBusTypeUSB, std::move(collection_info),
        /*max_input_report_size=*/64, /*max_output_report_size=*/64,
        /*max_feature_report_size=*/64);
    mock_hid_service_->AddDevice(device_info);
    return device_info;
  }

  void UpdateTestDeviceWithNewTopLevelCollection() {
    // Construct a device that will update the device added by
    // AddTestDeviceWithTopLevelCollection when added to the HidService.
    // The updated device info should have a new platform device ID and
    // different collection info but the physical device ID and interface ID
    // should be the same.
    auto collection_info = mojom::HidCollectionInfo::New();
    collection_info->usage = mojom::HidUsageAndPage::New(1, 0xff00);
    auto device_info = base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceIds[3], "physical id 2", "interface id 2", /*vendor_id=*/0,
        /*product_id=*/0, "Hid Service Unit Test", "HidDevice-2",
        mojom::HidBusType::kHIDBusTypeUSB, std::move(collection_info),
        /*max_input_report_size=*/128, /*max_output_report_size=*/128,
        /*max_feature_report_size=*/0);
    mock_hid_service_->AddDevice(device_info);
  }

  mojo::Remote<mojom::HidManager> hid_manager_;
  raw_ptr<MockHidService> mock_hid_service_;
};

// Test the GetDevices.
TEST_F(HidManagerTest, GetDevicesOnly) {
  // Add two hid devices.
  AddTestDevice0();
  AddTestDevice1();
  mock_hid_service_->FirstEnumerationComplete();

  // Expect two devices will be received.
  base::RunLoop run_loop;
  hid_manager_->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
        EXPECT_EQ(devices.size(), 2u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Test the GetDevicesAndSetClient and the mojom::HidManagerClient
// interface.
TEST_F(HidManagerTest, GetDevicesAndSetClient) {
  // Add one hid device.
  auto device0 = AddTestDevice0();
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect 1 device will be received.
  {
    base::RunLoop run_loop;
    hid_manager_->GetDevicesAndSetClient(
        std::move(hid_manager_client),
        base::BindLambdaForTesting(
            [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
              EXPECT_EQ(devices.size(), 1u);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Add another hid device, expect MockHidManagerClient::DeviceAdded() will be
  // called, and the guid should be same as expected.
  {
    scoped_refptr<HidDeviceInfo> device1;
    base::RunLoop run_loop;
    EXPECT_CALL(*client, DeviceAdded).WillOnce([&](auto d) {
      ASSERT_TRUE(device1);
      EXPECT_EQ(d->guid, device1->device_guid());
      run_loop.Quit();
    });
    device1 = AddTestDevice1();
    run_loop.Run();
  }

  // Remove one hid device, expect MockHidManagerClient::DeviceRemoved() will be
  // called, and the guid should be same as expected.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*client, DeviceRemoved).WillOnce([&](auto d) {
      EXPECT_EQ(d->guid, device0->device_guid());
      run_loop.Quit();
    });
    mock_hid_service_->RemoveDevice(kTestDeviceIds[0]);
    run_loop.Run();
  }
}

TEST_F(HidManagerTest, DeviceChangedBeforeFirstEnumeration) {
  // Add a hid device representing a single top-level collection, then update it
  // with a second top-level collection before the first enumeration is
  // complete.
  auto device = AddTestDeviceWithTopLevelCollection();
  UpdateTestDeviceWithNewTopLevelCollection();
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect 1 device will be received. The device
  // should include the updated device info.
  base::RunLoop get_devices_loop;
  hid_manager_->GetDevicesAndSetClient(
      std::move(hid_manager_client),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
            EXPECT_EQ(devices.size(), 1u);
            EXPECT_EQ(devices[0]->collections.size(), 2u);
            EXPECT_EQ(devices[0]->max_input_report_size, 128u);
            EXPECT_EQ(devices[0]->max_output_report_size, 128u);
            EXPECT_EQ(devices[0]->max_feature_report_size, 64u);
            get_devices_loop.Quit();
          }));
  get_devices_loop.Run();

  // Remove the hid device, expect MockHidManagerClient::DeviceRemoved()
  // will be called, and the guid should be same as expected.
  base::RunLoop device_removed_loop;
  EXPECT_CALL(*client, DeviceRemoved).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    device_removed_loop.Quit();
  });
  mock_hid_service_->RemoveDevice(kTestDeviceIds[2]);
  device_removed_loop.Run();

  // Call GetDevices, expect no devices.
  base::RunLoop get_devices_loop2;
  hid_manager_->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
        EXPECT_TRUE(devices.empty());
        get_devices_loop2.Quit();
      }));
  get_devices_loop2.Run();
}

TEST_F(HidManagerTest, DeviceChangedAfterFirstEnumeration) {
  // Add a hid device with a single top-level collection.
  auto device = AddTestDeviceWithTopLevelCollection();
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect 1 device will be received.
  base::RunLoop get_devices_loop;
  hid_manager_->GetDevicesAndSetClient(
      std::move(hid_manager_client),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
            EXPECT_EQ(devices.size(), 1u);
            EXPECT_EQ(devices[0]->collections.size(), 1u);
            EXPECT_EQ(devices[0]->max_input_report_size, 64u);
            EXPECT_EQ(devices[0]->max_output_report_size, 64u);
            EXPECT_EQ(devices[0]->max_feature_report_size, 64u);
            get_devices_loop.Quit();
          }));
  get_devices_loop.Run();

  // Add a sibling hid device, expect MockHidManagerClient::DeviceChanged() will
  // be called. Make sure the device info is updated.
  base::RunLoop device_changed_loop;
  EXPECT_CALL(*client, DeviceChanged).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    EXPECT_EQ(d->collections.size(), 2u);
    EXPECT_EQ(d->max_input_report_size, 128u);
    EXPECT_EQ(d->max_output_report_size, 128u);
    EXPECT_EQ(d->max_feature_report_size, 64u);
    device_changed_loop.Quit();
  });
  UpdateTestDeviceWithNewTopLevelCollection();
  device_changed_loop.Run();

  // Remove the hid device, expect MockHidManagerClient::DeviceRemoved()
  // will be called, and the guid should be same as expected.
  base::RunLoop device_removed_loop;
  EXPECT_CALL(*client, DeviceRemoved).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    device_removed_loop.Quit();
  });
  mock_hid_service_->RemoveDevice(kTestDeviceIds[2]);
  device_removed_loop.Run();

  // Call GetDevices, expect no devices.
  base::RunLoop get_devices_loop2;
  hid_manager_->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
        EXPECT_TRUE(devices.empty());
        get_devices_loop2.Quit();
      }));
  get_devices_loop2.Run();
}

TEST_F(HidManagerTest, DeviceAddedAndChangedAfterFirstEnumeration) {
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect no devices.
  base::RunLoop get_devices_loop;
  hid_manager_->GetDevicesAndSetClient(
      std::move(hid_manager_client),
      base::BindLambdaForTesting(
          [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
            EXPECT_TRUE(devices.empty());
            get_devices_loop.Quit();
          }));
  get_devices_loop.Run();

  // Add a hid device with a single top-level collection.
  scoped_refptr<HidDeviceInfo> device;
  base::RunLoop device_added_loop;
  EXPECT_CALL(*client, DeviceAdded).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    EXPECT_EQ(d->collections.size(), 1u);
    EXPECT_EQ(d->max_input_report_size, 64u);
    EXPECT_EQ(d->max_output_report_size, 64u);
    EXPECT_EQ(d->max_feature_report_size, 64u);
    device_added_loop.Quit();
  });
  device = AddTestDeviceWithTopLevelCollection();
  device_added_loop.Run();

  // Update the device, expect MockHidManagerClient::DeviceChanged() will be
  // called. Make sure the collections and max report sizes are updated.
  base::RunLoop device_changed_loop;
  EXPECT_CALL(*client, DeviceChanged).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    EXPECT_EQ(d->collections.size(), 2u);
    EXPECT_EQ(d->max_input_report_size, 128u);
    EXPECT_EQ(d->max_output_report_size, 128u);
    EXPECT_EQ(d->max_feature_report_size, 64u);
    device_changed_loop.Quit();
  });
  UpdateTestDeviceWithNewTopLevelCollection();
  device_changed_loop.Run();

  // Remove the device, expect MockHidManagerClient::DeviceRemoved() will be
  // called and the guid should be same as expected.
  base::RunLoop device_removed_loop;
  EXPECT_CALL(*client, DeviceRemoved).WillOnce([&](auto d) {
    EXPECT_EQ(d->guid, device->device_guid());
    device_removed_loop.Quit();
  });
  mock_hid_service_->RemoveDevice(kTestDeviceIds[2]);
  device_removed_loop.Run();

  // Call GetDevices, expect no devices.
  base::RunLoop get_devices_loop2;
  hid_manager_->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
        EXPECT_TRUE(devices.empty());
        get_devices_loop2.Quit();
      }));
  get_devices_loop2.Run();
}

// Test the Connect and the mojom::HidConnection interface.
TEST_F(HidManagerTest, TestHidConnectionInterface) {
  // Add a hid device with a top-level collection.
  auto device = AddTestDeviceWithTopLevelCollection();
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect 1 device will be received.
  {
    base::RunLoop run_loop;
    hid_manager_->GetDevicesAndSetClient(
        std::move(hid_manager_client),
        base::BindLambdaForTesting(
            [&](std::vector<mojom::HidDeviceInfoPtr> devices) {
              EXPECT_EQ(devices.size(), 1u);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Connect and save the HidConnection InterfacePtr into MockHidManagerClient.
  {
    base::RunLoop run_loop;
    hid_manager_->Connect(
        device->device_guid(),
        /*connection_client=*/mojo::NullRemote(),
        /*watcher=*/mojo::NullRemote(),
        /*allow_protected_reports=*/false,
        /*allow_fido_reports=*/false,
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<mojom::HidConnection> connection) {
              EXPECT_TRUE(connection);
              client->SetConnection(std::move(connection));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Test mojom::HidConnection::Read().
  {
    base::RunLoop run_loop;
    client->GetConnection()->Read(base::BindLambdaForTesting(
        [&](bool success, uint8_t report_id,
            const std::optional<std::vector<uint8_t>>& buffer) {
          constexpr std::string_view kExpected = "TestRead";
          EXPECT_TRUE(success);
          EXPECT_EQ(report_id, 1u);
          ASSERT_TRUE(buffer);
          EXPECT_THAT(*buffer, ElementsAreArray(kExpected));
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Test mojom::HidConnection::Write().
  {
    base::RunLoop run_loop;
    client->GetConnection()->Write(
        /*report_id=*/0,
        /*buffer=*/{}, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Test mojom::HidConnection::GetFeatureReport().
  {
    base::RunLoop run_loop;
    client->GetConnection()->GetFeatureReport(
        /*report_id=*/0,
        base::BindLambdaForTesting(
            [&](bool success,
                const std::optional<std::vector<uint8_t>>& buffer) {
              constexpr std::string_view kExpected = "TestGetFeatureReport";
              EXPECT_TRUE(success);
              ASSERT_TRUE(buffer);
              EXPECT_THAT(*buffer, ElementsAreArray(kExpected));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Test mojom::HidConnection::SendFeatureReport().
  {
    base::RunLoop run_loop;
    client->GetConnection()->SendFeatureReport(
        /*report_id=*/0,
        /*buffer=*/{}, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  //  namespace

}  //  namespace device
