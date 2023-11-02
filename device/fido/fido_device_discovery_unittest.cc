// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_discovery.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;

// A minimal implementation of FidoDeviceDiscovery that is no longer abstract.
class ConcreteFidoDiscovery : public FidoDeviceDiscovery {
 public:
  explicit ConcreteFidoDiscovery(FidoTransportProtocol transport)
      : FidoDeviceDiscovery(transport) {}

  ConcreteFidoDiscovery(const ConcreteFidoDiscovery&) = delete;
  ConcreteFidoDiscovery& operator=(const ConcreteFidoDiscovery&) = delete;

  ~ConcreteFidoDiscovery() override = default;

  MOCK_METHOD0(StartInternal, void());

  using FidoDeviceDiscovery::AddDevice;
  using FidoDeviceDiscovery::NotifyDiscoveryStarted;
  using FidoDeviceDiscovery::RemoveDevice;
};

}  // namespace

TEST(FidoDiscoveryTest, TestAddAndRemoveObserver) {
  ConcreteFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);
  MockFidoDiscoveryObserver observer;
  EXPECT_EQ(nullptr, discovery.observer());

  discovery.set_observer(&observer);
  EXPECT_EQ(&observer, discovery.observer());

  discovery.set_observer(nullptr);
  EXPECT_EQ(nullptr, discovery.observer());
}

TEST(FidoDiscoveryTest, TestNotificationsOnSuccessfulStart) {
  base::test::TaskEnvironment task_environment_;

  ConcreteFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  EXPECT_FALSE(discovery.is_start_requested());
  EXPECT_FALSE(discovery.is_running());

  EXPECT_CALL(discovery, StartInternal());
  discovery.Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(discovery.is_start_requested());
  EXPECT_FALSE(discovery.is_running());
  ::testing::Mock::VerifyAndClearExpectations(&discovery);

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true,
                                         std::vector<FidoAuthenticator*>()));
  discovery.NotifyDiscoveryStarted(true);
  EXPECT_TRUE(discovery.is_start_requested());
  EXPECT_TRUE(discovery.is_running());
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST(FidoDiscoveryTest, TestNotificationsOnFailedStart) {
  base::test::TaskEnvironment task_environment_;

  ConcreteFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  discovery.Start();
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, false,
                                         std::vector<FidoAuthenticator*>()));
  discovery.NotifyDiscoveryStarted(false);
  EXPECT_TRUE(discovery.is_start_requested());
  EXPECT_FALSE(discovery.is_running());
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST(FidoDiscoveryTest, TestAddRemoveDevices) {
  base::test::TaskEnvironment task_environment_;
  ConcreteFidoDiscovery discovery(FidoTransportProtocol::kBluetoothLowEnergy);
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);
  discovery.Start();

  // Expect successful insertion.
  auto device0 = std::make_unique<MockFidoDevice>();
  auto* device0_raw = device0.get();
  FidoAuthenticator* authenticator0 = nullptr;
  base::RunLoop device0_done;
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true, _))
      .WillOnce(testing::Invoke(
          [&](auto* discovery, bool success, auto authenticators) {
            EXPECT_EQ(1u, authenticators.size());
            authenticator0 = authenticators[0];
            device0_done.Quit();
          }));
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(Return("device0"));
  EXPECT_TRUE(discovery.AddDevice(std::move(device0)));
  discovery.NotifyDiscoveryStarted(true);
  EXPECT_EQ("device0", authenticator0->GetId());
  EXPECT_EQ(device0_raw,
            static_cast<FidoDeviceAuthenticator*>(authenticator0)->device());
  device0_done.Run();
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Expect successful insertion after starting.
  base::RunLoop device1_done;
  auto device1 = std::make_unique<MockFidoDevice>();
  auto* device1_raw = device1.get();
  FidoAuthenticator* authenticator1 = nullptr;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _))
      .WillOnce(DoAll(SaveArg<1>(&authenticator1),
                      testing::InvokeWithoutArgs(
                          [&device1_done]() { device1_done.Quit(); })));
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(Return("device1"));
  EXPECT_TRUE(discovery.AddDevice(std::move(device1)));
  EXPECT_EQ("device1", authenticator1->GetId());
  EXPECT_EQ(device1_raw,
            static_cast<FidoDeviceAuthenticator*>(authenticator1)->device());
  device1_done.Run();
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Inserting a device with an already present id should be prevented.
  auto device1_dup = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(observer, AuthenticatorAdded(_, _)).Times(0);
  EXPECT_CALL(*device1_dup, GetId()).WillOnce(Return("device1"));
  EXPECT_FALSE(discovery.AddDevice(std::move(device1_dup)));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_EQ(authenticator0, discovery.GetAuthenticatorForTesting("device0"));
  EXPECT_EQ(authenticator1, discovery.GetAuthenticatorForTesting("device1"));
  EXPECT_THAT(discovery.GetAuthenticatorsForTesting(),
              UnorderedElementsAre(authenticator0, authenticator1));

  const FidoDeviceDiscovery& const_discovery = discovery;
  EXPECT_THAT(const_discovery.GetAuthenticatorsForTesting(),
              UnorderedElementsAre(authenticator0, authenticator1));

  // Trying to remove a non-present device should fail.
  EXPECT_CALL(observer, AuthenticatorRemoved(_, _)).Times(0);
  EXPECT_FALSE(discovery.RemoveDevice("device2"));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, authenticator1));
  EXPECT_TRUE(discovery.RemoveDevice("device1"));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, authenticator0));
  EXPECT_TRUE(discovery.RemoveDevice("device0"));
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace device
