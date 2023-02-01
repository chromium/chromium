// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_descriptor_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_advertiser_client.h"
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"
#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"
#include "device/bluetooth/floss/fake_floss_lescan_client.h"
#include "device/bluetooth/floss/fake_floss_logging_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/fake_floss_socket_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/floss/fake_floss_admin_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
// Use this gatt client id for all interaction.
constexpr int kGattClientId = 39;

// Use this adapter when an adapter index is required for testing.
constexpr int kUseThisAdapter = 0;

// A fake service to search for that will show up.
constexpr char kFakeUuidShort[] = "1812";
}  // namespace

namespace floss {

using CharProperty = device::BluetoothGattCharacteristic::Property;

// Unit tests exercising GATT in device/bluetooth/floss implementations, with
// abstract Floss API implemented as a fake Floss*Client.
class BluetoothGattFlossTest : public testing::Test {
 public:
  void SetUp() override {
    // TODO(b/266989920): GetSetterForTesting method used as a shortcut to
    // initiate fake DBUS instances and fake clients. Replace this call with a
    // more proper init after Floss fake implement is completed.
    FlossDBusManager::GetSetterForTesting();

    // Always initialize and enable adapter for Gatt tests.
    InitializeAdapter();
    EnableAdapter();
    SetClientRegistered();
  }

  FakeFlossManagerClient* GetFakeManagerClient() {
    return static_cast<FakeFlossManagerClient*>(
        FlossDBusManager::Get()->GetManagerClient());
  }

  FakeFlossAdapterClient* GetFakeAdapterClient() {
    return static_cast<FakeFlossAdapterClient*>(
        floss::FlossDBusManager::Get()->GetAdapterClient());
  }

  FakeFlossGattManagerClient* GetFakeGattManagerClient() {
    return static_cast<FakeFlossGattManagerClient*>(
        FlossDBusManager::Get()->GetGattManagerClient());
  }

  void InitializeAdapter() {
    adapter_ = BluetoothAdapterFloss::CreateAdapter();

    GetFakeManagerClient()->SetAdapterPowered(/*adapter=*/kUseThisAdapter,
                                              /*powered=*/true);

    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Simulate adapter enabled event. After adapter is enabled, there are known
  // devices.
  void EnableAdapter() {
    ASSERT_TRUE(adapter_.get() != nullptr);

    GetFakeManagerClient()->NotifyObservers(
        base::BindLambdaForTesting([](FlossManagerClient::Observer* observer) {
          observer->AdapterEnabledChanged(kUseThisAdapter,
                                          /*enabled=*/true);
        }));
    base::RunLoop().RunUntilIdle();
  }

  void DiscoverDevices() {
    ASSERT_TRUE(adapter_.get() != nullptr);

    adapter_->StartDiscoverySession(
        /*client_name=*/std::string(), base::DoNothing(), base::DoNothing());
  }

  void SetClientRegistered() {
    GetFakeGattManagerClient()->GattClientRegistered(GattStatus::kSuccess,
                                                     kGattClientId);
  }

  void SetAclConnectionState(std::string address, bool connected) {
    FlossDeviceId device;
    device.address = address;

    GetFakeAdapterClient()->NotifyObservers(base::BindLambdaForTesting(
        [&connected, &device](FlossAdapterClient::Observer* observer) {
          if (connected) {
            observer->AdapterDeviceConnected(device);
          } else {
            observer->AdapterDeviceDisconnected(device);
          }
        }));
  }

  void SetGattConnectionState(GattStatus status,
                              bool connected,
                              std::string address) {
    GetFakeGattManagerClient()->GattClientConnectionState(status, kGattClientId,
                                                          connected, address);
  }

  void SetGattSearchComplete(std::string address,
                             const std::vector<GattService>& services,
                             GattStatus status) {
    GetFakeGattManagerClient()->GattSearchComplete(address, services, status);
  }

  void SetGattConfigureMtu(std::string address,
                           int32_t mtu,
                           GattStatus status) {
    GetFakeGattManagerClient()->GattConfigureMtu(address, mtu, status);
  }

  GattService CreateFakeServiceFor(const device::BluetoothUUID& uuid) {
    GattService underlying_service;
    underlying_service.uuid = uuid;
    underlying_service.instance_id = 1;
    underlying_service.service_type = 0;

    return underlying_service;
  }

  // Keep variables public since these are tests and we don't need them
  // protected.

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
};

TEST_F(BluetoothGattFlossTest, ConnectAndResolveServices) {
  device::BluetoothDevice* paired_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  ASSERT_TRUE(paired_device != nullptr);

  base::RunLoop loop;

  // Create a gatt connection with full service discovery.
  paired_device->CreateGattConnection(
      base::BindLambdaForTesting(
          [&paired_device, &loop](
              std::unique_ptr<device::BluetoothGattConnection> conn,
              absl::optional<device::BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            EXPECT_TRUE(conn->IsConnected());
            EXPECT_EQ(paired_device->GetAddress(), conn->GetDeviceAddress());

            loop.Quit();
          }),
      /*service_uuid=*/absl::nullopt);

  // Fake a connection completion. First you should get the ACL connection
  // completed and then the GattConnectionState.
  SetAclConnectionState(paired_device->GetAddress(), /*connected=*/true);
  SetGattConnectionState(GattStatus::kSuccess, /*connected=*/true,
                         paired_device->GetAddress());

  EXPECT_TRUE(paired_device->IsConnected());
  EXPECT_FALSE(paired_device->IsGattServicesDiscoveryComplete());

  // Fake a service completion.
  SetGattSearchComplete(paired_device->GetAddress(), /*services=*/{},
                        GattStatus::kSuccess);

  EXPECT_TRUE(paired_device->IsConnected());
  EXPECT_TRUE(paired_device->IsGattServicesDiscoveryComplete());

  // Wait for callbacks to run.
  loop.RunUntilIdle();
}

TEST_F(BluetoothGattFlossTest, UpgradeToFullDiscovery) {
  device::BluetoothDevice* paired_device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);
  ASSERT_TRUE(paired_device != nullptr);

