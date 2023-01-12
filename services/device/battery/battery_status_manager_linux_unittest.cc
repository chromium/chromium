// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager_linux.h"

#include <limits>
#include <list>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "services/device/battery/battery_status_manager_linux-inl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Unused;

namespace device {

namespace {
const char kUPowerDeviceACLinePath[] =
    "/org/freedesktop/UPower/devices/line_power_AC";
const char kUPowerDeviceBattery0Path[] =
    "/org/freedesktop/UPower/devices/battery_BAT0";
const char kUPowerDeviceBattery1Path[] =
    "/org/freedesktop/UPower/devices/battery_BAT1";
const char kUPowerDisplayDevicePath[] =
    "/org/freedesktop/UPower/devices/DisplayDevice";

class MockUPowerObject {
 public:
  MockUPowerObject() {}

  MockUPowerObject(const MockUPowerObject&) = delete;
  MockUPowerObject& operator=(const MockUPowerObject&) = delete;

  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback);
  std::unique_ptr<dbus::Response> CreateCallMethodResponse(
      dbus::MethodCall* method_call,
      Unused);
  void SignalDeviceAdded(const std::string& added_device_path);
  void SignalDeviceRemoved(const std::string& removed_device_path);

  scoped_refptr<dbus::MockObjectProxy> proxy;
  dbus::ObjectProxy::SignalCallback signal_callback_device_added;
  dbus::ObjectProxy::SignalCallback signal_callback_device_changed;
  dbus::ObjectProxy::SignalCallback signal_callback_device_removed;
  std::string daemon_version;
  std::list<std::string> devices;
  std::string display_device;
};

void MockUPowerObject::ConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
  bool on_connected_success = true;
  if (interface_name == kUPowerInterfaceName) {
    if (signal_name == kUPowerSignalDeviceAdded)
      signal_callback_device_added = signal_callback;
    else if (signal_name == kUPowerSignalDeviceRemoved)
      signal_callback_device_removed = signal_callback;
    else
      on_connected_success = false;
  } else {
    on_connected_success = false;
  }

  if (!on_connected_success) {
    LOG(WARNING) << "MockUPowerObject::" << __FUNCTION__
                 << " Unexpected interface=" << interface_name
                 << ", signal=" << signal_name;
  }
  std::move(*on_connected_callback)
      .Run(interface_name, signal_name, on_connected_success);
}

std::unique_ptr<dbus::Response> MockUPowerObject::CreateCallMethodResponse(
    dbus::MethodCall* method_call,
    Unused) {
  if (method_call->GetInterface() == kUPowerInterfaceName) {
    if (method_call->GetMember() == kUPowerMethodEnumerateDevices) {
      std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      dbus::MessageWriter array_writer(nullptr);
      writer.OpenArray("o", &array_writer);
      for (const auto& device : devices)
        array_writer.AppendObjectPath(dbus::ObjectPath(device));
      writer.CloseContainer(&array_writer);
      return response;
    } else if (method_call->GetMember() == kUPowerMethodGetDisplayDevice) {
      std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
      if (!display_device.empty()) {
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(display_device));
      }
      return response;
    }
  } else if (method_call->GetInterface() == dbus::kPropertiesInterface) {
    std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    if (method_call->GetMember() == dbus::kPropertiesGet) {
      dbus::MessageReader reader(method_call);
      std::string interface_name;
      std::string property_name;
      if (reader.PopString(&interface_name) &&
          reader.PopString(&property_name) &&
          property_name == kUPowerPropertyDaemonVersion) {
        writer.AppendVariantOfString(daemon_version);
        return response;
      }
    } else if (method_call->GetMember() == dbus::kPropertiesGetAll) {
      dbus::MessageWriter array_writer(nullptr);
      dbus::MessageWriter dict_entry_writer(nullptr);
      writer.OpenArray("{sv}", &array_writer);
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(kUPowerPropertyDaemonVersion);
      dict_entry_writer.AppendVariantOfString(daemon_version);
      array_writer.CloseContainer(&dict_entry_writer);
      writer.CloseContainer(&array_writer);
      return response;
    }
  }

  LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
  return nullptr;
}

