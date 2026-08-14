// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_descriptor_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Use this gatt client id for all interaction.
constexpr int kGattClientId = 39;

// Use this adapter when an adapter index is required for testing.
constexpr int kUseThisAdapter = 0;

// A fake service to search for that will show up.
constexpr char kFakeUuidShort[] = "1812";
}  // namespace

namespace floss {

using FlossCharacteristic = floss::GattCharacteristic;

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

    GetFakeManagerClient()->SetDefaultEnabled(true);

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
    GetFakeAdapterClient()->SetConnected(
        FakeFlossAdapterClient::kBondedAddress1, true);
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

  // Simulate the D-Bus OnExecuteWrite callback from flossd, i.e. what arrives
  // in the browser process when a remote BLE central sends a single ATT
  // Execute Write Request (opcode 0x18).
  void SimulateGattServerExecuteWrite(std::string address,
                                      int32_t request_id,
                                      bool execute_write) {
    GetFakeGattManagerClient()->GattServerExecuteWrite(address, request_id,
                                                       execute_write);
  }

  void SimulateGattServerCharacteristicWriteRequest(
      std::string address,
      int32_t request_id,
      int32_t offset,
      int32_t length,
      bool is_prepared_write,
      bool needs_response,
      int32_t handle,
      std::vector<uint8_t> value) {
    GetFakeGattManagerClient()->GattServerCharacteristicWriteRequest(
        address, request_id, offset, length, is_prepared_write, needs_response,
        handle, value);
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
              std::optional<device::BluetoothDevice::ConnectErrorCode> error) {
            EXPECT_FALSE(error.has_value());
            EXPECT_TRUE(conn->IsConnected());
            EXPECT_EQ(paired_device->GetAddress(), conn->GetDeviceAddress());

            loop.Quit();
          }),
      /*service_uuid=*/std::nullopt);

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

  device::BluetoothUUID fake_uuid(kFakeUuidShort);
  std::optional<device::BluetoothUUID> fake_uuid_optional = fake_uuid;
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
                                      /*service_uuid=*/std::nullopt);
  EXPECT_FALSE(paired_device->IsGattServicesDiscoveryComplete());

  // Wait for discovery to complete again.
  SetGattSearchComplete(paired_device->GetAddress(),
                        /*services=*/{fake_service}, GattStatus::kSuccess);

  // Now we should be complete.
  EXPECT_TRUE(paired_device->IsGattServicesDiscoveryComplete());

  // Wait for callbacks to run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothGattFlossTest, TranslateReadWriteAuthentication) {
  std::vector<std::pair<std::pair<uint32_t, uint32_t>, AuthRequired>>
      property_to_auth_read_map = {
          {{0, 0}, AuthRequired::kNoAuth},
          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_READ, 0},
           AuthRequired::kNoAuth},
          {{
               FlossCharacteristic::GATT_CHAR_PROP_BIT_READ,
               FlossCharacteristic::GATT_PERM_READ_ENCRYPTED,
           },
           AuthRequired::kNoMitm},
          {{
               FlossCharacteristic::GATT_CHAR_PROP_BIT_READ,
               FlossCharacteristic::GATT_PERM_READ_ENC_MITM,
           },
           AuthRequired::kReqMitm},

          // Use more restrictive requirement.
          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_READ,
            FlossCharacteristic::GATT_PERM_READ_ENCRYPTED |
                FlossCharacteristic::GATT_PERM_READ_ENC_MITM},
           AuthRequired::kReqMitm},
      };

  std::vector<std::pair<std::pair<uint32_t, uint32_t>, AuthRequired>>
      property_to_auth_write_map = {
          {{0, 0}, AuthRequired::kNoAuth},
          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE, 0},
           AuthRequired::kNoAuth},

          // Don't accept signed writes without authentication/encryption.
          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_SIGNED},
           AuthRequired::kNoAuth},

          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED},
           AuthRequired::kNoMitm},

          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM},
           AuthRequired::kReqMitm},

          // Prefer encrypted + authenticated over encrypted.
          {{
               FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
               FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED |
                   FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM,
           },
           AuthRequired::kReqMitm},

          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED |
                FlossCharacteristic::GATT_PERM_WRITE_SIGNED},
           AuthRequired::kSignedNoMitm},

          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM |
                FlossCharacteristic::GATT_PERM_WRITE_SIGNED},
           AuthRequired::kSignedReqMitm},

          {{FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
            FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED |
                FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM |
                FlossCharacteristic::GATT_PERM_WRITE_SIGNED},
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
      static_cast<BluetoothDeviceFloss*>(device), underlying_service);

  for (const auto& [pair, auth] : property_to_auth_read_map) {
    const auto& [props, perms] = pair;
    GattCharacteristic tmp;
    tmp.uuid = device::BluetoothUUID("1912");
    tmp.instance_id = 2;
    tmp.properties = props;
    tmp.permissions = perms;

    auto characteristic =
        BluetoothRemoteGattCharacteristicFloss::Create(service.get(), &tmp);

    EXPECT_EQ(characteristic->GetAuthForRead(), auth);
  }

  for (const auto& [pair, auth] : property_to_auth_write_map) {
    const auto& [props, perms] = pair;
    GattCharacteristic tmp;
    tmp.uuid = device::BluetoothUUID("1912");
    tmp.instance_id = 2;
    tmp.properties = props;
    tmp.permissions = perms;

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
      static_cast<BluetoothDeviceFloss*>(device), underlying_service);
  EXPECT_EQ(service->GetIdentifier(),
            base::StringPrintf("%s-%s/%04x", device->GetAddress().c_str(),
                               service->GetUUID().value().c_str(), 16));

  GattCharacteristic underlying_characteristic;
  underlying_characteristic.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_characteristic.instance_id = 47;

  auto characteristic = BluetoothRemoteGattCharacteristicFloss::Create(
      service.get(), &underlying_characteristic);
  EXPECT_EQ(characteristic->GetIdentifier(),
            base::StringPrintf("%s-%s/%04x/%04x", device->GetAddress().c_str(),
                               service->GetUUID().value().c_str(), 16, 47));

  GattDescriptor underlying_descriptor;
  underlying_descriptor.uuid = device::BluetoothUUID(kFakeUuidShort);
  underlying_descriptor.instance_id = 72;

  auto descriptor = BluetoothRemoteGattDescriptorFloss::Create(
      service.get(), characteristic.get(), &underlying_descriptor);
  EXPECT_EQ(
      descriptor->GetIdentifier(),
      base::StringPrintf("%s-%s/%04x/%04x/%04x", device->GetAddress().c_str(),
                         service->GetUUID().value().c_str(), 16, 47, 72));
}

