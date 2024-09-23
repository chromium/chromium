// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using base::test::RunOnceClosure;
using ::testing::_;
using BleStatus = FidoRequestHandlerBase::BleStatus;

constexpr char kTestBluetoothDeviceAddress[] = "test_device_address";
constexpr char kTestBluetoothDisplayName[] = "device_name";

class MockObserver : public FidoRequestHandlerBase::Observer {
 public:
  MockObserver() = default;

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  ~MockObserver() override = default;

  MOCK_METHOD1(OnTransportAvailabilityEnumerated,
               void(FidoRequestHandlerBase::TransportAvailabilityInfo data));
  MOCK_METHOD1(EmbedderControlsAuthenticatorDispatch,
               bool(const FidoAuthenticator& authenticator));
  MOCK_METHOD1(BluetoothAdapterStatusChanged, void(BleStatus ble_status));
  MOCK_METHOD1(FidoAuthenticatorAdded,
               void(const FidoAuthenticator& authenticator));
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(std::string_view device_id));
  MOCK_CONST_METHOD0(SupportsPIN, bool());
  MOCK_METHOD2(CollectPIN,
               void(CollectPINOptions,
                    base::OnceCallback<void(std::u16string)>));
  MOCK_METHOD0(OnForcePINChange, void());
  MOCK_METHOD1(StartBioEnrollment, void(base::OnceClosure));
  MOCK_METHOD1(OnSampleCollected, void(int));
  MOCK_METHOD0(FinishCollectToken, void());
  MOCK_METHOD1(OnRetryUserVerification, void(int));
  MOCK_METHOD0(OnInternalUserVerificationLocked, void());
  MOCK_METHOD1(SetMightCreateResidentCredential, void(bool));
};

class FakeFidoRequestHandlerBase : public FidoRequestHandlerBase {
 public:
  FakeFidoRequestHandlerBase(MockObserver* observer,
                             FidoDiscoveryFactory* fido_discovery_factory)
      : FidoRequestHandlerBase(fido_discovery_factory,
                               {FidoTransportProtocol::kHybrid}) {
    set_observer(observer);
    Start();
  }

  FakeFidoRequestHandlerBase(const FakeFidoRequestHandlerBase&) = delete;
  FakeFidoRequestHandlerBase& operator=(const FakeFidoRequestHandlerBase&) =
      delete;

  void SimulateFidoRequestHandlerHasAuthenticator(bool simulate_authenticator) {
    simulate_authenticator_ = simulate_authenticator;
  }

 private:
  void DispatchRequest(FidoAuthenticator*) override {}

  bool HasAuthenticator(
      const std::string& authentictator_address) const override {
    return simulate_authenticator_;
  }

  bool simulate_authenticator_ = false;
};

}  // namespace

class FidoBleAdapterManagerTest : public ::testing::Test {
 public:
  FidoBleAdapterManagerTest() {
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    bluetooth_config_ =
        BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
    bluetooth_config_->SetLESupported(true);
    fido_discovery_factory_->ForgeNextCableDiscovery(
        test::FakeFidoDiscovery::StartMode::kAutomatic);

    fake_request_handler_ = std::make_unique<FakeFidoRequestHandlerBase>(
        mock_observer_.get(), fido_discovery_factory_.get());
  }

  MockBluetoothDevice* AddMockBluetoothDeviceToAdapter() {
    auto mock_bluetooth_device = std::make_unique<MockBluetoothDevice>(
        adapter_.get(), 0 /* bluetooth_class */, kTestBluetoothDisplayName,
        kTestBluetoothDeviceAddress, false /* paired */, false /* connected */);

    auto* mock_bluetooth_device_ptr = mock_bluetooth_device.get();
    adapter_->AddMockDevice(std::move(mock_bluetooth_device));
    return mock_bluetooth_device_ptr;
  }

  MockBluetoothAdapter* adapter() { return adapter_.get(); }
  MockObserver* observer() { return mock_observer_.get(); }

  FakeFidoRequestHandlerBase* fake_request_handler() {
    return fake_request_handler_.get();
  }

  FidoRequestHandlerBase::TransportAvailabilityInfo
  WaitForTransportAvailabilityInfo() {
    FidoRequestHandlerBase::TransportAvailabilityInfo data;
    base::RunLoop run_loop;
    EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
        .WillOnce([&](auto transport_availability) {
          data = transport_availability;
          run_loop.Quit();
        });
    run_loop.Run();
    return data;
  }