void MockUPowerObject::SignalDeviceAdded(const std::string& added_device_path) {
  dbus::Signal signal(kUPowerInterfaceName, kUPowerSignalDeviceAdded);
  signal.SetPath(proxy->object_path());
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(dbus::ObjectPath(added_device_path));
  signal_callback_device_added.Run(&signal);
}

void MockUPowerObject::SignalDeviceRemoved(
    const std::string& removed_device_path) {
  dbus::Signal signal(kUPowerInterfaceName, kUPowerSignalDeviceRemoved);
  signal.SetPath(proxy->object_path());
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(dbus::ObjectPath(removed_device_path));
  signal_callback_device_removed.Run(&signal);
}

struct MockBatteryProperties {
  bool is_present = true;
  double percentage = 100;
  uint32_t state = UPowerDeviceState::UPOWER_DEVICE_STATE_UNKNOWN;
  int64_t time_to_empty = 0;
  int64_t time_to_full = 0;
  uint32_t type = UPowerDeviceType::UPOWER_DEVICE_TYPE_BATTERY;
};

class MockBatteryObject {
 public:
  MockBatteryObject(dbus::Bus* bus,
                    const std::string& object_path,
                    MockBatteryProperties* properties);

  MockBatteryObject(const MockBatteryObject&) = delete;
  MockBatteryObject& operator=(const MockBatteryObject&) = delete;

  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback);
  std::unique_ptr<dbus::Response> CreateCallMethodResponse(
      dbus::MethodCall* method_call,
      Unused);
  MockBatteryObject& ExpectConnectToSignalChanged();
  MockBatteryObject& ExpectConnectToSignalPropertyChanged();
  void SignalChanged();
  void SignalPropertyChanged(const std::string& property_name);

  scoped_refptr<dbus::MockObjectProxy> proxy;
  raw_ptr<MockBatteryProperties> properties;
  dbus::ObjectProxy::SignalCallback signal_callback_changed;
  dbus::ObjectProxy::SignalCallback signal_callback_properties_changed;

 private:
  void AppendPropertyToWriter(dbus::MessageWriter* writer,
                              const std::string& property_name);
  void AppendAllPropertiesToWriter(dbus::MessageWriter* writer);
};

MockBatteryObject::MockBatteryObject(dbus::Bus* bus,
                                     const std::string& object_path,
                                     MockBatteryProperties* properties)
    : proxy(new NiceMock<dbus::MockObjectProxy>(bus,
                                                kUPowerServiceName,
                                                dbus::ObjectPath(object_path))),
      properties(properties) {}

void MockBatteryObject::ConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
  bool on_connected_success = true;
  if (interface_name == kUPowerDeviceInterfaceName &&
      signal_name == kUPowerDeviceSignalChanged)
    signal_callback_changed = signal_callback;
  else if (interface_name == dbus::kPropertiesInterface &&
           signal_name == dbus::kPropertiesChanged)
    signal_callback_properties_changed = signal_callback;
  else
    on_connected_success = false;

  if (!on_connected_success) {
    LOG(WARNING) << "MockBatteryObject::" << __FUNCTION__
                 << " Unexpected interface=" << interface_name
                 << ", signal=" << signal_name;
  }
  std::move(*on_connected_callback)
      .Run(interface_name, signal_name, on_connected_success);
}

std::unique_ptr<dbus::Response> MockBatteryObject::CreateCallMethodResponse(
    dbus::MethodCall* method_call,
    Unused) {
  if (method_call->GetInterface() == dbus::kPropertiesInterface) {
    if (method_call->GetMember() == dbus::kPropertiesGet) {
      if (!properties)
        return nullptr;

      dbus::MessageReader reader(method_call);
      std::string interface_name;
      std::string property_name;
      if (reader.PopString(&interface_name) &&
          reader.PopString(&property_name)) {
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        AppendPropertyToWriter(&writer, property_name);
        return response;
      }
    } else if (method_call->GetMember() == dbus::kPropertiesGetAll) {
      if (!properties)
        return nullptr;

      std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      AppendAllPropertiesToWriter(&writer);
      return response;
    }
  }
  LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
  return nullptr;
}

