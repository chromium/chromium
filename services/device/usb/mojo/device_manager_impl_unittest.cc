// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "services/device/usb/mock_usb_device.h"
#include "services/device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace device {

using mojom::UsbDeviceInfoPtr;
using mojom::UsbDeviceManager;
using mojom::UsbDeviceManagerClient;
using mojom::UsbEnumerationOptionsPtr;

namespace usb {

namespace {

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
}

class USBDeviceManagerImplTest : public testing::Test {
 public:
  USBDeviceManagerImplTest() {
    auto mock_usb_service = std::make_unique<MockUsbService>();
    mock_usb_service_ = mock_usb_service.get();
    device_manager_instance_ =
        std::make_unique<DeviceManagerImpl>(std::move(mock_usb_service));
  }
  ~USBDeviceManagerImplTest() override = default;

 protected:
  raw_ptr<MockUsbService, DanglingUntriaged> mock_usb_service_;
  std::unique_ptr<DeviceManagerImpl> device_manager_instance_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class MockDeviceManagerClient : public mojom::UsbDeviceManagerClient {
 public:
  MockDeviceManagerClient() = default;
  ~MockDeviceManagerClient() override = default;

  mojo::PendingAssociatedRemote<mojom::UsbDeviceManagerClient>
  CreateRemoteAndBind() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  MOCK_METHOD1(DoOnDeviceAdded, void(mojom::UsbDeviceInfo*));
  void OnDeviceAdded(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceAdded(device_info.get());
  }

  MOCK_METHOD1(DoOnDeviceRemoved, void(mojom::UsbDeviceInfo*));
  void OnDeviceRemoved(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceRemoved(device_info.get());
  }

 private:
  mojo::AssociatedReceiver<mojom::UsbDeviceManagerClient> receiver_{this};
};

void ExpectDevicesAndThen(const std::set<std::string>& expected_guids,
                          base::OnceClosure continuation,
                          std::vector<UsbDeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (const auto& result : results) {
    actual_guids.insert(result->guid);
  }
  EXPECT_EQ(expected_guids, actual_guids);
  std::move(continuation).Run();
}

}  // namespace

// Test basic GetDevices functionality to ensure that all mock devices are
// returned by the service.
TEST_F(USBDeviceManagerImplTest, GetDevices) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");

  mock_usb_service_->AddDevice(device0);
  mock_usb_service_->AddDevice(device1);
  mock_usb_service_->AddDevice(device2);

  mojo::Remote<UsbDeviceManager> device_manager;
  device_manager_instance_->AddReceiver(
      device_manager.BindNewPipeAndPassReceiver());

  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1234;
  UsbEnumerationOptionsPtr options = mojom::UsbEnumerationOptions::New();
  options->filters.push_back(std::move(filter));

  std::set<std::string> guids;
  guids.insert(device0->guid());
  guids.insert(device1->guid());
  guids.insert(device2->guid());

  base::RunLoop loop;
  device_manager->GetDevices(
      std::move(options),
      base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
  loop.Run();
}

// Test requesting a single Device by GUID.
TEST_F(USBDeviceManagerImplTest, GetDevice) {
  scoped_refptr<MockUsbDevice> mock_device =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  mock_usb_service_->AddDevice(mock_device);

  mojo::Remote<UsbDeviceManager> device_manager;
  device_manager_instance_->AddReceiver(
      device_manager.BindNewPipeAndPassReceiver());

  {
    base::RunLoop loop;
    mojo::Remote<mojom::UsbDevice> device;
    device_manager->GetDevice(mock_device->guid(),
                              /*blocked_interface_classes=*/{},
                              device.BindNewPipeAndPassReceiver(),
                              /*device_client=*/mojo::NullRemote());
    // Close is a no-op if the device hasn't been opened but ensures that the
    // pipe was successfully connected.
    device->Close(loop.QuitClosure());
    loop.Run();
  }

  mojo::Remote<mojom::UsbDevice> bad_device;
  device_manager->GetDevice("not a real guid", /*blocked_interface_classes=*/{},
                            bad_device.BindNewPipeAndPassReceiver(),
                            /*device_client=*/mojo::NullRemote());

  {
    base::RunLoop loop;
    bad_device.set_disconnect_handler(loop.QuitClosure());
    loop.Run();
  }
}

// Test requesting device enumeration updates with GetDeviceChanges.
TEST_F(USBDeviceManagerImplTest, Client) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");
  scoped_refptr<MockUsbDevice> device3 =
      new MockUsbDevice(0x1234, 0x567b, "ACME", "Frobinator Xtreme", "STUVWX");

  mock_usb_service_->AddDevice(device0);

  mojo::Remote<UsbDeviceManager> device_manager;
  device_manager_instance_->AddReceiver(
      device_manager.BindNewPipeAndPassReceiver());

  MockDeviceManagerClient mock_client;
  device_manager->SetClient(mock_client.CreateRemoteAndBind());

  {
    // Call GetDevices once to make sure the device manager is up and running
    // and the client is set or else we could block forever waiting for calls.
    std::set<std::string> guids;
    guids.insert(device0->guid());
    base::RunLoop loop;
    device_manager->GetDevices(
        nullptr,
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  mock_usb_service_->AddDevice(device1);
  mock_usb_service_->AddDevice(device2);
  mock_usb_service_->RemoveDevice(device1);
  mock_usb_service_->RemoveDevice(device0);
  mock_usb_service_->RemoveDevice(device2);
  mock_usb_service_->AddDevice(device3);

  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(/*num_closures=*/6, loop.QuitClosure());
    testing::InSequence s;
    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    EXPECT_CALL(mock_client, DoOnDeviceRemoved(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device0->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device3->guid(), barrier));
    loop.Run();
  }
}

}  // namespace usb
}  // namespace device