// -----------------------------------------------------------------------------
// Security regression test:
// BluetoothGattServiceFloss::GattServerExecuteWrite iterates ALL of
// server_observer_by_handle_ instead of dispatching to a single handle.
// Combined with the unfiltered Layer-1 broadcast in
// FlossGattManagerClient::GattServerExecuteWrite, a single ATT Execute Write
// Request from an unpaired remote BLE peer reaches
// delegate->OnCharacteristicPrepareWriteRequest on EVERY characteristic in
// EVERY locally-registered GATT service -- including characteristics that were
// created with PROPERTY_READ / PERMISSION_READ only and have no write
// permission. The flossd-side per-handle write-permission check happens at the
// (handle-filtered) Prepare Write stage; Execute Write carries no handle and
// Chrome applies no equivalent filter, so per-characteristic write permissions
// are bypassed. The delegate is told to commit (has_subsequent_request=false)
// an empty write at offset 0.
// -----------------------------------------------------------------------------

namespace {

// Records every prepare-write event the delegate receives so the test can
// assert which characteristics were targeted by a single Execute Write.
class RecordingGattDelegate
    : public device::BluetoothLocalGattService::Delegate {
 public:
  struct PrepareWriteEvent {
    raw_ptr<const device::BluetoothLocalGattCharacteristic> characteristic;
    std::vector<uint8_t> value;
    int offset;
    bool has_subsequent_request;
  };

  void OnCharacteristicReadRequest(
      const device::BluetoothDevice*,
      const device::BluetoothLocalGattCharacteristic*,
      int,
      ValueCallback callback) override {
    std::move(callback).Run(std::nullopt, {});
  }
  void OnCharacteristicWriteRequest(
      const device::BluetoothDevice*,
      const device::BluetoothLocalGattCharacteristic*,
      const std::vector<uint8_t>&,
      int,
      base::OnceClosure callback,
      ErrorCallback) override {
    std::move(callback).Run();
  }
  void OnCharacteristicPrepareWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_request,
      base::OnceClosure callback,
      ErrorCallback) override {
    prepare_writes_.push_back(
        {characteristic, value, offset, has_subsequent_request});
    std::move(callback).Run();
  }
  void OnDescriptorReadRequest(const device::BluetoothDevice*,
                               const device::BluetoothLocalGattDescriptor*,
                               int,
                               ValueCallback callback) override {
    std::move(callback).Run(std::nullopt, {});
  }
  void OnDescriptorWriteRequest(const device::BluetoothDevice*,
                                const device::BluetoothLocalGattDescriptor*,
                                const std::vector<uint8_t>&,
                                int,
                                base::OnceClosure callback,
                                ErrorCallback) override {
    std::move(callback).Run();
  }
  void OnNotificationsStart(
      const device::BluetoothDevice*,
      device::BluetoothGattCharacteristic::NotificationType,
      const device::BluetoothLocalGattCharacteristic*) override {}
  void OnNotificationsStop(
      const device::BluetoothDevice*,
      const device::BluetoothLocalGattCharacteristic*) override {}