MockBatteryObject& MockBatteryObject::ExpectConnectToSignalChanged() {
  EXPECT_CALL(*proxy.get(), DoConnectToSignal(kUPowerDeviceInterfaceName,
                                              kUPowerDeviceSignalChanged, _, _))
      .WillOnce(Invoke(this, &MockBatteryObject::ConnectToSignal));
  return *this;
}

MockBatteryObject& MockBatteryObject::ExpectConnectToSignalPropertyChanged() {
  EXPECT_CALL(*proxy.get(), DoConnectToSignal(dbus::kPropertiesInterface,
                                              dbus::kPropertiesChanged, _, _))
      .WillOnce(Invoke(this, &MockBatteryObject::ConnectToSignal));
  return *this;
}

void MockBatteryObject::SignalChanged() {
  dbus::Signal signal(kUPowerInterfaceName, kUPowerDeviceSignalChanged);
  signal.SetPath(proxy->object_path());
  dbus::MessageWriter writer(&signal);
  writer.AppendString(kUPowerDeviceInterfaceName);
  signal_callback_changed.Run(&signal);
}

void MockBatteryObject::SignalPropertyChanged(
    const std::string& property_name) {
  dbus::Signal signal(dbus::kPropertiesInterface, dbus::kPropertiesChanged);
  signal.SetPath(proxy->object_path());
  dbus::MessageWriter writer(&signal);
  writer.AppendString(kUPowerDeviceInterfaceName);

  // Dictionary {sv} of property-name => new value:
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  writer.CloseContainer(&array_writer);

  // Array of invalidated properties:
  writer.OpenArray("s", &array_writer);
  array_writer.AppendString(property_name);
  writer.CloseContainer(&array_writer);

  signal_callback_properties_changed.Run(&signal);
}

void MockBatteryObject::AppendPropertyToWriter(
    dbus::MessageWriter* writer,
    const std::string& property_name) {
  if (property_name == kUPowerDevicePropertyIsPresent)
    writer->AppendVariantOfBool(properties->is_present);
  else if (property_name == kUPowerDevicePropertyPercentage)
    writer->AppendVariantOfDouble(properties->percentage);
  else if (property_name == kUPowerDevicePropertyState)
    writer->AppendVariantOfUint32(properties->state);
  else if (property_name == kUPowerDevicePropertyTimeToEmpty)
    writer->AppendVariantOfInt64(properties->time_to_empty);
  else if (property_name == kUPowerDevicePropertyTimeToFull)
    writer->AppendVariantOfInt64(properties->time_to_full);
  else if (property_name == kUPowerDevicePropertyType)
    writer->AppendVariantOfUint32(properties->type);
  else
    LOG(WARNING) << __FUNCTION__ << " unknown property: " << property_name;
}

void MockBatteryObject::AppendAllPropertiesToWriter(
    dbus::MessageWriter* writer) {
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);
  writer->OpenArray("{sv}", &array_writer);
  for (auto* property_name :
       {kUPowerDevicePropertyIsPresent, kUPowerDevicePropertyState,
        kUPowerDevicePropertyTimeToEmpty, kUPowerDevicePropertyTimeToFull,
        kUPowerDevicePropertyType}) {
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(property_name);
    AppendPropertyToWriter(&dict_entry_writer, property_name);
    array_writer.CloseContainer(&dict_entry_writer);
  }
  writer->CloseContainer(&array_writer);
}

}  // namespace

class BatteryStatusManagerLinuxTest : public testing::Test {
 public:
  BatteryStatusManagerLinuxTest() {}

  BatteryStatusManagerLinuxTest(const BatteryStatusManagerLinuxTest&) = delete;
  BatteryStatusManagerLinuxTest& operator=(
      const BatteryStatusManagerLinuxTest&) = delete;

  void SetUp() override;

