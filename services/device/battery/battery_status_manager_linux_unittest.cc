// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager_linux.h"

#include <limits>
#include <list>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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
  explicit MockBatteryObject(dbus::Bus* bus)
      : proxy(new NiceMock<dbus::MockObjectProxy>(
            bus,
            kUPowerServiceName,
            dbus::ObjectPath(kUPowerDevicePath))) {}

  MockBatteryObject(const MockBatteryObject&) = delete;
  MockBatteryObject& operator=(const MockBatteryObject&) = delete;

  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    bool on_connected_success = true;
    if (interface_name == dbus::kPropertiesInterface &&
        signal_name == dbus::kPropertiesChanged) {
      signal_callback_properties_changed = signal_callback;
    } else {
      on_connected_success = false;
    }

    if (!on_connected_success) {
      LOG(WARNING) << "MockBatteryObject::" << __FUNCTION__
                   << " Unexpected interface=" << interface_name
                   << ", signal=" << signal_name;
    }
    std::move(on_connected_callback)
        .Run(interface_name, signal_name, on_connected_success);
  }

  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms,
                  dbus::ObjectProxy::ResponseCallback callback) {
    std::unique_ptr<dbus::Response> response;
    if (method_call->GetInterface() == dbus::kPropertiesInterface) {
      if (method_call->GetMember() == dbus::kPropertiesGet) {
        dbus::MessageReader reader(method_call);
        std::string interface_name;
        std::string property_name;
        if (reader.PopString(&interface_name) &&
            reader.PopString(&property_name)) {
          response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          AppendPropertyToWriter(&writer, property_name);
        }
      } else if (method_call->GetMember() == dbus::kPropertiesGetAll) {
        response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        AppendAllPropertiesToWriter(&writer);
      }
    }

    if (!response) {
      ADD_FAILURE() << "Unexpected method call: " << method_call->ToString();
    }

    std::move(callback).Run(response.get());
  }

  MockBatteryObject& ExpectConnectToSignalPropertyChanged() {
    EXPECT_CALL(*proxy.get(), ConnectToSignal(dbus::kPropertiesInterface,
                                              dbus::kPropertiesChanged, _, _))
        .WillRepeatedly(
            [this](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              ConnectToSignal(interface_name, signal_name, signal_callback,
                              std::move(on_connected_callback));
            });
    return *this;
  }

  void SignalPropertyChanged(const std::string& property_name) {
    dbus::Signal signal(dbus::kPropertiesInterface, dbus::kPropertiesChanged);
    signal.SetPath(proxy->object_path());
    dbus::MessageWriter writer(&signal);
    writer.AppendString(kUPowerDeviceInterfaceName);

    // Dictionary {sv} of property-name => new value:
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("{sv}", &array_writer);
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(property_name);
    AppendPropertyToWriter(&dict_entry_writer, property_name);
    array_writer.CloseContainer(&dict_entry_writer);
    writer.CloseContainer(&array_writer);

    // Array of invalidated properties:
    writer.OpenArray("s", &array_writer);
    writer.CloseContainer(&array_writer);

    signal_callback_properties_changed.Run(&signal);
  }

  scoped_refptr<dbus::MockObjectProxy> proxy;
  MockBatteryProperties properties;
  dbus::ObjectProxy::SignalCallback signal_callback_properties_changed;

 private:
  void AppendPropertyToWriter(dbus::MessageWriter* writer,
                              const std::string& property_name) {
    if (property_name == kUPowerDevicePropertyIsPresent) {
      writer->AppendVariantOfBool(properties.is_present);
    } else if (property_name == kUPowerDevicePropertyPercentage) {
      writer->AppendVariantOfDouble(properties.percentage);
    } else if (property_name == kUPowerDevicePropertyState) {
      writer->AppendVariantOfUint32(properties.state);
    } else if (property_name == kUPowerDevicePropertyTimeToEmpty) {
      writer->AppendVariantOfInt64(properties.time_to_empty);
    } else if (property_name == kUPowerDevicePropertyTimeToFull) {
      writer->AppendVariantOfInt64(properties.time_to_full);
    } else if (property_name == kUPowerDevicePropertyType) {
      writer->AppendVariantOfUint32(properties.type);
    } else {
      ADD_FAILURE() << " unknown property: " << property_name;
    }
  }

  void AppendAllPropertiesToWriter(dbus::MessageWriter* writer) {
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);
    writer->OpenArray("{sv}", &array_writer);
    for (auto* property_name :
         {kUPowerDevicePropertyIsPresent, kUPowerDevicePropertyPercentage,
          kUPowerDevicePropertyState, kUPowerDevicePropertyTimeToEmpty,
          kUPowerDevicePropertyTimeToFull, kUPowerDevicePropertyType}) {
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(property_name);
      AppendPropertyToWriter(&dict_entry_writer, property_name);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }
};

}  // namespace

class BatteryStatusManagerLinuxTest : public testing::Test {
 public:
  BatteryStatusManagerLinuxTest() = default;

