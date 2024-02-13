// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothGattConnection;
using device::BluetoothGattNotifySession;
using device::BluetoothGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;
using device::TestBluetoothAdapterObserver;

using UUIDSet = device::BluetoothDevice::UUIDSet;
using WriteType = device::BluetoothRemoteGattCharacteristic::WriteType;

typedef std::unordered_map<device::BluetoothDevice*,
                           device::BluetoothDevice::UUIDSet>
    DeviceToUUIDs;

namespace bluez {

namespace {

const BluetoothUUID kGenericAccessServiceUUID(
    bluez::FakeBluetoothGattServiceClient::kGenericAccessServiceUUID);
const BluetoothUUID kBatteryServiceUUID(
    bluez::FakeBluetoothGattServiceClient::kBatteryServiceUUID);
const BluetoothUUID kHeartRateServiceUUID(
    bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID);
const BluetoothUUID kHeartRateMeasurementUUID(
    bluez::FakeBluetoothGattCharacteristicClient::kHeartRateMeasurementUUID);
const BluetoothUUID kBodySensorLocationUUID(
    bluez::FakeBluetoothGattCharacteristicClient::kBodySensorLocationUUID);
const BluetoothUUID kHeartRateControlPointUUID(
    bluez::FakeBluetoothGattCharacteristicClient::kHeartRateControlPointUUID);

// Compares GATT characteristic/descriptor values. Returns true, if the values
// are equal.
bool ValuesEqual(const std::vector<uint8_t>& value0,
                 const std::vector<uint8_t>& value1) {
  if (value0.size() != value1.size())
    return false;
  for (size_t i = 0; i < value0.size(); ++i)
    if (value0[i] != value1[i])
      return false;
  return true;
}

void AddDeviceFilterWithUUID(BluetoothDiscoveryFilter* filter,
                             BluetoothUUID uuid) {
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(uuid);
  filter->AddDeviceFilter(device_filter);
}

}  // namespace

class BluetoothGattBlueZTest : public testing::Test {
 public:
  BluetoothGattBlueZTest()
      : fake_bluetooth_gatt_service_client_(nullptr),
        success_callback_count_(0),
        error_callback_count_(0) {}

  void SetUp() override {
    // TODO(b/266989920) Remove when Floss fake implementation is completed.
    if (floss::features::IsFlossEnabled()) {
      GTEST_SKIP();
    }
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    auto fake_bluetooth_device_client =
        std::make_unique<bluez::FakeBluetoothDeviceClient>();
    auto fake_bluetooth_gatt_service_client =
        std::make_unique<bluez::FakeBluetoothGattServiceClient>();
    auto fake_bluetooth_gatt_characteristic_client =
        std::make_unique<bluez::FakeBluetoothGattCharacteristicClient>();
    auto fake_bluetooth_gatt_descriptor_client =
        std::make_unique<bluez::FakeBluetoothGattDescriptorClient>();

    fake_bluetooth_device_client_ = fake_bluetooth_device_client.get();
    fake_bluetooth_gatt_service_client_ =
        fake_bluetooth_gatt_service_client.get();
    fake_bluetooth_gatt_characteristic_client_ =
        fake_bluetooth_gatt_characteristic_client.get();
    fake_bluetooth_gatt_descriptor_client_ =
        fake_bluetooth_gatt_descriptor_client.get();

    dbus_setter->SetBluetoothDeviceClient(
        std::move(fake_bluetooth_device_client));
    dbus_setter->SetBluetoothGattServiceClient(
        std::move(fake_bluetooth_gatt_service_client));
    dbus_setter->SetBluetoothGattCharacteristicClient(
        std::move(fake_bluetooth_gatt_characteristic_client));
    dbus_setter->SetBluetoothGattDescriptorClient(
        std::move(fake_bluetooth_gatt_descriptor_client));
    dbus_setter->SetBluetoothAdapterClient(
        std::make_unique<bluez::FakeBluetoothAdapterClient>());
    dbus_setter->SetBluetoothInputClient(
        std::make_unique<bluez::FakeBluetoothInputClient>());
    dbus_setter->SetBluetoothAgentManagerClient(
        std::make_unique<bluez::FakeBluetoothAgentManagerClient>());

    GetAdapter();

    adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    if (floss::features::IsFlossEnabled()) {
      return;
    }
    adapter_.reset();
    update_sessions_.clear();
    gatt_conn_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  void GetAdapter() {
    base::RunLoop run_loop;
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindLambdaForTesting(
            [&](scoped_refptr<BluetoothAdapter> adapter) {
              adapter_ = std::move(adapter);
              run_loop.Quit();
            }));
    run_loop.Run();
    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());
  }

  BluetoothDevice* AddLeDevice() {
    fake_bluetooth_device_client_->CreateDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    bluez::FakeBluetoothDeviceClient::Properties* properties1 =
        fake_bluetooth_device_client_->GetProperties(
            dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    properties1->connected.ReplaceValue(true);
    properties1->connected_le.ReplaceValue(true);

    return adapter_->GetDevice(
        bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  }

  BluetoothDevice* AddDualDevice() {
    fake_bluetooth_device_client_->CreateDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDualPath));
    bluez::FakeBluetoothDeviceClient::Properties* properties2 =
        fake_bluetooth_device_client_->GetProperties(
            dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDualPath));
    properties2->connected.ReplaceValue(true);
    properties2->connected_le.ReplaceValue(true);