  std::vector<PrepareWriteEvent> prepare_writes_;
};

}  // namespace

TEST_F(BluetoothGattFlossTest, ExecuteWriteFiltersToTargetedCharacteristics) {
  using Char = device::BluetoothGattCharacteristic;

  RecordingGattDelegate delegate_a;
  RecordingGattDelegate delegate_b;

  auto* floss_adapter = static_cast<BluetoothAdapterFloss*>(adapter_.get());

  // ----- Service A: one writable characteristic + one READ-ONLY one --------
  auto svc_a = floss_adapter->CreateLocalGattService(
      device::BluetoothUUID("11111111-0000-1000-8000-00805f9b34fb"),
      /*is_primary=*/true, &delegate_a);
  ASSERT_TRUE(svc_a);
  auto writable = svc_a->CreateCharacteristic(
      device::BluetoothUUID("11111111-0001-1000-8000-00805f9b34fb"),
      Char::PROPERTY_READ | Char::PROPERTY_WRITE,
      Char::PERMISSION_READ | Char::PERMISSION_WRITE);
  auto read_only_a = svc_a->CreateCharacteristic(
      device::BluetoothUUID("11111111-0002-1000-8000-00805f9b34fb"),
      Char::PROPERTY_READ, Char::PERMISSION_READ);

  // ----- Service B: a single READ-ONLY characteristic in a *different*
  // service -------------
  auto svc_b = floss_adapter->CreateLocalGattService(
      device::BluetoothUUID("22222222-0000-1000-8000-00805f9b34fb"),
      /*is_primary=*/true, &delegate_b);
  ASSERT_TRUE(svc_b);
  auto read_only_b = svc_b->CreateCharacteristic(
      device::BluetoothUUID("22222222-0001-1000-8000-00805f9b34fb"),
      Char::PROPERTY_READ, Char::PERMISSION_READ);

  // Sanity: the read-only characteristics genuinely have no write permission.
  ASSERT_EQ(read_only_a->GetPermissions() & Char::PERMISSION_WRITE, 0u);
  ASSERT_EQ(read_only_b->GetPermissions() & Char::PERMISSION_WRITE, 0u);

  // Register both services.
  svc_a->Register(
      base::DoNothing(),
      base::BindLambdaForTesting(
          [](device::BluetoothGattService::GattErrorCode) { FAIL(); }));
  svc_b->Register(
      base::DoNothing(),
      base::BindLambdaForTesting(
          [](device::BluetoothGattService::GattErrorCode) { FAIL(); }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return svc_a->IsRegistered() && svc_b->IsRegistered(); }));

  // ----- 1. Execute Write with no prior prepared writes should do nothing
  // -----
  SimulateGattServerExecuteWrite(FakeFlossAdapterClient::kBondedAddress1,
                                 /*request_id=*/42, /*execute_write=*/true);
  EXPECT_TRUE(delegate_a.prepare_writes_.empty());
  EXPECT_TRUE(delegate_b.prepare_writes_.empty());

  // ----- 2. Prepare Write to the writable characteristic -----
  auto* writable_floss =
      static_cast<BluetoothLocalGattCharacteristicFloss*>(writable.get());
  SimulateGattServerCharacteristicWriteRequest(
      FakeFlossAdapterClient::kBondedAddress1, /*request_id=*/100, /*offset=*/0,
      /*length=*/2, /*is_prepared_write=*/true, /*needs_response=*/true,
      writable_floss->InstanceId(), {0x01, 0x02});
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate_a.prepare_writes_.size() == 1u; }));

  // Verify prepare write callback received by delegate.
  EXPECT_EQ(delegate_a.prepare_writes_[0].characteristic, writable.get());
  EXPECT_TRUE(delegate_a.prepare_writes_[0].has_subsequent_request);
  delegate_a.prepare_writes_.clear();

  // ----- 3. Execute Write (commit) -----
  SimulateGattServerExecuteWrite(FakeFlossAdapterClient::kBondedAddress1,
                                 /*request_id=*/43, /*execute_write=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate_a.prepare_writes_.size() == 1u; }));

  // ONLY the writable characteristic should receive the commit
  // (has_subsequent_request=false)
  EXPECT_EQ(delegate_a.prepare_writes_[0].characteristic, writable.get());
  EXPECT_FALSE(delegate_a.prepare_writes_[0].has_subsequent_request);
  EXPECT_TRUE(delegate_b.prepare_writes_.empty());
  delegate_a.prepare_writes_.clear();

  // ----- 4. Prepared write followed by execute abort -----
  SimulateGattServerCharacteristicWriteRequest(
      FakeFlossAdapterClient::kBondedAddress1, /*request_id=*/101, /*offset=*/0,
      /*length=*/2, /*is_prepared_write=*/true, /*needs_response=*/true,
      writable_floss->InstanceId(), {0x03, 0x04});
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate_a.prepare_writes_.size() == 1u; }));
  delegate_a.prepare_writes_.clear();

  SimulateGattServerExecuteWrite(FakeFlossAdapterClient::kBondedAddress1,
                                 /*request_id=*/44, /*execute_write=*/false);

  // Abort should clear state without calling delegates
  EXPECT_TRUE(delegate_a.prepare_writes_.empty());
  EXPECT_TRUE(delegate_b.prepare_writes_.empty());

  // Subsequent execute write should do nothing since state was aborted
  SimulateGattServerExecuteWrite(FakeFlossAdapterClient::kBondedAddress1,
                                 /*request_id=*/45, /*execute_write=*/true);
  EXPECT_TRUE(delegate_a.prepare_writes_.empty());
}