  MockBatteryObject& SetUpDisplayDeviceProxy(MockBatteryProperties* properties);
  void AddDevicePath(const std::string& object_path);
  void PushFrontDevicePath(const std::string& object_path);
  MockBatteryObject& AddDeviceProxy(const std::string& object_path,
                                    MockBatteryProperties* properties);
  MockBatteryObject& PushFrontDeviceProxy(const std::string& object_path,
                                          MockBatteryProperties* properties);

  void ExpectGetObjectProxy(const std::string& object_path,
                            MockBatteryObject* mock_object);
  void ExpectGetObjectProxy(const std::string& object_path,
                            dbus::ObjectProxy* object_proxy);

  void DeviceSignalChanged(MockBatteryObject* device);
  void DeviceSignalPropertyChanged(MockBatteryObject* device,
                                   const std::string& property_name);

  void UPowerSignalDeviceAdded(const std::string& device_path);
  void UPowerSignalDeviceRemoved(const std::string& device_path);

  void StartBatteryStatusManagerLinux();

  int count_battery_updates() const { return count_battery_updates_; }
  const mojom::BatteryStatus& last_battery_status() const {
    return last_status_;
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  MockUPowerObject mock_upower_;
  std::unique_ptr<MockBatteryObject> mock_display_device_;
  std::list<std::unique_ptr<MockBatteryObject>> mock_battery_devices_;

 private:
  std::unique_ptr<MockBatteryObject> CreateMockBatteryObject(
      const std::string& object_path,
      MockBatteryProperties* properties);
  void BatteryUpdateCallback(const mojom::BatteryStatus& status);
  void SyncWithNotifierThread();

  std::unique_ptr<BatteryStatusManagerLinux> manager_;
  int count_battery_updates_ = 0;
  mojom::BatteryStatus last_status_;
};

void BatteryStatusManagerLinuxTest::SetUp() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  options.connection_type = dbus::Bus::PRIVATE;
  mock_bus_ = new NiceMock<dbus::MockBus>(options);

  mock_upower_.proxy = new NiceMock<dbus::MockObjectProxy>(
      mock_bus_.get(), kUPowerServiceName, dbus::ObjectPath(kUPowerPath));
  ExpectGetObjectProxy(kUPowerPath, mock_upower_.proxy.get());
  EXPECT_CALL(*mock_upower_.proxy.get(), CallMethodAndBlock(_, _))
      .WillRepeatedly(
          Invoke(&mock_upower_, &MockUPowerObject::CreateCallMethodResponse));
  EXPECT_CALL(
      *mock_upower_.proxy.get(),
      DoConnectToSignal(kUPowerInterfaceName, kUPowerSignalDeviceAdded, _, _))
      .WillOnce(Invoke(&mock_upower_, &MockUPowerObject::ConnectToSignal));
  EXPECT_CALL(
      *mock_upower_.proxy.get(),
      DoConnectToSignal(kUPowerInterfaceName, kUPowerSignalDeviceRemoved, _, _))
      .WillOnce(Invoke(&mock_upower_, &MockUPowerObject::ConnectToSignal));
}

MockBatteryObject& BatteryStatusManagerLinuxTest::SetUpDisplayDeviceProxy(
    MockBatteryProperties* properties) {
  mock_upower_.display_device = kUPowerDisplayDevicePath;
  mock_display_device_ =
      CreateMockBatteryObject(mock_upower_.display_device, properties);
  return *mock_display_device_.get();
}

void BatteryStatusManagerLinuxTest::AddDevicePath(
    const std::string& object_path) {
  mock_upower_.devices.push_back(object_path);
}

void BatteryStatusManagerLinuxTest::PushFrontDevicePath(
    const std::string& object_path) {
  mock_upower_.devices.push_front(object_path);
}

MockBatteryObject& BatteryStatusManagerLinuxTest::AddDeviceProxy(
    const std::string& object_path,
    MockBatteryProperties* properties) {
  AddDevicePath(object_path);
  mock_battery_devices_.push_back(
      CreateMockBatteryObject(object_path, properties));
  return *mock_battery_devices_.back().get();
}

