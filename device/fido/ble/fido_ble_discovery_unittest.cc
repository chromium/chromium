// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#endif

namespace device {

namespace {

using ::testing::_;
using ::testing::Return;
using TestMockDevice = ::testing::NiceMock<MockBluetoothDevice>;
using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;

constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceChangedName[] = "device_changed_name";
constexpr char kDeviceAddress[] = "device_address";
constexpr char kDeviceChangedAddress[] = "device_changed_address";
constexpr char kAuthenticatorId[] = "ble:device_address";
constexpr char kAuthenticatorChangedId[] = "ble:device_changed_address";

ACTION_P(ReturnFromAsyncCall, closure) {
  closure.Run();
}

MATCHER_P(IdMatches, id, "") {
  return arg->GetId() == std::string("ble:") + id;
}

}  // namespace

class FidoBleDiscoveryTest : public ::testing::Test {
 public:
  FidoBleDiscoveryTest() { discovery_.set_observer(&observer_); }

  std::unique_ptr<TestMockDevice> CreateMockFidoDevice() {
    DCHECK(adapter_);
    auto mock_device = std::make_unique<TestMockDevice>(
        adapter_.get(), 0 /* bluetooth_class */, kDeviceName, kDeviceAddress,
        false /* paired */, false /* connected */);
    EXPECT_CALL(*mock_device, GetUUIDs)
        .WillRepeatedly(Return(
            std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)}));
    EXPECT_CALL(*mock_device, GetAddress)
        .WillRepeatedly(Return(kDeviceAddress));

    EXPECT_CALL(*adapter(), GetDevice(kDeviceAddress))
        .WillRepeatedly(Return(mock_device.get()));

    return mock_device;
  }

  std::unique_ptr<TestMockDevice> CreateChangedMockFidoDevice() {
    DCHECK(adapter_);
    auto mock_device = std::make_unique<TestMockDevice>(
        adapter_.get(), 0 /* bluetooth_class */, kDeviceChangedName,
        kDeviceChangedAddress, false /* paired */, false /* connected */);
    EXPECT_CALL(*mock_device, GetUUIDs)
        .WillRepeatedly(Return(
            std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)}));
    EXPECT_CALL(*mock_device, GetAddress)
        .WillRepeatedly(Return(kDeviceChangedAddress));

    EXPECT_CALL(*adapter(), GetDevice(kDeviceChangedAddress))
        .WillRepeatedly(Return(mock_device.get()));

    return mock_device;
  }

  void SetDeviceInPairingMode(TestMockDevice* device) {
    // Update device advertisement data so that it represents BLE device in
    // pairing mode.
    DCHECK(adapter_);
    device->UpdateAdvertisementData(
        0 /* rssi */, 1 << kLeLimitedDiscoverableModeBit,
        std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)},
        base::nullopt /* tx_power */, BluetoothDevice::ServiceDataMap(),
        BluetoothDevice::ManufacturerDataMap());
    adapter_->NotifyDeviceChanged(device);
  }

  void SetMockBluetoothAdapter() {
    adapter_ = base::MakeRefCounted<NiceMockBluetoothAdapter>();
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  void ExpectSuccessfulStartScan() {
    EXPECT_CALL(*adapter(), StartScanWithFilter_)
        .WillOnce(testing::Invoke(
            [](const device::BluetoothDiscoveryFilter* discovery_filter,
               device::BluetoothAdapter::DiscoverySessionResultCallback&
                   callback) {
              std::move(callback).Run(
                  /*is_error=*/false,
                  device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
            }));
  }

  FidoBleDiscovery* discovery() { return &discovery_; }
  MockFidoDiscoveryObserver* observer() { return &observer_; }
  MockBluetoothAdapter* adapter() {
    DCHECK(adapter_);
    return adapter_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<MockBluetoothAdapter> adapter_;
  MockFidoDiscoveryObserver observer_;
  FidoBleDiscovery discovery_;
};

TEST_F(FidoBleDiscoveryTest,
       FidoBleDiscoveryNotifyObserverWhenAdapterNotPresent) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(false));
  EXPECT_CALL(*adapter(), SetPowered).Times(0);
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), false,
                                            std::vector<FidoAuthenticator*>()));
  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(FidoBleDiscoveryTest, FidoBleDiscoveryResumeScanningAfterPoweredOn) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(Return(false));

  // After BluetoothAdapter is powered on, we expect that discovery session
  // starts again. Immediately calling the callback so that it does not hold a
  // reference to the adapter.
  ExpectSuccessfulStartScan();
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), true,
                                            std::vector<FidoAuthenticator*>()));
  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  adapter()->NotifyAdapterPoweredChanged(true);
}

