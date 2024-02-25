// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/bluez/bluetooth_low_energy_scan_session_bluez.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider_impl.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider_impl.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_service_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

constexpr char kExpectedMessage1[] = R"(message_type: MESSAGE_METHOD_RETURN
signature: a{oa{sa{sv}}}
reply_serial: 123

array [
]
)";

constexpr char kExpectedMessage2[] = R"(message_type: MESSAGE_METHOD_RETURN
signature: a{oa{sa{sv}}}
reply_serial: 123

array [
  dict entry {
    object_path "/path/monitor1"
    array [
      dict entry {
        string "org.bluez.AdvertisementMonitor1"
        array [
          dict entry {
            string "Type"
            variant               string "or_patterns"
          }
          dict entry {
            string "RSSIHighThreshold"
            variant               int16_t -65
          }
          dict entry {
            string "RSSIHighTimeout"
            variant               uint16_t 1
          }
          dict entry {
            string "RSSILowThreshold"
            variant               int16_t -80
          }
          dict entry {
            string "RSSILowTimeout"
            variant               uint16_t 3
          }
          dict entry {
            string "Patterns"
            variant               array [
                struct {
                  byte 0
                  byte 22
                  array [
                    byte 44
                    byte 254
                    byte 252
                    byte 18
                    byte 142
                  ]
                }
              ]
          }
        ]
      }
    ]
  }
]
)";

constexpr char kExpectedMessage3[] = R"(message_type: MESSAGE_SIGNAL
interface: org.freedesktop.DBus.ObjectManager
member: InterfacesAdded
signature: oa{sa{sv}}

object_path "/path/monitor1"
array [
  dict entry {
    string "org.bluez.AdvertisementMonitor1"
    array [
      dict entry {
        string "Type"
        variant           string "or_patterns"
      }
      dict entry {
        string "RSSIHighThreshold"
        variant           int16_t -65
      }
      dict entry {
        string "RSSIHighTimeout"
        variant           uint16_t 1
      }
      dict entry {
        string "RSSILowThreshold"
        variant           int16_t -80
      }
      dict entry {
        string "RSSILowTimeout"
        variant           uint16_t 3
      }
      dict entry {
        string "Patterns"
        variant           array [
            struct {
              byte 0
              byte 22
              array [
                byte 44
                byte 254
                byte 252
                byte 18
                byte 142
              ]
            }
          ]
      }
    ]
  }
]
)";

constexpr char kExpectedMessage4[] = R"(message_type: MESSAGE_SIGNAL
interface: org.freedesktop.DBus.ObjectManager
member: InterfacesRemoved
signature: oas

object_path "/path/monitor1"
array [
  string "org.bluez.AdvertisementMonitor1"
]
)";

constexpr base::TimeDelta kDeviceFoundTimeout = base::Seconds(1);
constexpr base::TimeDelta kDeviceLostTimeout = base::Seconds(3);
// Used to verify the filter pattern value provided by the scanner.
constexpr uint8_t kPatternValue[] = {0x2c, 0xfe, 0xfc, 0x12, 0x8e};

class FakeBluetoothLowEnergyScanSessionDelegate
    : public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  FakeBluetoothLowEnergyScanSessionDelegate() = default;

  // BluetoothLowEnergyScanSession::Delegate
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override {}
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override {}
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override {}
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override {}

  base::WeakPtr<FakeBluetoothLowEnergyScanSessionDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeBluetoothLowEnergyScanSessionDelegate>
      weak_ptr_factory_{this};
};

void ResponseSenderCallback(const std::string& expected_message,
                            std::unique_ptr<dbus::Response> response) {
  EXPECT_EQ(expected_message, response->ToString());
}

void SendSignal(const std::string& expected_message, dbus::Signal* signal) {
  EXPECT_EQ(expected_message, signal->ToString());
}