MockBatteryObject& BatteryStatusManagerLinuxTest::PushFrontDeviceProxy(
    const std::string& object_path,
    MockBatteryProperties* properties) {
  PushFrontDevicePath(object_path);
  mock_battery_devices_.push_front(
      CreateMockBatteryObject(object_path, properties));
  return *mock_battery_devices_.front().get();
}

void BatteryStatusManagerLinuxTest::ExpectGetObjectProxy(
    const std::string& object_path,
    MockBatteryObject* mock_object) {
  ExpectGetObjectProxy(object_path, mock_object->proxy.get());
}

void BatteryStatusManagerLinuxTest::ExpectGetObjectProxy(
    const std::string& object_path,
    dbus::ObjectProxy* object_proxy) {
  EXPECT_CALL(*mock_bus_.get(),
              GetObjectProxy(kUPowerServiceName, dbus::ObjectPath(object_path)))
      .WillOnce(Return(object_proxy));
}

void BatteryStatusManagerLinuxTest::DeviceSignalChanged(
    MockBatteryObject* device) {
  manager_->GetNotifierThreadForTesting()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockBatteryObject::SignalChanged,
                                base::Unretained(device)));
  SyncWithNotifierThread();
}

void BatteryStatusManagerLinuxTest::DeviceSignalPropertyChanged(
    MockBatteryObject* device,
    const std::string& property_name) {
  manager_->GetNotifierThreadForTesting()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockBatteryObject::SignalPropertyChanged,
                                base::Unretained(device), property_name));
  SyncWithNotifierThread();
}

void BatteryStatusManagerLinuxTest::UPowerSignalDeviceAdded(
    const std::string& device_path) {
  ASSERT_FALSE(mock_upower_.signal_callback_device_added.is_null());
  manager_->GetNotifierThreadForTesting()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockUPowerObject::SignalDeviceAdded,
                                base::Unretained(&mock_upower_), device_path));
  SyncWithNotifierThread();
}

void BatteryStatusManagerLinuxTest::UPowerSignalDeviceRemoved(
    const std::string& device_path) {
  ASSERT_FALSE(mock_upower_.signal_callback_device_removed.is_null());
  manager_->GetNotifierThreadForTesting()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockUPowerObject::SignalDeviceRemoved,
                                base::Unretained(&mock_upower_), device_path));
  SyncWithNotifierThread();
}

void BatteryStatusManagerLinuxTest::StartBatteryStatusManagerLinux() {
  manager_ = BatteryStatusManagerLinux::CreateForTesting(
      base::BindRepeating(&BatteryStatusManagerLinuxTest::BatteryUpdateCallback,
                          base::Unretained(this)),
      mock_bus_.get());
  manager_->StartListeningBatteryChange();
  SyncWithNotifierThread();
}

std::unique_ptr<MockBatteryObject>
BatteryStatusManagerLinuxTest::CreateMockBatteryObject(
    const std::string& object_path,
    MockBatteryProperties* properties) {
  std::unique_ptr<MockBatteryObject> mock_object(
      new MockBatteryObject(mock_bus_.get(), object_path, properties));
  ExpectGetObjectProxy(object_path, mock_object.get());
  EXPECT_CALL(*mock_object->proxy.get(), CallMethodAndBlock(_, _))
      .WillRepeatedly(Invoke(mock_object.get(),
                             &MockBatteryObject::CreateCallMethodResponse));
  return mock_object;
}

void BatteryStatusManagerLinuxTest::BatteryUpdateCallback(
    const mojom::BatteryStatus& status) {
  ++count_battery_updates_;
  last_status_ = status;
}

void BatteryStatusManagerLinuxTest::SyncWithNotifierThread() {
  ASSERT_TRUE(manager_ != nullptr);
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  manager_->GetNotifierThreadForTesting()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
  event.Wait();
}