  base::RunLoop loop;

  device::BluetoothUUID fake_uuid(kFakeUuidShort);
  absl::optional<device::BluetoothUUID> fake_uuid_optional = fake_uuid;
  GattService fake_service = CreateFakeServiceFor(fake_uuid);

  // Create a gatt connection with partial service discovery.
  paired_device->CreateGattConnection(base::DoNothing(), fake_uuid_optional);

  // Fake a successful configure MTU.
  SetGattConfigureMtu(paired_device->GetAddress(), 500, GattStatus::kSuccess);

  // Fake a connection completion.
  SetGattConnectionState(GattStatus::kSuccess, /*connected=*/true,
                         paired_device->GetAddress());

  EXPECT_TRUE(paired_device->IsConnected());
  EXPECT_FALSE(paired_device->IsGattServicesDiscoveryComplete());

  // Fake a service completion with just a single entry.
  SetGattSearchComplete(paired_device->GetAddress(),
                        /*services=*/{fake_service}, GattStatus::kSuccess);

  EXPECT_TRUE(paired_device->IsConnected());
  EXPECT_FALSE(paired_device->IsGattServicesDiscoveryComplete());

  // Now try to upgrade to full discovery by connecting with no services.
  paired_device->CreateGattConnection(base::DoNothing(),
                                      /*service_uuid=*/absl::nullopt);
  EXPECT_FALSE(paired_device->IsGattServicesDiscoveryComplete());

  // Wait for discovery to complete again.
  SetGattSearchComplete(paired_device->GetAddress(),
                        /*services=*/{fake_service}, GattStatus::kSuccess);

  // Now we should be complete.
  EXPECT_TRUE(paired_device->IsGattServicesDiscoveryComplete());