    return adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kDualAddress);
  }

  void BatteryServiceShouldBeComplete(BluetoothDevice* device) {
    ASSERT_TRUE(device);
    ASSERT_GE(device->GetGattServices().size(), 1u);

    BluetoothRemoteGattService* service = device->GetGattService(
        fake_bluetooth_gatt_service_client_->GetBatteryServicePath().value());

    ASSERT_TRUE(service);
    EXPECT_TRUE(service->IsPrimary());
    EXPECT_EQ(BluetoothUUID(
                  bluez::FakeBluetoothGattServiceClient::kBatteryServiceUUID),
              service->GetUUID());
    EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));
  }

  void HeartRateServiceShouldBeComplete(BluetoothDevice* device) {
    ASSERT_TRUE(device);
    ASSERT_GE(device->GetGattServices().size(), 1u);

    BluetoothRemoteGattService* service = device->GetGattService(
        fake_bluetooth_gatt_service_client_->GetHeartRateServicePath().value());

    ASSERT_TRUE(service);
    EXPECT_TRUE(service->IsPrimary());
    EXPECT_EQ(BluetoothUUID(
                  bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
              service->GetUUID());
    EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));
    EXPECT_EQ(3U, service->GetCharacteristics().size());

    BluetoothRemoteGattCharacteristic* characteristic =
        service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                       ->GetBodySensorLocationPath()
                                       .value());
    ASSERT_TRUE(characteristic);
    EXPECT_EQ(BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                kBodySensorLocationUUID),
              characteristic->GetUUID());
    EXPECT_TRUE(characteristic->GetDescriptors().empty());

    characteristic =
        service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                       ->GetHeartRateControlPointPath()
                                       .value());
    ASSERT_TRUE(characteristic);
    EXPECT_EQ(BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                kHeartRateControlPointUUID),
              characteristic->GetUUID());
    EXPECT_TRUE(characteristic->GetDescriptors().empty());

    characteristic =
        service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                       ->GetHeartRateMeasurementPath()
                                       .value());
    ASSERT_TRUE(characteristic);
    EXPECT_EQ(BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                kHeartRateMeasurementUUID),
              characteristic->GetUUID());

    ASSERT_EQ(1u, characteristic->GetDescriptors().size());

    BluetoothRemoteGattDescriptor* descriptor =
        characteristic->GetDescriptors()[0];
    EXPECT_EQ(
        BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid(),
        descriptor->GetUUID());
  }

  void SuccessCallback() { ++success_callback_count_; }

  void ValueCallback(
      std::optional<BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value) {
    if (error_code.has_value()) {
      ++error_callback_count_;
      last_service_error_ = error_code.value();
    } else {
      ++success_callback_count_;
      last_read_value_ = value;
    }
  }

  void GattConnectionCallback(
      std::unique_ptr<BluetoothGattConnection> conn,
      std::optional<BluetoothDevice::ConnectErrorCode> error) {
    if (error.has_value()) {
      ++error_callback_count_;
      return;
    }
    ++success_callback_count_;
    gatt_conn_ = std::move(conn);
  }

  void NotifySessionCallback(
      std::unique_ptr<BluetoothGattNotifySession> session) {
    ++success_callback_count_;
    update_sessions_.push_back(std::move(session));
  }

  void ServiceErrorCallback(BluetoothGattService::GattErrorCode err) {
    ++error_callback_count_;
    last_service_error_ = err;
  }

  void ErrorCallback() { ++error_callback_count_; }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
  }

 protected:

  base::test::SingleThreadTaskEnvironment task_environment_;

  raw_ptr<bluez::FakeBluetoothDeviceClient, DanglingUntriaged>
      fake_bluetooth_device_client_;
  raw_ptr<bluez::FakeBluetoothGattServiceClient, DanglingUntriaged>
      fake_bluetooth_gatt_service_client_;
  raw_ptr<bluez::FakeBluetoothGattCharacteristicClient, DanglingUntriaged>
      fake_bluetooth_gatt_characteristic_client_;
  raw_ptr<bluez::FakeBluetoothGattDescriptorClient, DanglingUntriaged>
      fake_bluetooth_gatt_descriptor_client_;
  std::unique_ptr<device::BluetoothGattConnection> gatt_conn_;
  std::vector<std::unique_ptr<BluetoothGattNotifySession>> update_sessions_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int success_callback_count_;
  int error_callback_count_;
  std::vector<uint8_t> last_read_value_;
  BluetoothGattService::GattErrorCode last_service_error_;
};

TEST_F(BluetoothGattBlueZTest,
       RetrieveGattConnectedDevicesWithDiscoveryFilter_NoFilter) {
  BluetoothDevice* le = AddLeDevice();
  BluetoothDevice* dual = AddDualDevice();

  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(DeviceToUUIDs({{le, UUIDSet()}, {dual, UUIDSet()}}), result);
}

TEST_F(BluetoothGattBlueZTest,
       RetrieveGattConnectedDevicesWithDiscoveryFilter_NonMatchingFilter) {
  AddLeDevice();
  AddDualDevice();

  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(&discovery_filter, kBatteryServiceUUID);

  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_TRUE(result.empty());
}

TEST_F(
    BluetoothGattBlueZTest,
    RetrieveGattConnectedDevicesWithDiscoveryFilter_OneMatchingServiceOneDevice) {
  AddLeDevice();
  BluetoothDevice* dual = AddDualDevice();

  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(kGenericAccessServiceUUID);
  discovery_filter.AddDeviceFilter(device_filter);

  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(DeviceToUUIDs({{dual, UUIDSet({kGenericAccessServiceUUID})}}),
            result);
}