TEST_F(BatteryStatusManagerLinuxTest, NoBattery) {
  mojom::BatteryStatus default_status;
  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_EQ(default_status.charging, status.charging);
  EXPECT_EQ(default_status.charging_time, status.charging_time);
  EXPECT_EQ(default_status.discharging_time, status.discharging_time);
  EXPECT_EQ(default_status.level, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, ChargingHalfFull) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.percentage = 50;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.5, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, ChargingTimeToFull) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.percentage = 1;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.01, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, FullyCharged) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state = UPowerDeviceState::UPOWER_DEVICE_STATE_FULL;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 100;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(0, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, Discharging) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 90;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DischargingTimeToEmptyUnknown) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 0;
  battery_bat0_properties.percentage = 90;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DeviceStateUnknown) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_UNKNOWN;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 0;
  battery_bat0_properties.percentage = 50;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.5, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DeviceStateEmpty) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state = UPowerDeviceState::UPOWER_DEVICE_STATE_EMPTY;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 0;
  battery_bat0_properties.percentage = 0;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, LevelRoundedToThreeSignificantDigits) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.percentage = 14.56;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.15, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UsingFirstBatteryDevice) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  MockBatteryProperties battery_bat1_properties;
  battery_bat1_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat1_properties.time_to_full = 100;
  battery_bat1_properties.time_to_empty = 0;
  battery_bat1_properties.percentage = 80;
  AddDeviceProxy(kUPowerDeviceBattery1Path, &battery_bat1_properties);

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, SkipNonBatteryDevice) {
  MockBatteryProperties line_power_AC_properties;
  line_power_AC_properties.type =
      UPowerDeviceType::UPOWER_DEVICE_TYPE_LINE_POWER;
  AddDeviceProxy(kUPowerDeviceACLinePath, &line_power_AC_properties);

  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyState) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 80;
  MockBatteryObject& battery_bat0 =
      AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
          .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  DeviceSignalPropertyChanged(&battery_bat0, kUPowerDevicePropertyState);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyPercentage) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 80;
  MockBatteryObject& battery_bat0 =
      AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
          .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  battery_bat0_properties.percentage = 70;
  DeviceSignalPropertyChanged(&battery_bat0, kUPowerDevicePropertyPercentage);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyTimeToEmpty) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 80;
  MockBatteryObject& battery_bat0 =
      AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
          .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  battery_bat0_properties.time_to_empty = 150;
  DeviceSignalPropertyChanged(&battery_bat0, kUPowerDevicePropertyTimeToEmpty);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(150, status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyTimeToFull) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 80;
  MockBatteryObject& battery_bat0 =
      AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
          .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  battery_bat0_properties.time_to_full = 50;
  DeviceSignalPropertyChanged(&battery_bat0, kUPowerDevicePropertyTimeToFull);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(50, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, OldDaemonDeviceSignalChanged) {
  mock_upower_.daemon_version = "0.9.23";

  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 100;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 80;
  MockBatteryObject& battery_bat0 =
      AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
          .ExpectConnectToSignalChanged()
          .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  battery_bat0_properties.percentage = 70;
  DeviceSignalChanged(&battery_bat0);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DisplayDeviceNoBattery) {
  MockBatteryProperties display_device_properties;
  display_device_properties.type = UPowerDeviceType::UPOWER_DEVICE_TYPE_UNKNOWN;
  SetUpDisplayDeviceProxy(&display_device_properties);

  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 90;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DisplayDeviceBattery) {
  MockBatteryProperties display_device_properties;
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 90;
  SetUpDisplayDeviceProxy(&display_device_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DisplayDeviceBatterySkipsEnumerate) {
  MockBatteryProperties display_device_properties;
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 90;
  SetUpDisplayDeviceProxy(&display_device_properties)
      .ExpectConnectToSignalPropertyChanged();

  AddDevicePath(kUPowerDeviceACLinePath);
  AddDevicePath(kUPowerDeviceBattery0Path);
  AddDevicePath(kUPowerDeviceBattery1Path);

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

// Adding a display-device will make the BatteryStatusManagerLinux switch to
// the display-device.
TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceAddedDisplayDevice) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);

  int last_count = count_battery_updates();
  MockBatteryProperties display_device_properties;
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 150;
  display_device_properties.percentage = 80;
  SetUpDisplayDeviceProxy(&display_device_properties)
      .ExpectConnectToSignalPropertyChanged();

  UPowerSignalDeviceAdded(mock_upower_.display_device);
  status = last_battery_status();
  EXPECT_LT(last_count, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.8, status.level);
}