TEST_F(FidoBleDiscoveryTest, FidoBleDiscoveryNoAdapter) {
  // We purposefully construct a temporary and provide no fake adapter,
  // simulating cases where the discovery is destroyed before obtaining a handle
  // to an adapter. This should be handled gracefully and not result in a crash.
  // We don't expect any calls to the notification methods.
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), _, _)).Times(0);
  EXPECT_CALL(*observer(), AuthenticatorAdded(discovery(), _)).Times(0);
  EXPECT_CALL(*observer(), AuthenticatorRemoved(discovery(), _)).Times(0);
}

TEST_F(BluetoothTest, FidoBleDiscoveryFindsKnownDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  SimulateLowEnergyDevice(4);  // This device should be ignored.
  SimulateLowEnergyDevice(7);

  FidoBleDiscovery discovery;

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer,
                DiscoveryStarted(&discovery, true,
                                 testing::ElementsAre(IdMatches(
                                     BluetoothTestBase::kTestDeviceAddress1))))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }
}

TEST_F(BluetoothTest, FidoBleDiscoveryFindsNewDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true,
                                           std::vector<FidoAuthenticator*>()))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(
        observer,
        AuthenticatorAdded(&discovery,
                           IdMatches(BluetoothTestBase::kTestDeviceAddress1)))
        .WillOnce(ReturnFromAsyncCall(quit));

    SimulateLowEnergyDevice(4);  // This device should be ignored.
    SimulateLowEnergyDevice(7);

    run_loop.Run();
  }
}

// Simulate the scenario where the BLE device is already known at start-up time,
// but no service advertisements have been received from the device yet, so we
// do not know if it is a CTAP2/U2F device or not. As soon as it is discovered
// that the device supports the FIDO service, the observer should be notified of
// a new FidoBleDevice.
TEST_F(BluetoothTest, FidoBleDiscoveryFindsUpdatedDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  SimulateLowEnergyDevice(3);

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true,
                                           std::vector<FidoAuthenticator*>()))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();

    EXPECT_THAT(discovery.GetAuthenticatorsForTesting(), ::testing::IsEmpty());
  }

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(
        observer,
        AuthenticatorAdded(&discovery,
                           IdMatches(BluetoothTestBase::kTestDeviceAddress1)))
        .WillOnce(ReturnFromAsyncCall(quit));

    // This will update properties for device 3.
    SimulateLowEnergyDevice(7);

    run_loop.Run();

    const auto authenticators = discovery.GetAuthenticatorsForTesting();
    ASSERT_THAT(authenticators, ::testing::SizeIs(1u));
    EXPECT_EQ(
        FidoBleDevice::GetIdForAddress(BluetoothTestBase::kTestDeviceAddress1),
        authenticators[0]->GetId());
  }
}

TEST_F(BluetoothTest, FidoBleDiscoveryRejectsCableDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true,
                                           std::vector<FidoAuthenticator*>()))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _)).Times(0);

  // Simulates a discovery of two Cable devices one of which is an Android Cable
  // authenticator and other is IOS Cable authenticator.
  SimulateLowEnergyDevice(8);
  SimulateLowEnergyDevice(9);

  // Simulates a device change update received from the BluetoothAdapter. As the
  // updated device has an address that we know is an Cable device, this should
  // not trigger AuthenticatorAdded().
  SimulateLowEnergyDevice(7);
}

TEST_F(FidoBleDiscoveryTest,
       DiscoveryDoesNotAddDuplicateDeviceOnAddressChanged) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(true));
  auto mock_device = CreateMockFidoDevice();

  EXPECT_CALL(*observer(), AuthenticatorIdChanged(discovery(), kAuthenticatorId,
                                                  kAuthenticatorChangedId));
  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  adapter()->NotifyDeviceChanged(mock_device.get());
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_device.get()));

  EXPECT_CALL(*mock_device.get(), GetAddress)
      .WillRepeatedly(Return(kDeviceChangedAddress));
  for (auto& observer : adapter()->GetObservers()) {
    observer.DeviceAddressChanged(adapter(), mock_device.get(), kDeviceAddress);
  }

  adapter()->NotifyDeviceChanged(mock_device.get());

  EXPECT_EQ(1u, discovery()->GetAuthenticatorsForTesting().size());
  EXPECT_TRUE(discovery()->GetAuthenticatorForTesting(kAuthenticatorChangedId));
}

TEST_F(FidoBleDiscoveryTest, DiscoveryNotifiesObserverWhenDeviceInPairingMode) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(Return(true));
  auto mock_device = CreateMockFidoDevice();

  ::testing::InSequence sequence;
  ExpectSuccessfulStartScan();
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), true, _));
  const auto device_id = FidoBleDevice::GetIdForAddress(kDeviceAddress);
  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*observer(),
              AuthenticatorAdded(discovery(), IdMatches(kDeviceAddress)));
  adapter()->NotifyDeviceChanged(mock_device.get());

  EXPECT_CALL(*observer(),
              AuthenticatorPairingModeChanged(discovery(), device_id, true));
  SetDeviceInPairingMode(mock_device.get());
  auto it = discovery()->pairing_mode_device_tracker_.find(device_id);
  EXPECT_TRUE(it != discovery()->pairing_mode_device_tracker_.end());
  EXPECT_TRUE(it->second->IsRunning());
}