TEST_F(
    BluetoothGattBlueZTest,
    RetrieveGattConnectedDevicesWithDiscoveryFilter_OneMatchingServiceTwoDevices) {
  BluetoothDevice* le = AddLeDevice();
  BluetoothDevice* dual = AddDualDevice();

  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(&discovery_filter, kHeartRateServiceUUID);

  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(DeviceToUUIDs({{le, UUIDSet({kHeartRateServiceUUID})},
                           {dual, UUIDSet({kHeartRateServiceUUID})}}),
            result);
}

TEST_F(BluetoothGattBlueZTest,
       RetrieveGattConnectedDevicesWithDiscoveryFilter_TwoServicesTwoDevices) {
  BluetoothDevice* le = AddLeDevice();
  BluetoothDevice* dual = AddDualDevice();

  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  AddDeviceFilterWithUUID(&discovery_filter, kGenericAccessServiceUUID);
  AddDeviceFilterWithUUID(&discovery_filter, kHeartRateServiceUUID);

  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(DeviceToUUIDs({{le, UUIDSet({kHeartRateServiceUUID})},
                           {dual, UUIDSet({kHeartRateServiceUUID,
                                           kGenericAccessServiceUUID})}}),
            result);
}

TEST_F(
    BluetoothGattBlueZTest,
    RetrieveGattConnectedDevicesWithDiscoveryFilter_OneMatchingServiceOneNonMatchingService) {
  AddLeDevice();
  BluetoothDevice* dual = AddDualDevice();
  BluetoothDiscoveryFilter discovery_filter(device::BLUETOOTH_TRANSPORT_LE);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(kGenericAccessServiceUUID);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter2;
  device_filter2.uuids.insert(kBatteryServiceUUID);
  discovery_filter.AddDeviceFilter(device_filter);
  discovery_filter.AddDeviceFilter(device_filter2);

  DeviceToUUIDs result =
      adapter_->RetrieveGattConnectedDevicesWithDiscoveryFilter(
          discovery_filter);

  EXPECT_EQ(DeviceToUUIDs({{dual, UUIDSet({kGenericAccessServiceUUID})}}),
            result);
}

TEST_F(BluetoothGattBlueZTest, GattConnection) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->IsConnected());
  ASSERT_FALSE(device->IsGattConnected());
  ASSERT_FALSE(gatt_conn_.get());
  ASSERT_EQ(0, success_callback_count_);
  ASSERT_EQ(0, error_callback_count_);

  device->CreateGattConnection(
      base::BindOnce(&BluetoothGattBlueZTest::GattConnectionCallback,
                     base::Unretained(this)));

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_TRUE(gatt_conn_->IsConnected());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress,
            gatt_conn_->GetDeviceAddress());

  gatt_conn_->Disconnect();
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
  EXPECT_FALSE(gatt_conn_->IsConnected());

  device->CreateGattConnection(
      base::BindOnce(&BluetoothGattBlueZTest::GattConnectionCallback,
                     base::Unretained(this)));

  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_TRUE(gatt_conn_->IsConnected());

  device->Disconnect(base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                                    base::Unretained(this)),
                     base::BindOnce(&BluetoothGattBlueZTest::ErrorCallback,
                                    base::Unretained(this)));

  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_FALSE(gatt_conn_->IsConnected());

  device->CreateGattConnection(
      base::BindOnce(&BluetoothGattBlueZTest::GattConnectionCallback,
                     base::Unretained(this)));

  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_TRUE(gatt_conn_->IsConnected());

  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_FALSE(gatt_conn_->IsConnected());
}

TEST_F(BluetoothGattBlueZTest, GattServiceAddedAndRemoved) {
  // Create a fake LE device. We store the device pointer here because this is a
  // test. It's unsafe to do this in production as the device might get deleted.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.gatt_service_added_count());
  EXPECT_EQ(0, observer.gatt_service_removed_count());
  EXPECT_TRUE(observer.last_gatt_service_id().empty());
  EXPECT_FALSE(observer.last_gatt_service_uuid().IsValid());
  EXPECT_TRUE(device->GetGattServices().empty());

  // Expose the fake Heart Rate Service.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.gatt_service_added_count());
  EXPECT_EQ(0, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            observer.last_gatt_service_uuid());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());
  EXPECT_TRUE(service->IsPrimary());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));

  EXPECT_EQ(observer.last_gatt_service_uuid(), service->GetUUID());

  // Hide the service.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_gatt_service_client_->HideHeartRateService();

  EXPECT_EQ(1, observer.gatt_service_added_count());
  EXPECT_EQ(1, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_TRUE(device->GetGattServices().empty());
  EXPECT_EQ(BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            observer.last_gatt_service_uuid());

  EXPECT_EQ(NULL, device->GetGattService(observer.last_gatt_service_id()));

  // Expose the service again.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  EXPECT_EQ(2, observer.gatt_service_added_count());
  EXPECT_EQ(1, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            observer.last_gatt_service_uuid());

  // The object |service| points to should have been deallocated. |device|
  // should contain a brand new instance.
  service = device->GetGattService(observer.last_gatt_service_id());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(observer.last_gatt_service_uuid(), service->GetUUID());

  // Remove the device. The observer should be notified of the removed service.
  // |device| becomes invalid after this.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  EXPECT_EQ(2, observer.gatt_service_added_count());
  EXPECT_EQ(2, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            observer.last_gatt_service_uuid());
  EXPECT_EQ(NULL, adapter_->GetDevice(
                      bluez::FakeBluetoothDeviceClient::kLowEnergyAddress));
}

TEST_F(BluetoothGattBlueZTest, ServicesDiscoveredBeforeAdapterIsCreated) {
  // Tests that all GATT objects are created for a device whose D-Bus objects
  // were already exposed and for which services have been resolved.
  adapter_.reset();
  ASSERT_FALSE(device::BluetoothAdapterFactory::HasSharedInstanceForTesting());

  // Create the adapter. This should create all the GATT objects.
  GetAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible()) {
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  properties->services_resolved.ReplaceValue(true);


  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);

  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  HeartRateServiceShouldBeComplete(device);
}