// Prepending a battery should switch to that battery.
TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceAddedBatteryAtFront) {
  MockBatteryProperties battery_bat1_properties;
  battery_bat1_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat1_properties.time_to_full = 0;
  battery_bat1_properties.time_to_empty = 200;
  battery_bat1_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery1Path, &battery_bat1_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);

  int last_count = count_battery_updates();
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 150;
  battery_bat0_properties.percentage = 50;
  PushFrontDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();
  UPowerSignalDeviceAdded(kUPowerDeviceBattery0Path);
  status = last_battery_status();
  EXPECT_LT(last_count, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(150, status.discharging_time);
  EXPECT_EQ(0.5, status.level);
}

// Appending a battery should keep the current battery.
TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceAddedBatteryAtBack) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 150;
  battery_bat0_properties.percentage = 50;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(150, status.discharging_time);
  EXPECT_EQ(0.5, status.level);

  int last_count = count_battery_updates();
  MockBatteryProperties battery_bat1_properties;
  battery_bat1_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat1_properties.time_to_full = 0;
  battery_bat1_properties.time_to_empty = 200;
  battery_bat1_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery1Path, &battery_bat1_properties);
  UPowerSignalDeviceAdded(kUPowerDeviceBattery1Path);
  status = last_battery_status();
  EXPECT_LT(last_count, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(150, status.discharging_time);
  EXPECT_EQ(0.5, status.level);
}

// Adding a device that is no battery should not change anything.
TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceAddedNoBattery) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);

  int last_count = count_battery_updates();
  MockBatteryProperties line_power_AC_properties;
  line_power_AC_properties.type =
      UPowerDeviceType::UPOWER_DEVICE_TYPE_LINE_POWER;
  PushFrontDeviceProxy(kUPowerDeviceACLinePath, &line_power_AC_properties);
  UPowerSignalDeviceAdded(kUPowerDeviceACLinePath);
  status = last_battery_status();
  EXPECT_LT(last_count, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceRemovedBattery) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  MockBatteryProperties battery_bat1_properties;
  battery_bat1_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  battery_bat1_properties.time_to_full = 100;
  battery_bat1_properties.time_to_empty = 0;
  battery_bat1_properties.percentage = 80;
  MockBatteryObject& battery_bat1 =
      AddDeviceProxy(kUPowerDeviceBattery1Path, &battery_bat1_properties);

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);

  int last_count = count_battery_updates();
  ExpectGetObjectProxy(kUPowerDeviceBattery1Path, &battery_bat1);
  battery_bat1.ExpectConnectToSignalPropertyChanged();

  EXPECT_EQ(kUPowerDeviceBattery0Path, mock_upower_.devices.front());
  mock_upower_.devices.pop_front();
  UPowerSignalDeviceRemoved(kUPowerDeviceBattery0Path);
  status = last_battery_status();
  EXPECT_LT(last_count, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, SignalDeviceRemovedOther) {
  MockBatteryProperties battery_bat0_properties;
  battery_bat0_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  battery_bat0_properties.time_to_full = 0;
  battery_bat0_properties.time_to_empty = 200;
  battery_bat0_properties.percentage = 70;
  AddDeviceProxy(kUPowerDeviceBattery0Path, &battery_bat0_properties)
      .ExpectConnectToSignalPropertyChanged();

  MockBatteryProperties line_power_AC_properties;
  line_power_AC_properties.type =
      UPowerDeviceType::UPOWER_DEVICE_TYPE_LINE_POWER;
  AddDeviceProxy(kUPowerDeviceACLinePath, &line_power_AC_properties);

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);

  int last_count = count_battery_updates();
  mock_upower_.devices.pop_back();
  UPowerSignalDeviceRemoved(kUPowerDeviceACLinePath);
  status = last_battery_status();
  EXPECT_EQ(last_count, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(0.7, status.level);
}

}  // namespace device