  // Wait for callbacks to run.
  loop.RunUntilIdle();
}

TEST_F(BluetoothGattFlossTest, TranslateReadWriteAuthentication) {
  std::vector<std::pair<uint32_t, AuthRequired>> property_to_auth_read_map = {
      {CharProperty::PROPERTY_NONE, AuthRequired::kNoAuth},
      {CharProperty::PROPERTY_READ, AuthRequired::kNoAuth},

      {CharProperty::PROPERTY_READ_ENCRYPTED, AuthRequired::kNoMitm},
      {CharProperty::PROPERTY_READ_ENCRYPTED_AUTHENTICATED,
       AuthRequired::kReqMitm},

      // Use more restrictive requirement.
      {CharProperty::PROPERTY_READ_ENCRYPTED |
           CharProperty::PROPERTY_READ_ENCRYPTED_AUTHENTICATED,
       AuthRequired::kReqMitm},
  };

  std::vector<std::pair<uint32_t, AuthRequired>> property_to_auth_write_map = {
      {CharProperty::PROPERTY_NONE, AuthRequired::kNoAuth},
      {CharProperty::PROPERTY_WRITE, AuthRequired::kNoAuth},

      // Don't accept signed writes without authentication/encryption.
      {CharProperty::PROPERTY_AUTHENTICATED_SIGNED_WRITES,
       AuthRequired::kNoAuth},

      {CharProperty::PROPERTY_WRITE_ENCRYPTED, AuthRequired::kNoMitm},
      {CharProperty::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED,
       AuthRequired::kReqMitm},
      {CharProperty::PROPERTY_WRITE_ENCRYPTED |
           CharProperty::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED,
       AuthRequired::kReqMitm},

      {CharProperty::PROPERTY_WRITE_ENCRYPTED |
           CharProperty::PROPERTY_AUTHENTICATED_SIGNED_WRITES,
       AuthRequired::kSignedNoMitm},
      {CharProperty::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED |
           CharProperty::PROPERTY_AUTHENTICATED_SIGNED_WRITES,
       AuthRequired::kSignedReqMitm},
      {CharProperty::PROPERTY_WRITE_ENCRYPTED |
           CharProperty::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED |
           CharProperty::PROPERTY_AUTHENTICATED_SIGNED_WRITES,
       AuthRequired::kSignedReqMitm},
  };

  device::BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);

  GattService underlying_service;
  underlying_service.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_service.instance_id = 1;
  underlying_service.service_type = 0;

  auto service = BluetoothRemoteGattServiceFloss::Create(
      static_cast<BluetoothAdapterFloss*>(adapter_.get()),
      static_cast<BluetoothDeviceFloss*>(device), underlying_service, true);

  for (const auto& [props, auth] : property_to_auth_read_map) {
    GattCharacteristic tmp;
    tmp.uuid = device::BluetoothUUID("1912");
    tmp.instance_id = 2;
    tmp.properties = props;

    auto characteristic =
        BluetoothRemoteGattCharacteristicFloss::Create(service.get(), &tmp);

    EXPECT_EQ(characteristic->GetAuthForRead(), auth);
  }

  for (const auto& [props, auth] : property_to_auth_write_map) {
    GattCharacteristic tmp;
    tmp.uuid = device::BluetoothUUID("1912");
    tmp.instance_id = 2;
    tmp.properties = props;

    auto characteristic =
        BluetoothRemoteGattCharacteristicFloss::Create(service.get(), &tmp);

    EXPECT_EQ(characteristic->GetAuthForWrite(), auth);
  }
}

TEST_F(BluetoothGattFlossTest, VerifyAllIdentifiers) {
  device::BluetoothDevice* device =
      adapter_->GetDevice(FakeFlossAdapterClient::kBondedAddress1);

  GattService underlying_service;
  underlying_service.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_service.instance_id = 16;
  underlying_service.service_type = 0;

  auto service = BluetoothRemoteGattServiceFloss::Create(
      static_cast<BluetoothAdapterFloss*>(adapter_.get()),
      static_cast<BluetoothDeviceFloss*>(device), underlying_service, true);
  EXPECT_EQ(service->GetIdentifier(),
            base::StringPrintf("%s/%d", device->GetAddress().c_str(), 16));

  GattCharacteristic underlying_characteristic;
  underlying_characteristic.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_characteristic.instance_id = 47;

  auto characteristic = BluetoothRemoteGattCharacteristicFloss::Create(
      service.get(), &underlying_characteristic);
  EXPECT_EQ(
      characteristic->GetIdentifier(),
      base::StringPrintf("%s/%d/%d", device->GetAddress().c_str(), 16, 47));

  GattDescriptor underlying_descriptor;
  underlying_descriptor.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_descriptor.instance_id = 72;

  auto descriptor = BluetoothRemoteGattDescriptorFloss::Create(
      service.get(), characteristic.get(), &underlying_descriptor);
  EXPECT_EQ(descriptor->GetIdentifier(),
            base::StringPrintf("%s/%d/%d/%d", device->GetAddress().c_str(), 16,
                               47, 72));
}

}  // namespace floss