TEST_F(BluetoothGattBlueZTest, ServicesDiscoveredAfterAdapterIsCreated) {
  // Create a fake LE device. We store the device pointer here because this is a
  // test. It's unsafe to do this in production as the device might get deleted.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate Service.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  // Run the message loop so that the characteristics/descriptors appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }
  properties->services_resolved.ReplaceValue(true);

  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1u, device->GetGattServices().size());
  EXPECT_EQ(device, observer.last_device());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress,
            observer.last_device_address());

  // Disconnect from the device:
  device->Disconnect(base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                                    base::Unretained(this)),
                     base::BindOnce(&BluetoothGattBlueZTest::ErrorCallback,
                                    base::Unretained(this)));
  fake_bluetooth_gatt_service_client_->HideHeartRateService();
  properties->connected.ReplaceValue(false);
  properties->services_resolved.ReplaceValue(false);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());

  // Verify that the device can be connected to again:
  device->CreateGattConnection(
      base::BindOnce(&BluetoothGattBlueZTest::GattConnectionCallback,
                     base::Unretained(this)));
  properties->connected.ReplaceValue(true);
  EXPECT_TRUE(device->IsConnected());

  // Verify that service discovery can be done again:
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  // Run the message loop so that the characteristics/descriptors appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }
  properties->services_resolved.ReplaceValue(true);

  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1u, device->GetGattServices().size());
}

TEST_F(BluetoothGattBlueZTest, DiscoverCachedServices) {
  // This unit test tests that all remote GATT objects are created for D-Bus
  // objects that were already exposed and all relevant events have been
  // dispatched.
  adapter_.reset();
  ASSERT_FALSE(device::BluetoothAdapterFactory::HasSharedInstanceForTesting());

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  properties->services_resolved.ReplaceValue(false);

  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible()) {
    base::RunLoop().RunUntilIdle();
  }

  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  // Create the adapter. This should create all the GATT objects.
  GetAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // The device should exist but contain no cached services while the services
  // haven't been resolved.
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);

  EXPECT_TRUE(device->GetGattServices().empty());

  EXPECT_EQ(0, observer.gatt_services_discovered_count());
  EXPECT_EQ(0, observer.gatt_service_added_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_EQ(0, observer.gatt_characteristic_added_count());
  EXPECT_EQ(0, observer.gatt_descriptor_added_count());

  properties->services_resolved.ReplaceValue(true);

  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  EXPECT_EQ(1, observer.gatt_service_added_count());
  EXPECT_EQ(1, observer.gatt_discovery_complete_count());
  EXPECT_EQ(3, observer.gatt_characteristic_added_count());
  EXPECT_EQ(1, observer.gatt_descriptor_added_count());

  HeartRateServiceShouldBeComplete(device);
}