TEST_F(BluetoothGattFlossTest, ClearPendingPreparedWritesOnDisconnect) {
  // 1. Set up a local GATT service and characteristic
  auto service = BluetoothLocalGattServiceFloss::Create(
      static_cast<BluetoothAdapterFloss*>(adapter_.get()),
      device::BluetoothUUID("1234"), /*is_primary=*/true,
      /*delegate=*/nullptr);

  auto characteristic = BluetoothLocalGattCharacteristicFloss::Create(
      device::BluetoothUUID("5678"),
      device::BluetoothGattCharacteristic::PROPERTY_WRITE,
      device::BluetoothGattCharacteristic::PERMISSION_WRITE,
      static_cast<BluetoothLocalGattServiceFloss*>(service.get()));

  auto* floss_char =
      static_cast<BluetoothLocalGattCharacteristicFloss*>(characteristic.get());

  std::string test_address = "00:11:22:33:44:55";

  // 2. Simulate an unresolved "Prepare Write" by directly inserting the address
  floss_char->devices_with_pending_prepared_writes_.insert(test_address);

  // Verify it was successfully added
  EXPECT_TRUE(
      floss_char->devices_with_pending_prepared_writes_.contains(test_address));

  // 3. Trigger the disconnect event DIRECTLY on the characteristic
  floss_char->GattServerConnectionState(/*server_id=*/1, /*connected=*/false,
                                        test_address);

  // 4. Assert that the address was successfully removed, proving the leak is
  // fixed!
  EXPECT_FALSE(
      floss_char->devices_with_pending_prepared_writes_.contains(test_address));
}

}  // namespace floss