TEST_F(FidoBleDiscoveryTest,
       DiscoveryNotifiesObserverWhenDeviceInNonPairingMode) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(Return(true));
  auto mock_device = CreateMockFidoDevice();

  ::testing::InSequence sequence;
  ExpectSuccessfulStartScan();
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), true, _));
  const auto device_id = FidoBleDevice::GetIdForAddress(kDeviceAddress);
  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*observer(),
              AuthenticatorAdded(discovery(), IdMatches(kDeviceAddress)));
  adapter()->NotifyDeviceChanged(mock_device.get());

  EXPECT_CALL(*observer(),
              AuthenticatorPairingModeChanged(discovery(), device_id, true));

  SetDeviceInPairingMode(mock_device.get());
  auto it = discovery()->pairing_mode_device_tracker_.find(device_id);
  ASSERT_TRUE(it != discovery()->pairing_mode_device_tracker_.end());
  EXPECT_TRUE(it->second->IsRunning());

  // Simulates BLE device sending advertisement packet after some interval.
  // Since device did not advertise that it is in pairing mode for longer than 3
  // seconds, we expect that the discovery should
  //   1) First set the device to be in non-pairing mode and notify the
  //   observer.
  //   2) When advertisement packet arrives after delay, device is re-set to
  //   pairing mode and observer is notified.
  //   3) When timer completes due to FastForwardUntilNoTasksRemain(),
  //   authenticator is re-set to non-pairing mode.
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(observer()));
  EXPECT_CALL(*observer(),
              AuthenticatorPairingModeChanged(discovery(), device_id, false));
  EXPECT_CALL(*observer(),
              AuthenticatorPairingModeChanged(discovery(), device_id, true));
  EXPECT_CALL(*observer(),
              AuthenticatorPairingModeChanged(discovery(), device_id, false));

  task_environment_.GetMainThreadTaskRunner().get()->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&, this] {
        adapter()->NotifyDeviceChanged(mock_device.get());
      }),
      base::TimeDelta::FromSeconds(4));

  task_environment_.FastForwardUntilNoTasksRemain();
}

// Verify that if a device changes its address and the new address collides
// with the address of an existing device, the operation is aborted with no
// changes.
TEST_F(FidoBleDiscoveryTest, DiscoveryDoesNotDeleteDeviceOnAddressCollision) {
  SetMockBluetoothAdapter();
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(Return(true));
  auto mock_device = CreateMockFidoDevice();
  auto changed_mock_device = CreateChangedMockFidoDevice();

  ExpectSuccessfulStartScan();
  EXPECT_CALL(*observer(), DiscoveryStarted(discovery(), true, _));

  EXPECT_CALL(*observer(),
              AuthenticatorAdded(discovery(), IdMatches(kDeviceAddress)));

  EXPECT_CALL(*observer(), AuthenticatorAdded(
                               discovery(), IdMatches(kDeviceChangedAddress)));

  discovery()->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  adapter()->NotifyDeviceChanged(mock_device.get());
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_device.get()));

  adapter()->NotifyDeviceChanged(changed_mock_device.get());
  ASSERT_TRUE(
      ::testing::Mock::VerifyAndClearExpectations(changed_mock_device.get()));

  EXPECT_EQ(2u, discovery()->GetAuthenticatorsForTesting().size());

  FidoAuthenticator* authenticator =
      discovery()->GetAuthenticatorForTesting(kAuthenticatorId);

  // Assign address |kDeviceChangedAddress| to mock_device, which originally
  // had address |kDeviceAddress|. This will collide with
  // |changed_mock_device| which is already present.
  EXPECT_CALL(*mock_device.get(), GetAddress)
      .WillRepeatedly(Return(kDeviceChangedAddress));
  for (auto& observer : adapter()->GetObservers()) {
    observer.DeviceAddressChanged(adapter(), mock_device.get(), kDeviceAddress);
  }

  adapter()->NotifyDeviceChanged(mock_device.get());

  EXPECT_EQ(authenticator->GetId(),
            FidoBleDevice::GetIdForAddress(kDeviceChangedAddress));
  EXPECT_EQ(2u, discovery()->GetAuthenticatorsForTesting().size());
  EXPECT_TRUE(discovery()->GetAuthenticatorForTesting(
      FidoBleDevice::GetIdForAddress(kDeviceChangedAddress)));
}

}  // namespace device