TEST_F(BluetoothGattBlueZTest, DiscoverNewServices) {
  // This unit test tests that all remote GATT Objects are created for D-Bus
  // objects that are newly exposed.
  TestBluetoothAdapterObserver observer(adapter_);

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  properties->services_resolved.ReplaceValue(false);
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible()) {
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  // The device should exist and contain new services even though the services
  // haven't been resolved.
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  EXPECT_EQ(1u, device->GetGattServices().size());
  HeartRateServiceShouldBeComplete(device);

  EXPECT_EQ(0, observer.gatt_services_discovered_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  // The rest of the events are tested in the corresponding
  // {GATTObject}AddedAndRemoved tests.

  properties->services_resolved.ReplaceValue(true);

  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  EXPECT_EQ(1, observer.gatt_discovery_complete_count());
}

TEST_F(BluetoothGattBlueZTest, DiscoverCachedAndNewServices) {
  // This unit test tests that all remote GATT objects are created for D-Bus
  // objects that were already exposed and for new GATT Objects.
  adapter_.reset();
  ASSERT_FALSE(device::BluetoothAdapterFactory::HasSharedInstanceForTesting());

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  properties->services_resolved.ReplaceValue(false);

  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible()) {
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  // Create the adapter. This should create all the GATT objects.
  GetAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // The device should exist but contain no cached services while the services
  // haven't been resolved.
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);

  EXPECT_EQ(0, observer.gatt_services_discovered_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_TRUE(device->GetGattServices().empty());

  fake_bluetooth_gatt_service_client_->ExposeBatteryService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsBatteryServiceVisible());

  EXPECT_EQ(0, observer.gatt_services_discovered_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_EQ(1u, device->GetGattServices().size());

  // A newly added service should be complete even if services haven't been
  // resolved.
  BatteryServiceShouldBeComplete(device);

  properties->services_resolved.ReplaceValue(true);

  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  EXPECT_EQ(2, observer.gatt_discovery_complete_count());
  EXPECT_EQ(2u, device->GetGattServices().size());

  // Now that services have been resolved both services should be present.
  HeartRateServiceShouldBeComplete(device);
  BatteryServiceShouldBeComplete(device);
}

TEST_F(BluetoothGattBlueZTest, GattCharacteristicAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_EQ(0, observer.gatt_characteristic_added_count());
  EXPECT_EQ(0, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // 3 characteristics should appear. Only 1 of the characteristics sends
  // value changed signals. Service changed should be fired once for
  // descriptor added.
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(3, observer.gatt_characteristic_added_count());
  EXPECT_EQ(0, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the characteristics. 3 removed signals should be received.
  fake_bluetooth_gatt_characteristic_client_->HideHeartRateCharacteristics();
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(3, observer.gatt_characteristic_added_count());
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Re-expose the heart rate characteristics. We shouldn't get another
  // GattDiscoveryCompleteForService call, since the service thinks that
  // discovery is done. On the bluetoothd side, characteristics will be removed
  // only if the service will also be subsequently removed.
  fake_bluetooth_gatt_characteristic_client_->ExposeHeartRateCharacteristics(
      fake_bluetooth_gatt_service_client_->GetHeartRateServicePath());
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(6, observer.gatt_characteristic_added_count());
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the service. All characteristics should disappear.
  fake_bluetooth_gatt_service_client_->HideHeartRateService();
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(6, observer.gatt_characteristic_added_count());
  EXPECT_EQ(6, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}

TEST_F(BluetoothGattBlueZTest, GattDescriptorAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_added_count());
  EXPECT_EQ(0, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_EQ(0, observer.gatt_service_changed_count());

  // Only the Heart Rate Measurement characteristic has a descriptor.
  EXPECT_EQ(1, observer.gatt_descriptor_added_count());
  EXPECT_EQ(0, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetBodySensorLocationPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptors()[0];
  EXPECT_EQ(
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid(),
      descriptor->GetUUID());
  EXPECT_EQ(descriptor->GetUUID(), observer.last_gatt_descriptor_uuid());
  EXPECT_EQ(descriptor->GetIdentifier(), observer.last_gatt_descriptor_id());

  // Hide the descriptor.
  fake_bluetooth_gatt_descriptor_client_->HideDescriptor(
      dbus::ObjectPath(descriptor->GetIdentifier()));
  EXPECT_TRUE(characteristic->GetDescriptors().empty());
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_added_count());
  EXPECT_EQ(1, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  // Expose the descriptor again.
  observer.last_gatt_descriptor_id().clear();
  observer.last_gatt_descriptor_uuid() = BluetoothUUID();
  fake_bluetooth_gatt_descriptor_client_->ExposeDescriptor(
      dbus::ObjectPath(characteristic->GetIdentifier()),
      bluez::FakeBluetoothGattDescriptorClient::
          kClientCharacteristicConfigurationUUID);
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());
  EXPECT_EQ(2, observer.gatt_descriptor_added_count());
  EXPECT_EQ(1, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  descriptor = characteristic->GetDescriptors()[0];
  EXPECT_EQ(
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid(),
      descriptor->GetUUID());
  EXPECT_EQ(descriptor->GetUUID(), observer.last_gatt_descriptor_uuid());
  EXPECT_EQ(descriptor->GetIdentifier(), observer.last_gatt_descriptor_id());
}

TEST_F(BluetoothGattBlueZTest, GattCharacteristicValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Issue write request to non-writable characteristics.
  observer.Reset();

  std::vector<uint8_t> write_value;
  write_value.push_back(0x01);
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateMeasurementUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value, WriteType::kWithResponse,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotSupported,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value, WriteType::kWithResponse,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotPermitted,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue write request to writable characteristic. The "Body Sensor Location"
  // characteristic does not send notifications and WriteValue does not result
  // in a CharacteristicValueChanged event, thus no such event should be
  // received.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateControlPointUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value, WriteType::kWithResponse,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue some invalid write requests to the characteristic.
  // The value should still not change.

  std::vector<uint8_t> invalid_write_length;
  invalid_write_length.push_back(0x01);
  invalid_write_length.push_back(0x00);
  characteristic->WriteRemoteCharacteristic(
      invalid_write_length, WriteType::kWithResponse,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(3, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kInvalidLength,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  std::vector<uint8_t> invalid_write_value;
  invalid_write_value.push_back(0x02);
  characteristic->WriteRemoteCharacteristic(
      invalid_write_value, WriteType::kWithResponse,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kFailed, last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue a read request.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));

  // Test long-running actions.
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(1);
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));

  // Callback counts shouldn't change, this one will be delayed until after
  // tne next one.
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Next read should error because IN_PROGRESS
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(5, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kInProgress,
            last_service_error_);

  // But previous call finished.
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(0);

  // Test unauthorized actions.
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(false);
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(6, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotAuthorized,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(true);

  // Test unauthenticated / needs login.
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(false);
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(7, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotPaired,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(true);
}

TEST_F(BluetoothGattBlueZTest, DeprecatedGattCharacteristicValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Issue write request to non-writable characteristics.
  observer.Reset();

  std::vector<uint8_t> write_value;
  write_value.push_back(0x01);
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateMeasurementUUID, characteristic->GetUUID());
  characteristic->DeprecatedWriteRemoteCharacteristic(
      write_value,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotSupported,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->DeprecatedWriteRemoteCharacteristic(
      write_value,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotPermitted,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue write request to writable characteristic. The "Body Sensor Location"
  // characteristic does not send notifications and WriteValue does not result
  // in a CharacteristicValueChanged event, thus no such event should be
  // received.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateControlPointUUID, characteristic->GetUUID());
  characteristic->DeprecatedWriteRemoteCharacteristic(
      write_value,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue some invalid write requests to the characteristic.
  // The value should still not change.

  std::vector<uint8_t> invalid_write_length;
  invalid_write_length.push_back(0x01);
  invalid_write_length.push_back(0x00);
  characteristic->DeprecatedWriteRemoteCharacteristic(
      invalid_write_length,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(3, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kInvalidLength,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  std::vector<uint8_t> invalid_write_value;
  invalid_write_value.push_back(0x02);
  characteristic->DeprecatedWriteRemoteCharacteristic(
      invalid_write_value,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kFailed, last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue a read request.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));

  // Test long-running actions.
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(1);
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));

  // Callback counts shouldn't change, this one will be delayed until after
  // tne next one.
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Next read should error because IN_PROGRESS
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(5, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kInProgress,
            last_service_error_);

  // But previous call finished.
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(0);

  // Test unauthorized actions.
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(false);
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(6, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotAuthorized,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(true);

  // Test unauthenticated / needs login.
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(false);
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(7, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotPaired,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(true);
}

// Test a read request issued from the success callback of another read request.
TEST_F(BluetoothGattBlueZTest, GattCharacteristicValue_Nested_Read_Read) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain readable Body Sensor Location characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetBodySensorLocationPath()
                                     .value());

  characteristic->ReadRemoteCharacteristic(base::BindLambdaForTesting(
      [&](std::optional<BluetoothGattService::GattErrorCode> error_code,
          const std::vector<uint8_t>& data) {
        ValueCallback(error_code, data);
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(characteristic->GetValue(), last_read_value_);

        characteristic->ReadRemoteCharacteristic(
            base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                           base::Unretained(this)));
      }));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(characteristic->GetValue(), last_read_value_);
}

// Test a write request issued from the success callback of another write
// request.
TEST_F(BluetoothGattBlueZTest, GattCharacteristicValue_Nested_Write_Write) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain writable Heart Rate Control Point characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateControlPointPath()
                                     .value());

  std::vector<uint8_t> write_value = {0x01};
  characteristic->WriteRemoteCharacteristic(
      write_value, WriteType::kWithResponse, base::BindLambdaForTesting([&] {
        SuccessCallback();
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);

        characteristic->WriteRemoteCharacteristic(
            write_value, WriteType::kWithResponse,
            base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                           base::Unretained(this)),
            base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                           base::Unretained(this)));
      }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

// Test a write request issued from the success callback of another write
// request.
TEST_F(BluetoothGattBlueZTest,
       GattCharacteristicValue_Nested_DeprecatedWrite_DeprecatedWrite) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain writable Heart Rate Control Point characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateControlPointPath()
                                     .value());

  std::vector<uint8_t> write_value = {0x01};
  characteristic->DeprecatedWriteRemoteCharacteristic(
      write_value, base::BindLambdaForTesting([&] {
        SuccessCallback();
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);

        characteristic->DeprecatedWriteRemoteCharacteristic(
            write_value,
            base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                           base::Unretained(this)),
            base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                           base::Unretained(this)));
      }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

// Test a write request issued from the success callback of a read request.
TEST_F(BluetoothGattBlueZTest, GattCharacteristicValue_Nested_Read_Write) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain readable Body Sensor Location characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetBodySensorLocationPath()
                                     .value());

  characteristic->ReadRemoteCharacteristic(base::BindLambdaForTesting(
      [&](std::optional<BluetoothGattService::GattErrorCode> error_code,
          const std::vector<uint8_t>& data) {
        ValueCallback(error_code, data);
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(characteristic->GetValue(), last_read_value_);

        // Obtain writable Heart Rate Control Point characteristic.
        characteristic = service->GetCharacteristic(
            fake_bluetooth_gatt_characteristic_client_
                ->GetHeartRateControlPointPath()
                .value());

        characteristic->WriteRemoteCharacteristic(
            std::vector<uint8_t>({0x01}), WriteType::kWithResponse,
            base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                           base::Unretained(this)),
            base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                           base::Unretained(this)));
      }));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

