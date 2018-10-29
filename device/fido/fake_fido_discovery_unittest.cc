// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fake_fido_discovery.h"

#include <utility>

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace test {

using ::testing::_;

class FakeFidoDiscoveryTest : public ::testing::Test {
 public:
  FakeFidoDiscoveryTest() = default;
  ~FakeFidoDiscoveryTest() override = default;

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeFidoDiscoveryTest);
};

using ScopedFakeFidoDiscoveryFactoryTest = FakeFidoDiscoveryTest;

TEST_F(FakeFidoDiscoveryTest, Transport) {
  FakeFidoDiscovery discovery_ble(FidoTransportProtocol::kBluetoothLowEnergy);
  EXPECT_EQ(FidoTransportProtocol::kBluetoothLowEnergy,
            discovery_ble.transport());

  FakeFidoDiscovery discovery_hid(
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_EQ(FidoTransportProtocol::kUsbHumanInterfaceDevice,
            discovery_hid.transport());
}

TEST_F(FakeFidoDiscoveryTest, InitialState) {
  FakeFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);

  ASSERT_FALSE(discovery.is_running());
  ASSERT_FALSE(discovery.is_start_requested());
}

TEST_F(FakeFidoDiscoveryTest, StartDiscovery) {
  FakeFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  discovery.Start();

  ASSERT_TRUE(discovery.is_start_requested());
  ASSERT_FALSE(discovery.is_running());

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.WaitForCallToStartAndSimulateSuccess();
  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
}

TEST_F(FakeFidoDiscoveryTest, WaitThenStartStopDiscovery) {
  FakeFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { discovery.Start(); }));

  discovery.WaitForCallToStart();

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.SimulateStarted(true);
  ASSERT_TRUE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
}

// Starting discovery and failing: instance stays in "not running" state
TEST_F(FakeFidoDiscoveryTest, StartFail) {
  FakeFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  discovery.Start();

  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, false));
  discovery.SimulateStarted(false);
  ASSERT_FALSE(discovery.is_running());
  ASSERT_TRUE(discovery.is_start_requested());
}

// Adding device is possible both before and after discovery actually starts.
TEST_F(FakeFidoDiscoveryTest, AddDevice) {
  FakeFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  discovery.Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillOnce(::testing::Return("device0"));
  base::RunLoop device0_done;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _))
      .WillOnce(testing::InvokeWithoutArgs(
          [&device0_done]() { device0_done.Quit(); }));
  discovery.AddDevice(std::move(device0));
  device0_done.Run();
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.SimulateStarted(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillOnce(::testing::Return("device1"));
  base::RunLoop device1_done;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _))
      .WillOnce(testing::InvokeWithoutArgs(
          [&device1_done]() { device1_done.Quit(); }));
  discovery.AddDevice(std::move(device1));
  device1_done.Run();
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

#if !defined(OS_ANDROID)
TEST_F(ScopedFakeFidoDiscoveryFactoryTest,
       OverridesUsbFactoryFunctionWhileInScope) {
  ScopedFakeFidoDiscoveryFactory factory;
  auto* injected_fake_discovery = factory.ForgeNextHidDiscovery();
  ASSERT_EQ(FidoTransportProtocol::kUsbHumanInterfaceDevice,
            injected_fake_discovery->transport());
  auto produced_discovery = FidoDeviceDiscovery::Create(
      FidoTransportProtocol::kUsbHumanInterfaceDevice, nullptr);
  EXPECT_TRUE(produced_discovery);
  EXPECT_EQ(injected_fake_discovery, produced_discovery.get());
}
#endif

TEST_F(ScopedFakeFidoDiscoveryFactoryTest,
       OverridesBleFactoryFunctionWhileInScope) {
  ScopedFakeFidoDiscoveryFactory factory;

  auto* injected_fake_discovery_1 = factory.ForgeNextBleDiscovery();
  ASSERT_EQ(FidoTransportProtocol::kBluetoothLowEnergy,
            injected_fake_discovery_1->transport());
  auto produced_discovery_1 = FidoDeviceDiscovery::Create(
      FidoTransportProtocol::kBluetoothLowEnergy, nullptr);
  EXPECT_EQ(injected_fake_discovery_1, produced_discovery_1.get());

  auto* injected_fake_discovery_2 = factory.ForgeNextBleDiscovery();
  ASSERT_EQ(FidoTransportProtocol::kBluetoothLowEnergy,
            injected_fake_discovery_2->transport());
  auto produced_discovery_2 = FidoDeviceDiscovery::Create(
      FidoTransportProtocol::kBluetoothLowEnergy, nullptr);
  EXPECT_EQ(injected_fake_discovery_2, produced_discovery_2.get());
}

}  // namespace test
}  // namespace device
