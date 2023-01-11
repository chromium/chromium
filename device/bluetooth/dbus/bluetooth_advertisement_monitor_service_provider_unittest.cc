// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {
class FakeBluetoothAdvertisementMonitorServiceProviderDelegate
    : public BluetoothAdvertisementMonitorServiceProvider::Delegate {
 public:
  FakeBluetoothAdvertisementMonitorServiceProviderDelegate() = default;

  // BluetoothAdvertisementMonitorServiceProvider::Delegate
  void OnActivate() override { activate_count_++; }
  void OnRelease() override { release_count_++; }
  void OnDeviceFound(const dbus::ObjectPath& device_path) override {
    devices_found_.push_back(device_path);
  }
  void OnDeviceLost(const dbus::ObjectPath& device_path) override {
    devices_lost_.push_back(device_path);
  }

  size_t activate_count() const { return activate_count_; }

  size_t release_count() const { return release_count_; }

  std::vector<dbus::ObjectPath> devices_found() const { return devices_found_; }

  std::vector<dbus::ObjectPath> devices_lost() const { return devices_lost_; }

  base::WeakPtr<FakeBluetoothAdvertisementMonitorServiceProviderDelegate>
  GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  size_t activate_count_ = 0;
  size_t release_count_ = 0;
  std::vector<dbus::ObjectPath> devices_found_;
  std::vector<dbus::ObjectPath> devices_lost_;

  base::WeakPtrFactory<FakeBluetoothAdvertisementMonitorServiceProviderDelegate>
      weak_ptr_factory_{this};
};

void SetUpMocksDbus(dbus::MockBus* mock_bus,
                    dbus::MockExportedObject* mock_exported_object) {
  EXPECT_CALL(*mock_bus, GetExportedObject(dbus::ObjectPath("/path")))
      .WillOnce(testing::Return(mock_exported_object));
  EXPECT_CALL(*mock_exported_object,
              ExportMethod(bluetooth_advertisement_monitor::
                               kBluetoothAdvertisementMonitorInterface,
                           bluetooth_advertisement_monitor::kDeviceLost,
                           testing::_, testing::_));
  EXPECT_CALL(*mock_exported_object,
              ExportMethod(bluetooth_advertisement_monitor::
                               kBluetoothAdvertisementMonitorInterface,
                           bluetooth_advertisement_monitor::kDeviceFound,
                           testing::_, testing::_));
  EXPECT_CALL(*mock_exported_object,
              ExportMethod(bluetooth_advertisement_monitor::
                               kBluetoothAdvertisementMonitorInterface,
                           bluetooth_advertisement_monitor::kActivate,
                           testing::_, testing::_));
  EXPECT_CALL(*mock_exported_object,
              ExportMethod(bluetooth_advertisement_monitor::
                               kBluetoothAdvertisementMonitorInterface,
                           bluetooth_advertisement_monitor::kRelease,
                           testing::_, testing::_));
}
}  // namespace

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, Activate) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");
  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.Activate(method_call.get(), base::DoNothing());

  EXPECT_EQ(1u, delegate.activate_count());
}

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, Release) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");
  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.Release(method_call.get(), base::DoNothing());

  EXPECT_EQ(1u, delegate.release_count());
}

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, DeviceFound) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");

  dbus::MessageWriter writer(method_call.get());
  auto device_path = dbus::ObjectPath("/device/path");
  writer.AppendObjectPath(device_path);

  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.DeviceFound(method_call.get(), base::DoNothing());

  EXPECT_EQ(1u, delegate.devices_found().size());
  EXPECT_EQ(device_path.value(), delegate.devices_found()[0].value());
}

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, DeviceFoundFailure) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");

  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.DeviceFound(method_call.get(), base::DoNothing());

  EXPECT_EQ(0u, delegate.devices_found().size());
}

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, DeviceLost) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");

  dbus::MessageWriter writer(method_call.get());
  auto device_path = dbus::ObjectPath("/device/path");
  writer.AppendObjectPath(device_path);

  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.DeviceLost(method_call.get(), base::DoNothing());

  EXPECT_EQ(1u, delegate.devices_lost().size());
  EXPECT_EQ(device_path.value(), delegate.devices_lost()[0].value());
}

TEST(BluetoothAdvertisementMonitorServiceProviderImplTest, DeviceLostFailure) {
  auto method_call =
      std::make_unique<dbus::MethodCall>("com.example.Interface", "SomeMethod");

  FakeBluetoothAdvertisementMonitorServiceProviderDelegate delegate;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(), object_path);

  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/object_path,
      /*filter=*/nullptr,
      /*delegate=*/delegate.GetWeakPtr());

  provider_impl.DeviceLost(method_call.get(), base::DoNothing());

  EXPECT_EQ(0u, delegate.devices_lost().size());
}

}  // namespace bluez