// Test a write request issued from the success callback of a read request.
TEST_F(BluetoothGattBlueZTest,
       GattCharacteristicValue_Nested_Read_DeprecatedWrite) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain readable Body Sensor Location characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetBodySensorLocationPath()
                                     .value());

  characteristic->ReadRemoteCharacteristic(base::BindLambdaForTesting(
      [&](std::optional<BluetoothGattService::GattErrorCode> error_code,
          const std::vector<uint8_t>& data) {
        ValueCallback(error_code, data);
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(characteristic->GetValue(), last_read_value_);

        // Obtain writable Heart Rate Control Point characteristic.
        characteristic = service->GetCharacteristic(
            fake_bluetooth_gatt_characteristic_client_
                ->GetHeartRateControlPointPath()
                .value());

        characteristic->DeprecatedWriteRemoteCharacteristic(
            std::vector<uint8_t>({0x01}),
            base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                           base::Unretained(this)),
            base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                           base::Unretained(this)));
      }));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

// Test a read request issued from the success callback of a write request.
TEST_F(BluetoothGattBlueZTest, GattCharacteristicValue_Nested_Write_Read) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain writable Heart Rate Control Point characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateControlPointPath()
                                     .value());

  characteristic->WriteRemoteCharacteristic(
      std::vector<uint8_t>({0x01}), WriteType::kWithResponse,
      base::BindLambdaForTesting([&] {
        SuccessCallback();
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);

        // Obtain readable Body Sensor Location characteristic.
        characteristic = service->GetCharacteristic(
            fake_bluetooth_gatt_characteristic_client_
                ->GetBodySensorLocationPath()
                .value());

        characteristic->ReadRemoteCharacteristic(
            base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                           base::Unretained(this)));
      }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(characteristic->GetValue(), last_read_value_);
}

