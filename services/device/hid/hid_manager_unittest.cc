// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/hid/mock_hid_connection.h"
#include "services/device/hid/mock_hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

namespace {

#if defined(OS_MAC)
const uint64_t kTestDeviceIds[] = {0, 1, 2};
#elif defined(OS_WIN)
const wchar_t* const kTestDeviceIds[] = {L"0", L"1", L"2"};
#else
const char* const kTestDeviceIds[] = {"0", "1", "2"};
#endif

class MockHidManagerClient : public mojom::HidManagerClient {
 public:
  MockHidManagerClient() = default;
  ~MockHidManagerClient() override = default;

  void Bind(mojo::PendingAssociatedReceiver<mojom::HidManagerClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void DeviceAdded(mojom::HidDeviceInfoPtr device_info) override {
    EXPECT_EQ(expect_guid_, device_info->guid);
    std::move(quit_closure_).Run();
  }

  void DeviceRemoved(mojom::HidDeviceInfoPtr device_info) override {
    EXPECT_EQ(expect_guid_, device_info->guid);
    std::move(quit_closure_).Run();
  }

  void SetConnection(mojo::PendingRemote<mojom::HidConnection> hid_connection) {
    hid_connection_.Bind(std::move(hid_connection));
  }

  mojom::HidConnection* GetConnection() { return hid_connection_.get(); }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void SetExpectGUID(std::string guid) { expect_guid_ = guid; }

 private:
  mojo::AssociatedReceiver<mojom::HidManagerClient> receiver_{this};
  mojo::Remote<mojom::HidConnection> hid_connection_;
  base::OnceClosure quit_closure_;
  std::string expect_guid_;

  DISALLOW_COPY_AND_ASSIGN(MockHidManagerClient);
};

void OnGetDevices(base::OnceClosure quit_closure,
                  size_t expect,
                  std::vector<mojom::HidDeviceInfoPtr> devices) {
  EXPECT_EQ(expect, devices.size());

  std::move(quit_closure).Run();
}

void OnConnect(base::OnceClosure quit_closure,
               MockHidManagerClient* client,
               mojo::PendingRemote<mojom::HidConnection> connection) {
  DCHECK(client);
  DCHECK(connection);
  client->SetConnection(std::move(connection));

  std::move(quit_closure).Run();
}

void OnRead(base::OnceClosure quit_closure,
            bool success,
            uint8_t report_id,
            const base::Optional<std::vector<uint8_t>>& buffer) {
  EXPECT_TRUE(success);

  DCHECK(buffer);
  const char expected[] = "TestRead";
  EXPECT_EQ(report_id, 1);
  EXPECT_EQ(memcmp(buffer->data(), expected, sizeof(expected) - 1), 0);
  EXPECT_EQ(buffer->size(), sizeof(expected) - 1);

  std::move(quit_closure).Run();
}

void OnWrite(base::OnceClosure quit_closure, bool success) {
  EXPECT_TRUE(success);

  std::move(quit_closure).Run();
}

void OnGetFeatureReport(base::OnceClosure quit_closure,
                        bool success,
                        const base::Optional<std::vector<uint8_t>>& buffer) {
  EXPECT_TRUE(success);

  DCHECK(buffer);
  const char expected[] = "TestGetFeatureReport";
  EXPECT_EQ(memcmp(buffer->data(), expected, sizeof(expected) - 1), 0);
  EXPECT_EQ(buffer->size(), sizeof(expected) - 1);

  std::move(quit_closure).Run();
}

class HidManagerTest : public DeviceServiceTestBase {
 public:
  HidManagerTest() {}

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
        kTestDeviceIds[2], "physical id 2", /*vendor_id=*/0, /*product_id=*/0,
        "Hid Service Unit Test", "HidDevice-2",
        mojom::HidBusType::kHIDBusTypeUSB, std::move(collection_info),
        /*max_input_report_size=*/64, /*max_output_report_size=*/64,
        /*max_feature_report_size=*/64);
    mock_hid_service_->AddDevice(device_info);
    return device_info;
  }