  BatteryStatusManagerLinuxTest(const BatteryStatusManagerLinuxTest&) = delete;
  BatteryStatusManagerLinuxTest& operator=(
      const BatteryStatusManagerLinuxTest&) = delete;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    mock_bus_ = new NiceMock<dbus::MockBus>(std::move(options));

    EXPECT_CALL(*mock_bus_, Connect()).WillRepeatedly(Return(true));
    mock_display_device_ = CreateMockBatteryObject();
    mock_display_device_->ExpectConnectToSignalPropertyChanged();
  }

  void ExpectGetObjectProxy(const std::string& object_path,
                            MockBatteryObject* mock_object) {
    EXPECT_CALL(*mock_bus_.get(), GetObjectProxy(kUPowerServiceName,
                                                 dbus::ObjectPath(object_path)))
        .WillRepeatedly(Return(mock_object->proxy.get()));
  }

  void DeviceSignalPropertyChanged(MockBatteryObject* device,
                                   const std::string& property_name) {
    device->SignalPropertyChanged(property_name);
  }

  void StartBatteryStatusManagerLinux() {
    manager_ = BatteryStatusManagerLinux::CreateForTesting(
        base::BindRepeating(
            &BatteryStatusManagerLinuxTest::BatteryUpdateCallback,
            base::Unretained(this)),
        mock_bus_.get());
    manager_->StartListeningBatteryChange();
  }

  int count_battery_updates() const { return count_battery_updates_; }
  const mojom::BatteryStatus& last_battery_status() const {
    return last_status_;
  }

  MockBatteryObject* GetDisplayDevice() { return mock_display_device_.get(); }

  MockBatteryProperties& GetDisplayDeviceProperties() {
    return mock_display_device_->properties;
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  std::unique_ptr<MockBatteryObject> mock_display_device_;

 private:
  std::unique_ptr<MockBatteryObject> CreateMockBatteryObject() {
    auto mock_object = std::make_unique<MockBatteryObject>(mock_bus_.get());
    ExpectGetObjectProxy(kUPowerDevicePath, mock_object.get());
    EXPECT_CALL(*mock_object->proxy.get(), CallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(mock_object.get(), &MockBatteryObject::CallMethod));
    return mock_object;
  }

  void BatteryUpdateCallback(const mojom::BatteryStatus& status) {
    ++count_battery_updates_;
    last_status_ = status;
  }

  std::unique_ptr<BatteryStatusManagerLinux> manager_;
  int count_battery_updates_ = 0;
  mojom::BatteryStatus last_status_;
};

TEST_F(BatteryStatusManagerLinuxTest, NoBattery) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.is_present = false;

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
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.percentage = 50;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.5, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, ChargingTimeToFull) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.percentage = 1;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.01, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, FullyCharged) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state = UPowerDeviceState::UPOWER_DEVICE_STATE_FULL;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 100;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(0, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, Discharging) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 90;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DischargingTimeToEmptyUnknown) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 0;
  display_device_properties.percentage = 90;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DeviceStateUnknown) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_UNKNOWN;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 0;
  display_device_properties.percentage = 50;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.5, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DeviceStateEmpty) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_EMPTY;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 0;
  display_device_properties.percentage = 0;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, LevelRoundedToThreeSignificantDigits) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.percentage = 14.56;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(0.15, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyState) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 80;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  DeviceSignalPropertyChanged(GetDisplayDevice(), kUPowerDevicePropertyState);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyPercentage) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 80;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  display_device_properties.percentage = 70;
  DeviceSignalPropertyChanged(GetDisplayDevice(),
                              kUPowerDevicePropertyPercentage);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.7, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyTimeToEmpty) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 80;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  display_device_properties.time_to_empty = 150;
  DeviceSignalPropertyChanged(GetDisplayDevice(),
                              kUPowerDevicePropertyTimeToEmpty);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(150, status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, UpdateDevicePropertyTimeToFull) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_CHARGING;
  display_device_properties.time_to_full = 100;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 80;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_TRUE(status.charging);
  EXPECT_EQ(100, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);

  int last_count = count_battery_updates();
  display_device_properties.time_to_full = 50;
  DeviceSignalPropertyChanged(GetDisplayDevice(),
                              kUPowerDevicePropertyTimeToFull);
  status = last_battery_status();

  EXPECT_LT(last_count, count_battery_updates());
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(50, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.8, status.level);
}

TEST_F(BatteryStatusManagerLinuxTest, DisplayDeviceBattery) {
  MockBatteryProperties& display_device_properties =
      GetDisplayDeviceProperties();
  display_device_properties.state =
      UPowerDeviceState::UPOWER_DEVICE_STATE_DISCHARGING;
  display_device_properties.time_to_full = 0;
  display_device_properties.time_to_empty = 200;
  display_device_properties.percentage = 90;

  StartBatteryStatusManagerLinux();
  mojom::BatteryStatus status = last_battery_status();
  EXPECT_LE(1, count_battery_updates());

  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(.9, status.level);
}

}  // namespace device