// Test a read request issued from the success callback of a write request.
TEST_F(BluetoothGattBlueZTest,
       GattCharacteristicValue_Nested_DeprecatedWrite_Read) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  // Obtain writable Heart Rate Control Point characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateControlPointPath()
                                     .value());

  characteristic->DeprecatedWriteRemoteCharacteristic(
      std::vector<uint8_t>({0x01}), base::BindLambdaForTesting([&] {
        SuccessCallback();
        EXPECT_EQ(1, success_callback_count_);
        EXPECT_EQ(0, error_callback_count_);

        // Obtain readable Body Sensor Location characteristic.
        characteristic = service->GetCharacteristic(
            fake_bluetooth_gatt_characteristic_client_
                ->GetBodySensorLocationPath()
                .value());

        characteristic->ReadRemoteCharacteristic(
            base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                           base::Unretained(this)));
      }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(characteristic->GetValue(), last_read_value_);
}

TEST_F(BluetoothGattBlueZTest, GattCharacteristicProperties) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetBodySensorLocationPath()
                                     .value());
  EXPECT_EQ(BluetoothRemoteGattCharacteristic::PROPERTY_READ,
            characteristic->GetProperties());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateControlPointPath()
          .value());
  EXPECT_EQ(BluetoothRemoteGattCharacteristic::PROPERTY_WRITE,
            characteristic->GetProperties());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value());
  EXPECT_EQ(BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
                BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE,
            static_cast<BluetoothRemoteGattCharacteristic::Property>(
                characteristic->GetProperties()));
}

TEST_F(BluetoothGattBlueZTest, GattDescriptorValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_EQ(0, observer.gatt_service_changed_count());

  // Only the Heart Rate Measurement characteristic has a descriptor.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());
  EXPECT_FALSE(characteristic->IsNotifying());

  BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptors()[0];
  EXPECT_EQ(
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid(),
      descriptor->GetUUID());

  std::vector<uint8_t> desc_value = {0x00, 0x00};

  /* The cached value will be empty until the first read request */
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_TRUE(descriptor->GetValue().empty());

  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_read_value_.empty());

  // Read value. GattDescriptorValueChanged event will be sent after a
  // successful read.
  descriptor->ReadRemoteDescriptor(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_TRUE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Write value. Writes to this descriptor will fail.
  desc_value[0] = 0x03;
  descriptor->WriteRemoteDescriptor(
      desc_value,
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GattErrorCode::kNotPermitted,
            last_service_error_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Read value. The value should remain unchanged.
  descriptor->ReadRemoteDescriptor(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Start notifications on the descriptor's characteristic. The descriptor
  // value should change.
  base::RunLoop loop;
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  loop.Run();

  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Read the new descriptor value. We should receive a value updated event.
  descriptor->ReadRemoteDescriptor(
      base::BindOnce(&BluetoothGattBlueZTest::ValueCallback,
                     base::Unretained(this)));
  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(2, observer.gatt_descriptor_value_changed_count());
}

TEST_F(BluetoothGattBlueZTest, NotifySessions) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());
  base::RunLoop loop1;
  base::RunLoop loop2;
  base::RunLoop loop3;
  // Request to start notifications.
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop1, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop1.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));

  // The operation still hasn't completed but we should have received the first
  // notification.
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());

  // Send a two more requests, which should get queued.
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop2, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop2.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop3, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop3.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());
  EXPECT_FALSE(characteristic->IsNotifying());

  // Run the main loop. The initial call should complete. The queued call should
  // succeed immediately.
  loop1.Run();
  loop2.Run();
  loop3.Run();

  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, update_sessions_.size());

  // Notifications should be getting sent regularly now.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_GT(observer.gatt_characteristic_value_changed_count(), 1);

  // Stop one of the sessions. The session should become inactive but the
  // characteristic should still be notifying.
  BluetoothGattNotifySession* session = update_sessions_[0].get();
  EXPECT_TRUE(session->IsActive());
  session->Stop(base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                               base::Unretained(this)));

  // Run message loop to stop the notify session.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(session->IsActive());
  EXPECT_EQ(characteristic->GetIdentifier(),
            session->GetCharacteristicIdentifier());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Delete another session. Characteristic should still be notifying.
  update_sessions_.pop_back();
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_FALSE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(update_sessions_[1]->IsActive());

  // Clear the last session.
  update_sessions_.clear();
  EXPECT_TRUE(update_sessions_.empty());

  // Run message loop in order to do proper cleanup of sessions.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(characteristic->IsNotifying());

  success_callback_count_ = 0;
  observer.Reset();
  base::RunLoop loop4;
  // Enable notifications again.
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop4, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop4.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());
  EXPECT_FALSE(characteristic->IsNotifying());

  // Run the message loop. Notifications should begin.
  loop4.Run();

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Check that notifications are happening.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_GT(observer.gatt_characteristic_value_changed_count(), 1);

  base::RunLoop loop5;
  // Request another session. This should return immediately.
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop5, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop5.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));

  // Run message loop to stop the notify session.
  loop5.Run();

  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(update_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Hide the characteristic. The sessions should become inactive.
  fake_bluetooth_gatt_characteristic_client_->HideHeartRateCharacteristics();
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_FALSE(update_sessions_[0]->IsActive());
  EXPECT_FALSE(update_sessions_[1]->IsActive());
}