void SetUpMocksDbus(dbus::MockBus* mock_bus,
                    dbus::MockExportedObject* mock_exported_object) {
  EXPECT_CALL(*mock_bus, GetExportedObject(dbus::ObjectPath("/path")))
      .WillOnce(testing::Return(mock_exported_object));
  EXPECT_CALL(*mock_exported_object,
              ExportMethod(dbus::kDBusObjectManagerInterface,
                           dbus::kDBusObjectManagerGetManagedObjects,
                           testing::_, testing::_));
}

void SetupExpectedMockAdvertisementMonitorDbusCalls(
    dbus::MockBus* mock_bus,
    dbus::MockExportedObject* mock_exported_object,
    dbus::ObjectPath monitor_object_path) {
  EXPECT_CALL(*mock_bus, GetExportedObject(monitor_object_path))
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

TEST(BluetoothAdvertisementMonitorApplicationServiceProviderImplTest,
     AddMonitorThenRemoveMonitor) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath application_object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(),
                                   application_object_path);
  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorApplicationServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/application_object_path);

  dbus::ObjectPath monitor_object_path = dbus::ObjectPath("/path/monitor1");
  FakeBluetoothLowEnergyScanSessionDelegate delegate;
  auto low_energy_scan_session =
      std::make_unique<BluetoothLowEnergyScanSessionBlueZ>(
          monitor_object_path.value(), nullptr, delegate.GetWeakPtr(),
          base::DoNothing());
  std::vector<uint8_t> pattern_value(std::begin(kPatternValue),
                                     std::end(kPatternValue));
  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      /*start_position=*/0,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      std::move(pattern_value));
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear, kDeviceFoundTimeout,
      kDeviceLostTimeout, {pattern}, /*rssi_sampling_period=*/std::nullopt);

  SetupExpectedMockAdvertisementMonitorDbusCalls(
      mock_bus.get(), mock_exported_object.get(), monitor_object_path);
  auto advertisement_monitor =
      std::make_unique<BluetoothAdvertisementMonitorServiceProviderImpl>(
          mock_bus.get(), monitor_object_path, std::move(filter),
          low_energy_scan_session->GetWeakPtr());
  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  provider_impl.GetManagedObjects(
      &method_call, base::BindOnce(&ResponseSenderCallback, kExpectedMessage1));

  ON_CALL(*mock_exported_object, SendSignal(testing::_))
      .WillByDefault(testing::Invoke(
          [](dbus::Signal* signal) { SendSignal(kExpectedMessage3, signal); }));
  EXPECT_CALL(*mock_exported_object, SendSignal(testing::_));

  provider_impl.AddMonitor(std::move(advertisement_monitor));

  provider_impl.GetManagedObjects(
      &method_call, base::BindOnce(&ResponseSenderCallback, kExpectedMessage2));

  ON_CALL(*mock_exported_object, SendSignal(testing::_))
      .WillByDefault(testing::Invoke(
          [](dbus::Signal* signal) { SendSignal(kExpectedMessage4, signal); }));
  EXPECT_CALL(*mock_exported_object, SendSignal(testing::_));
  provider_impl.RemoveMonitor(monitor_object_path);

  provider_impl.GetManagedObjects(
      &method_call, base::BindOnce(&ResponseSenderCallback, kExpectedMessage1));
}

TEST(BluetoothAdvertisementMonitorApplicationServiceProviderImplTest,
     RemoveFailure) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);
  dbus::ObjectPath application_object_path = dbus::ObjectPath("/path");
  scoped_refptr<dbus::MockExportedObject> mock_exported_object =
      new dbus::MockExportedObject(/*bus=*/mock_bus.get(),
                                   application_object_path);
  SetUpMocksDbus(mock_bus.get(), mock_exported_object.get());

  BluetoothAdvertisementMonitorApplicationServiceProviderImpl provider_impl(
      /*bus=*/mock_bus.get(),
      /*object_path=*/application_object_path);
  dbus::ObjectPath monitor_object_path = dbus::ObjectPath("/path/monitor1");
  EXPECT_CALL(*mock_exported_object, SendSignal(testing::_)).Times(0);
  provider_impl.RemoveMonitor(monitor_object_path);
}

}  // namespace bluez
