// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_linux.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

namespace device {

namespace {

constexpr uint8_t kMassStorageDeviceClass = 0x08;
// Typical values, but others are possible.
constexpr uint8_t kMassStorageSubclassCode = 0x06;
constexpr uint8_t kMassStorageProtocolCode = 0x50;

// An interface that won't be restricted.
constexpr uint8_t kSafeDeviceClass = 0xff;
constexpr uint8_t kSafeSubclassCode = 0x42;
constexpr uint8_t kSafeProtocolCode = 0x01;

class MockObserver : public UsbService::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD2(OnDeviceAdded, void(scoped_refptr<UsbDevice>, bool));
  MOCK_METHOD2(OnDeviceRemoved, void(scoped_refptr<UsbDevice>, bool));
  MOCK_METHOD2(OnDeviceRemovedCleanup, void(scoped_refptr<UsbDevice>, bool));
  MOCK_METHOD0(WillDestroyUsbService, void());
};

}  // namespace

// Currently this test is only compiled for Ash, as we are only testing
// behaviour specific to Chrome OS.
class UsbServiceLinuxTest : public testing::Test {
 public:
  UsbServiceLinuxTest() {
    chromeos::PermissionBrokerClient::InitializeFake();
    service_ = std::make_unique<UsbServiceLinux>();
    service_->AddObserver(&observer_);
  }

  ~UsbServiceLinuxTest() override {
    service_->RemoveObserver(&observer_);
    chromeos::PermissionBrokerClient::Shutdown();
  }

 protected:
  void RunInitialEnumeration() {
    // UsbServiceLinux uses a blocking TaskRunner.
    task_environment_.RunUntilIdle();
  }

  void AddConfiguration(UsbDeviceDescriptor* descriptor,
                        uint8_t device_class,
                        uint8_t subclass_code,
                        uint8_t protocol_code) {
    auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
    alternate->alternate_setting = 0;
    alternate->class_code = device_class;
    alternate->subclass_code = subclass_code;
    alternate->protocol_code = protocol_code;

    auto interface = device::mojom::UsbInterfaceInfo::New();
    interface->interface_number =
        descriptor->device_info->configurations.size();
    interface->alternates.push_back(std::move(alternate));

    auto config = device::mojom::UsbConfigurationInfo::New();
    config->configuration_value = 1;
    config->interfaces.push_back(std::move(interface));

    descriptor->device_info->configurations.push_back(std::move(config));
  }

  // In production a UdevWatcher is used, but for purposes of testing it's
  // easier to directly call OnDeviceAdded/OnDeviceRemoved.

  void AddDevice(const std::string& device_path,
                 std::unique_ptr<UsbDeviceDescriptor> descriptor) {
    service_->OnDeviceAdded(device_path, std::move(descriptor));
  }

  void RemoveDevice(const std::string& device_path) {
    service_->OnDeviceRemoved(device_path);
  }

  UsbServiceLinux* service() { return service_.get(); }

  MockObserver& observer() { return observer_; }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::FakeUdevLoader fake_udev_loader_;
  testing::StrictMock<MockObserver> observer_;

  std::unique_ptr<UsbServiceLinux> service_;
};

TEST_F(UsbServiceLinuxTest, MassStorageDevice) {
  RunInitialEnumeration();

  std::string device_path = "/dev/bus/usb/002/006";
  auto descriptor = std::make_unique<UsbDeviceDescriptor>();
  AddConfiguration(descriptor.get(), kMassStorageDeviceClass,
                   kMassStorageSubclassCode, kMassStorageProtocolCode);

  base::RunLoop run_loop_1;
  EXPECT_CALL(observer(), OnDeviceAdded(_, /*is_restricted_device=*/true))
      .WillOnce(base::test::RunOnceClosure(run_loop_1.QuitClosure()));
  AddDevice(device_path, std::move(descriptor));
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  service()->GetDevices(
      /*allow_restricted_devices=*/false,
      base::BindLambdaForTesting(
          [&](const std::vector<scoped_refptr<UsbDevice>>& devices) {
            EXPECT_THAT(devices, IsEmpty());
            run_loop_2.Quit();
          }));
  run_loop_2.Run();

  base::RunLoop run_loop_3;
  service()->GetDevices(
      /*allow_restricted_devices=*/true,
      base::BindLambdaForTesting(
          [&](const std::vector<scoped_refptr<UsbDevice>>& devices) {
            EXPECT_EQ(devices.size(), 1u);
            run_loop_3.Quit();
          }));
  run_loop_3.Run();

  base::RunLoop run_loop_4;
  EXPECT_CALL(observer(), OnDeviceRemoved(_, /*is_restricted_device=*/true));
  EXPECT_CALL(observer(),
              OnDeviceRemovedCleanup(_, /*is_restricted_device=*/true))
      .WillOnce(base::test::RunOnceClosure(run_loop_4.QuitClosure()));
  RemoveDevice(device_path);
  run_loop_4.Run();
}