TEST_F(BluetoothGattBlueZTest, NotifySessionsMadeInactive) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());
  base::RunLoop loop1;
  // Send several requests to start notifications.
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop1, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop1.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  base::RunLoop loop2;
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop2, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop2.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  base::RunLoop loop3;
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop3, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop3.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  base::RunLoop loop4;
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop4, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop4.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));

  // The operation still hasn't completed but we should have received the first
  // notification.
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  // Run the main loop. The initial call should complete. The queued calls
  // should succeed immediately.
  loop1.Run();
  loop2.Run();
  loop3.Run();
  loop4.Run();

  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_EQ(4U, update_sessions_.size());

  for (int i = 0; i < 4; i++)
    EXPECT_TRUE(update_sessions_[0]->IsActive());

  // Stop notifications directly through the client. The sessions should get
  // marked as inactive.
  fake_bluetooth_gatt_characteristic_client_->StopNotify(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath(),
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(5, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Run message loop to stop the notify session.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_EQ(4U, update_sessions_.size());

  for (int i = 0; i < 4; i++)
    EXPECT_FALSE(update_sessions_[0]->IsActive());

  // It should be possible to restart notifications and the call should reset
  // the session count and make a request through the client.
  update_sessions_.clear();
  success_callback_count_ = 0;
  observer.Reset();
  base::RunLoop loop5;
  characteristic->StartNotifySession(
      base::BindLambdaForTesting(
          [&loop5, this](std::unique_ptr<BluetoothGattNotifySession> session) {
            NotifySessionCallback(std::move(session));
            loop5.Quit();
          }),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));

  // Run message loop to start the notify session.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  loop5.Run();

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothGattBlueZTest, ReliableWrite) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDeviceBlueZ* device = static_cast<BluetoothDeviceBlueZ*>(
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress));
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  base::RunLoop loop;
  // Request to start notifications.
  service
      ->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                              ->GetHeartRateMeasurementPath()
                              .value())
      ->StartNotifySession(
          base::BindLambdaForTesting(
              [&loop,
               this](std::unique_ptr<BluetoothGattNotifySession> session) {
                NotifySessionCallback(std::move(session));
                loop.Quit();
              }),
          base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                         base::Unretained(this)));
  loop.Run();

  // Obtain writable Heart Rate Control Point characteristic.
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateControlPointPath()
                                     .value());
  std::vector<uint8_t> write_value = {0x01};

  // Prepare 1000 writes.
  success_callback_count_ = 0;
  error_callback_count_ = 0;
  observer.Reset();
  for (int i = 0; i < 1000; ++i) {
    characteristic->PrepareWriteRemoteCharacteristic(
        write_value,
        base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                       base::Unretained(this)),
        base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                       base::Unretained(this)));
  }
  EXPECT_EQ(1000, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Abort.
  device->AbortWrite(
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1001, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Prepare another 1000 writes.
  success_callback_count_ = 0;
  error_callback_count_ = 0;
  observer.Reset();
  for (int i = 0; i < 1000; ++i) {
    characteristic->PrepareWriteRemoteCharacteristic(
        write_value,
        base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                       base::Unretained(this)),
        base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                       base::Unretained(this)));
  }
  EXPECT_EQ(1000, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Execute.
  device->ExecuteWrite(
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                     base::Unretained(this)));
  EXPECT_EQ(1001, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1000, observer.gatt_characteristic_value_changed_count());
}

TEST_F(BluetoothGattBlueZTest, NotificationType) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDeviceBlueZ* device = static_cast<BluetoothDeviceBlueZ*>(
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress));
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothRemoteGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  // Run the message loop so that the characteristics appear.
  {
    base::RunLoop loop;
    observer.set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_
                                     ->GetHeartRateMeasurementPath()
                                     .value());

  // Request to start notifications.
  {
    base::RunLoop loop;
    characteristic->StartNotifySession(
        device::BluetoothGattCharacteristic::NotificationType::kNotification,
        base::BindLambdaForTesting(
            [&loop, this](std::unique_ptr<BluetoothGattNotifySession> session) {
              NotifySessionCallback(std::move(session));
              loop.Quit();
            }),
        base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                       base::Unretained(this)));
    loop.Run();
  }
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());

  // Request to start indications.
  fake_bluetooth_gatt_characteristic_client_->StopNotify(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath(),
      base::BindOnce(&BluetoothGattBlueZTest::SuccessCallback,
                     base::Unretained(this)),
      base::BindOnce(&BluetoothGattBlueZTest::DBusErrorCallback,
                     base::Unretained(this)));
  {
    base::RunLoop loop;
    characteristic->StartNotifySession(
        device::BluetoothGattCharacteristic::NotificationType::kIndication,
        base::BindLambdaForTesting(
            [&loop, this](std::unique_ptr<BluetoothGattNotifySession> session) {
              NotifySessionCallback(std::move(session));
              loop.Quit();
            }),
        base::BindOnce(&BluetoothGattBlueZTest::ServiceErrorCallback,
                       base::Unretained(this)));
    loop.Run();
  }
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(BluetoothGattBlueZTest, MultipleDevices) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties1 =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  properties1->services_resolved.ReplaceValue(false);

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible()) {
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDualPath));
  bluez::FakeBluetoothDeviceClient::Properties* properties2 =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDualPath));
  properties2->services_resolved.ReplaceValue(false);

  fake_bluetooth_gatt_service_client_->ExposeBatteryService(
      dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kDualPath));
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsBatteryServiceVisible());

  BluetoothDeviceBlueZ* device1 = static_cast<BluetoothDeviceBlueZ*>(
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress));
  ASSERT_TRUE(device1);
  BluetoothDeviceBlueZ* device2 = static_cast<BluetoothDeviceBlueZ*>(
      adapter_->GetDevice(bluez::FakeBluetoothDeviceClient::kDualAddress));
  ASSERT_TRUE(device2);


  EXPECT_EQ(0, observer.gatt_discovery_complete_count());

  properties1->services_resolved.ReplaceValue(true);
  properties2->services_resolved.ReplaceValue(true);

  // Since BlueZ iterates all services for all devices for each device, this
  // can catch errors like https://crbug.com/1087648
  EXPECT_EQ(2, observer.gatt_discovery_complete_count());
}
}  // namespace bluez