  void SetAdapterPermissions(BluetoothAdapter::PermissionStatus status) {
    EXPECT_CALL(*adapter(), RequestSystemPermission)
        .WillOnce([status](MockBluetoothAdapter::RequestSystemPermissionCallback
                               callback) { std::move(callback).Run(status); });
    EXPECT_CALL(*adapter(), GetOsPermissionStatus)
        .WillOnce(::testing::Return(status));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  std::unique_ptr<MockObserver> mock_observer_ =
      std::make_unique<MockObserver>();
  std::unique_ptr<test::FakeFidoDiscoveryFactory> fido_discovery_factory_ =
      std::make_unique<test::FakeFidoDiscoveryFactory>();

  std::unique_ptr<FakeFidoRequestHandlerBase> fake_request_handler_;
  std::unique_ptr<BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_config_;
  FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_calls_;
};

TEST_F(FidoBleAdapterManagerTest, AdapterNotPresent) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data =
      WaitForTransportAvailabilityInfo();
  EXPECT_EQ(data.ble_status, BleStatus::kOff);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentAndPowered) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data =
      WaitForTransportAvailabilityInfo();
  EXPECT_EQ(data.ble_status, BleStatus::kOn);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentAndCanBePowered) {
  EXPECT_CALL(*adapter(), IsPresent).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower).WillOnce(::testing::Return(true));

  FidoRequestHandlerBase::TransportAvailabilityInfo data =
      WaitForTransportAvailabilityInfo();
  EXPECT_EQ(data.ble_status, BleStatus::kOff);
  EXPECT_TRUE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentButChromeDisallowed) {
  EXPECT_CALL(*adapter(), IsPresent).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), CanPower).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), GetOsPermissionStatus)
      .WillOnce(::testing::Return(BluetoothAdapter::PermissionStatus::kDenied));

  FidoRequestHandlerBase::TransportAvailabilityInfo data =
      WaitForTransportAvailabilityInfo();
  EXPECT_EQ(data.ble_status, BleStatus::kPermissionDenied);
  EXPECT_TRUE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentButPermissionUndetermined) {
  EXPECT_CALL(*adapter(), IsPresent).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), CanPower).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), GetOsPermissionStatus)
      .WillOnce(
          ::testing::Return(BluetoothAdapter::PermissionStatus::kUndetermined));
  EXPECT_CALL(*adapter(), IsPowered).Times(0);

  FidoRequestHandlerBase::TransportAvailabilityInfo data =
      WaitForTransportAvailabilityInfo();
  EXPECT_EQ(data.ble_status, BleStatus::kPendingPermissionRequest);
  EXPECT_TRUE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, SetBluetoothPowerOn) {
  task_environment_.RunUntilIdle();
  auto& power_manager =
      fake_request_handler_->get_bluetooth_adapter_manager_for_testing();
  ::testing::InSequence s;
  EXPECT_CALL(*adapter(), SetPowered(true, _, _));
  power_manager->SetAdapterPower(/*set_power_on=*/true);
  power_manager.reset();
}

TEST_F(FidoBleAdapterManagerTest, RequestBluetoothPermissionAllowed) {
  task_environment_.RunUntilIdle();
  SetAdapterPermissions(BluetoothAdapter::PermissionStatus::kAllowed);
  EXPECT_CALL(*adapter(), IsPowered).WillOnce(::testing::Return(true));

  base::test::TestFuture<BleStatus> future;
  fake_request_handler_->RequestBluetoothPermission(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), BleStatus::kOn);
}

TEST_F(FidoBleAdapterManagerTest, RequestBluetoothPermissionDenied) {
  task_environment_.RunUntilIdle();
  SetAdapterPermissions(BluetoothAdapter::PermissionStatus::kDenied);
  EXPECT_CALL(*adapter(), IsPowered).Times(0);

  base::test::TestFuture<BleStatus> future;
  fake_request_handler_->RequestBluetoothPermission(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), BleStatus::kPermissionDenied);
}

// Tests that if the Bluetooth API happens to report the OS permission status as
// "undetermined" after requesting permissions, the adapter manager reports is
// as available to let Chrome at least try to use it.
TEST_F(FidoBleAdapterManagerTest,
       RequestBluetoothPermissionHandlesUndetermined) {
  task_environment_.RunUntilIdle();
  SetAdapterPermissions(BluetoothAdapter::PermissionStatus::kUndetermined);
  EXPECT_CALL(*adapter(), IsPowered).WillOnce(::testing::Return(true));

  base::test::TestFuture<BleStatus> future;
  fake_request_handler_->RequestBluetoothPermission(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), BleStatus::kOn);
}

}  // namespace device