TEST_F(UsbServiceLinuxTest, CompositeMassStorageDevice) {
  RunInitialEnumeration();

  std::string device_path = "/dev/bus/usb/002/006";
  auto descriptor = std::make_unique<UsbDeviceDescriptor>();
  AddConfiguration(descriptor.get(), 0xff, 0x00, 0x00);
  AddConfiguration(descriptor.get(), 0xff, 0xff, 0xff);
  AddConfiguration(descriptor.get(), kMassStorageDeviceClass,
                   kMassStorageSubclassCode, kMassStorageProtocolCode);
  AddConfiguration(descriptor.get(), kSafeDeviceClass, kSafeSubclassCode,
                   kSafeProtocolCode);

  // A device with a mass storage and a different interface is not considered
  // restricted.
  base::RunLoop run_loop_1;
  EXPECT_CALL(observer(), OnDeviceAdded(_, /*is_restricted_device=*/false))
      .WillOnce(base::test::RunOnceClosure(run_loop_1.QuitClosure()));
  AddDevice(device_path, std::move(descriptor));
  run_loop_1.Run();

  scoped_refptr<UsbDevice> device;
  base::RunLoop run_loop_2;
  service()->GetDevices(
      /*allow_restricted_devices=*/false,
      base::BindLambdaForTesting(
          [&](const std::vector<scoped_refptr<UsbDevice>>& devices) {
            EXPECT_EQ(devices.size(), 1u);
            device = devices[0];
            run_loop_2.Quit();
          }));
  run_loop_2.Run();

  // The device should open successfully, but the call to the permission broker
  // should disallow the mass storage interface.
  device->Open(
      base::BindLambdaForTesting([&](scoped_refptr<UsbDeviceHandle> handle) {
        EXPECT_NE(handle, nullptr);
      }));

  auto log = chromeos::FakePermissionBrokerClient::Get()
                 ->GetAndResetClaimDevicePathLog();
  EXPECT_THAT(log, ElementsAre(Pair(device_path, 1 | 2 | 8)));

  base::RunLoop run_loop_3;
  EXPECT_CALL(observer(), OnDeviceRemoved(_, /*is_restricted_device=*/false));
  EXPECT_CALL(observer(),
              OnDeviceRemovedCleanup(_, /*is_restricted_device=*/false))
      .WillOnce(base::test::RunOnceClosure(run_loop_3.QuitClosure()));
  RemoveDevice(device_path);
  run_loop_3.Run();
}

TEST_F(UsbServiceLinuxTest, InitialEnumerationWithMassStorageDevice) {
  std::string device_mass_storage = "/dev/bus/usb/001/001";
  auto descriptor_mass_storage = std::make_unique<UsbDeviceDescriptor>();
  AddConfiguration(descriptor_mass_storage.get(), kMassStorageDeviceClass,
                   kMassStorageSubclassCode, kMassStorageProtocolCode);
  descriptor_mass_storage->device_info->product_id = 0x1111;

  std::string device_safe = "/dev/bus/usb/001/002";
  auto descriptor_safe = std::make_unique<UsbDeviceDescriptor>();
  AddConfiguration(descriptor_safe.get(), kSafeDeviceClass, kSafeSubclassCode,
                   kSafeProtocolCode);
  descriptor_safe->device_info->product_id = 0x9876;

  std::vector<scoped_refptr<UsbDevice>> devices_with_restricted;
  std::vector<scoped_refptr<UsbDevice>> devices_without_restricted;

  service()->GetDevices(
      /*allow_restricted_devices=*/false,
      base::BindLambdaForTesting(
          [&](const std::vector<scoped_refptr<UsbDevice>>& devices) {
            devices_without_restricted = devices;
          }));

  service()->GetDevices(
      /*allow_restricted_devices=*/true,
      base::BindLambdaForTesting(
          [&](const std::vector<scoped_refptr<UsbDevice>>& devices) {
            devices_with_restricted = devices;
          }));

  // By adding these devices prior to running the blocking TaskRunner, they
  // will be included in the initial enumeration, so OnDeviceAdded() is not
  // called.
  AddDevice(device_mass_storage, std::move(descriptor_mass_storage));
  AddDevice(device_safe, std::move(descriptor_safe));

  RunInitialEnumeration();

  EXPECT_EQ(devices_without_restricted.size(), 1u);
  EXPECT_EQ(devices_without_restricted[0]->product_id(), 0x9876);
  EXPECT_EQ(devices_with_restricted.size(), 2u);
}

}  // namespace device