  mojo::Remote<mojom::HidManager> hid_manager_;
  MockHidService* mock_hid_service_;

  DISALLOW_COPY_AND_ASSIGN(HidManagerTest);
};

// Test the GetDevices.
TEST_F(HidManagerTest, GetDevicesOnly) {
  // Add two hid devices.
  AddTestDevice0();
  AddTestDevice1();
  mock_hid_service_->FirstEnumerationComplete();

  // Expect two devices will be received in OnGetDevices().
  base::RunLoop run_loop;
  hid_manager_->GetDevices(
      base::BindOnce(&OnGetDevices, run_loop.QuitClosure(), 2));
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

  // Call GetDevicesAndSetClient, expect 1 device will be received in
  // OnGetDevices().
  {
    base::RunLoop run_loop;
    hid_manager_->GetDevicesAndSetClient(
        std::move(hid_manager_client),
        base::BindOnce(&OnGetDevices, run_loop.QuitClosure(), 1));
    run_loop.Run();
  }

  // Add another hid device, expect MockHidManagerClient::DeviceAdded() will be
  // called, and the guid should be same as expected.
  auto device1 = AddTestDevice1();
  {
    base::RunLoop run_loop;
    client->SetQuitClosure(run_loop.QuitClosure());
    client->SetExpectGUID(device1->device_guid());
    run_loop.Run();
  }

  // Remove one hid device, expect MockHidManagerClient::DeviceRemoved() will be
  // called, and the guid should be same as expected.
  mock_hid_service_->RemoveDevice(kTestDeviceIds[0]);
  {
    base::RunLoop run_loop;
    client->SetQuitClosure(run_loop.QuitClosure());
    client->SetExpectGUID(device0->device_guid());
    run_loop.Run();
  }
}

// Test the Connect and the mojom::HidConnection interface.
TEST_F(HidManagerTest, TestHidConnectionInterface) {
  // Add a hid device with a top-level collection.
  auto device = AddTestDeviceWithTopLevelCollection();
  mock_hid_service_->FirstEnumerationComplete();

  auto client = std::make_unique<MockHidManagerClient>();
  mojo::PendingAssociatedRemote<mojom::HidManagerClient> hid_manager_client;
  client->Bind(hid_manager_client.InitWithNewEndpointAndPassReceiver());

  // Call GetDevicesAndSetClient, expect 1 device will be received in
  // OnGetDevices().
  {
    base::RunLoop run_loop;
    hid_manager_->GetDevicesAndSetClient(
        std::move(hid_manager_client),
        base::BindOnce(&OnGetDevices, run_loop.QuitClosure(), 1));
    run_loop.Run();
  }

  // Connect and save the HidConnection InterfacePtr into MockHidManagerClient.
  {
    base::RunLoop run_loop;
    hid_manager_->Connect(
        device->device_guid(),
        /*connection_client=*/mojo::NullRemote(),
        /*watcher=*/mojo::NullRemote(),
        base::BindOnce(&OnConnect, run_loop.QuitClosure(), client.get()));
    run_loop.Run();
  }

  // Test mojom::HidConnection::Read().
  {
    base::RunLoop run_loop;
    client->GetConnection()->Read(
        base::BindOnce(&OnRead, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Test mojom::HidConnection::Write().
  {
    base::RunLoop run_loop;
    client->GetConnection()->Write(
        0, /* report_id */
        {}, base::BindOnce(&OnWrite, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Test mojom::HidConnection::GetFeatureReport().
  {
    base::RunLoop run_loop;
    client->GetConnection()->GetFeatureReport(
        0, /* report_id*/
        base::BindOnce(&OnGetFeatureReport, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Test mojom::HidConnection::SendFeatureReport().
  {
    base::RunLoop run_loop;
    // The callback of SendFeatureReport() is same as Write().
    client->GetConnection()->SendFeatureReport(
        0, /*report_id*/
        {}, base::BindOnce(&OnWrite, run_loop.QuitClosure()));
    run_loop.Run();
  }
}

}  //  namespace

}  //  namespace device
