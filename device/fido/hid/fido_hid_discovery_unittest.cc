// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/hid/fido_hid_device.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;

namespace {

device::mojom::HidDeviceInfoPtr MakeOtherDevice(std::string guid) {
  auto other_device = device::mojom::HidDeviceInfo::New();
  other_device->guid = std::move(guid);
  other_device->product_name = "Other Device";
  other_device->serial_number = "OtherDevice";
  other_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  return other_device;
}

MATCHER_P(IdMatches, id, "") {
  return arg->GetId() == std::string("hid:") + id;
}

}  // namespace

class FidoHidDiscoveryTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  ScopedFakeFidoHidManager fake_hid_manager_;
};

TEST_F(FidoHidDiscoveryTest, TestAddRemoveDevice) {
  FidoHidDiscovery discovery(fake_hid_manager_.service_manager_connector());
  MockFidoDiscoveryObserver observer;

  fake_hid_manager_.AddFidoHidDevice("known");

  // Devices initially known to the service before discovery started should be
  // reported as KNOWN.
  EXPECT_CALL(observer,
              DiscoveryStarted(&discovery, true,
                               testing::ElementsAre(IdMatches("known"))));
  discovery.set_observer(&observer);
  discovery.Start();
  task_environment_.RunUntilIdle();

  // Devices added during the discovery should be reported as ADDED.
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches("added")));
  fake_hid_manager_.AddFidoHidDevice("added");
  task_environment_.RunUntilIdle();

  // Added non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorAdded(_, _)).Times(0);
  fake_hid_manager_.AddDevice(MakeOtherDevice("other"));

  // Removed non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorRemoved(_, _)).Times(0);
  fake_hid_manager_.RemoveDevice("other");
  task_environment_.RunUntilIdle();

  // Removed U2F devices should be reported as REMOVED.
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("known")));
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("added")));
  fake_hid_manager_.RemoveDevice("known");
  fake_hid_manager_.RemoveDevice("added");
  task_environment_.RunUntilIdle();
}

}  // namespace device
