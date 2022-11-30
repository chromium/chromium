// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/hid/fido_hid_device.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::base::test::RunClosure;
using ::testing::ElementsAre;

namespace {

device::mojom::HidDeviceInfoPtr MakeOtherDevice(std::string guid) {
  auto other_device = device::mojom::HidDeviceInfo::New();
  other_device->guid = std::move(guid);
  other_device->product_name = "Other Device";
  other_device->serial_number = "OtherDevice";
  other_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  return other_device;
}

device::mojom::HidDeviceInfoPtr MakeDeviceWithOneCollection(
    const std::string& guid,
    uint16_t usage_page) {
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(1, usage_page);
  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = guid;
  device->physical_device_id = "physical-device-id";
  device->vendor_id = 0x1234;
  device->product_id = 0xabcd;
  device->product_name = "product-name";
  device->serial_number = "serial-number";
  device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  device->collections.push_back(std::move(collection));
  device->has_report_id = true;
  device->max_input_report_size = 64;
  device->max_output_report_size = 64;
  device->max_feature_report_size = 64;
  device->device_node = "device-node";
  return device;
}

device::mojom::HidDeviceInfoPtr MakeDeviceWithTwoCollections(
    const std::string& guid,
    uint16_t usage_page1,
    uint16_t usage_page2) {
  // Start with the single-collection device info.
  auto device = MakeDeviceWithOneCollection(guid, usage_page1);

  // Add a second collection with |usage_page2|.
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(1, usage_page2);
  device->collections.push_back(std::move(collection));
  return device;
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
  FidoHidDiscovery discovery;
  MockFidoDiscoveryObserver observer;

  fake_hid_manager_.AddFidoHidDevice("known");

  // Devices initially known to the service before discovery started should be
  // reported as KNOWN.
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true,
                                         ElementsAre(IdMatches("known"))));
  discovery.set_observer(&observer);
  discovery.Start();
  task_environment_.RunUntilIdle();

  // Devices added during the discovery should be reported as ADDED.
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches("added")));
  fake_hid_manager_.AddFidoHidDevice("added");
  task_environment_.RunUntilIdle();

  // Added non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorAdded).Times(0);
  fake_hid_manager_.AddDevice(MakeOtherDevice("other"));

  // Removed non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorRemoved).Times(0);
  fake_hid_manager_.RemoveDevice("other");
  task_environment_.RunUntilIdle();

  // Removed U2F devices should be reported as REMOVED.
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("known")));
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("added")));
  fake_hid_manager_.RemoveDevice("known");
  fake_hid_manager_.RemoveDevice("added");
  task_environment_.RunUntilIdle();
}

TEST_F(FidoHidDiscoveryTest, AlreadyConnectedNonFidoDeviceBecomesFido) {
  constexpr char kTestGuid[] = "guid";

  FidoHidDiscovery discovery;
  MockFidoDiscoveryObserver observer;

  // Add a partially-initialized device. To start, the device only has a single
  // top-level collection with a vendor-defined usage and should not be
  // recognized as a FIDO device.
  fake_hid_manager_.AddDevice(
      MakeDeviceWithOneCollection(kTestGuid, device::mojom::kPageVendor));

  // Start discovery. No U2F devices should be discovered.
  base::RunLoop discovery_started_loop;
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true, ElementsAre()))
      .WillOnce(RunClosure(discovery_started_loop.QuitClosure()));
  discovery.set_observer(&observer);
  discovery.Start();
  discovery_started_loop.Run();

  // Update the device with a second top-level collection with FIDO usage. It
  // should be recognized as a U2F device and dispatch AuthenticatorAdded.
  base::RunLoop authenticator_added_loop;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_added_loop.QuitClosure()));
  fake_hid_manager_.ChangeDevice(MakeDeviceWithTwoCollections(
      kTestGuid, device::mojom::kPageVendor, device::mojom::kPageFido));
  authenticator_added_loop.Run();

  // Remove the device.
  base::RunLoop authenticator_removed_loop;
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_removed_loop.QuitClosure()));
  fake_hid_manager_.RemoveDevice(kTestGuid);
  authenticator_removed_loop.Run();
}

TEST_F(FidoHidDiscoveryTest, NewlyAddedNonFidoDeviceBecomesFido) {
  constexpr char kTestGuid[] = "guid";

  FidoHidDiscovery discovery;
  MockFidoDiscoveryObserver observer;

  // Start discovery. No U2F devices should be discovered.
  base::RunLoop discovery_started_loop;
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true, ElementsAre()))
      .WillOnce(RunClosure(discovery_started_loop.QuitClosure()));
  discovery.set_observer(&observer);
  discovery.Start();
  discovery_started_loop.Run();

  // Add a partially-initialized device. To start, the device only has a single
  // top-level collection with a vendor-defined usage and should not be
  // recognized as a FIDO device.
  EXPECT_CALL(observer, AuthenticatorAdded).Times(0);
  fake_hid_manager_.AddDevice(
      MakeDeviceWithOneCollection(kTestGuid, device::mojom::kPageVendor));
  task_environment_.RunUntilIdle();

  // Update the device with a second top-level collection with FIDO usage. It
  // should be recognized as a U2F device and dispatch AuthenticatorAdded.
  base::RunLoop authenticator_added_loop;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_added_loop.QuitClosure()));
  fake_hid_manager_.ChangeDevice(MakeDeviceWithTwoCollections(
      kTestGuid, device::mojom::kPageVendor, device::mojom::kPageFido));
  authenticator_added_loop.Run();

  // Remove the device.
  base::RunLoop authenticator_removed_loop;
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_removed_loop.QuitClosure()));
  fake_hid_manager_.RemoveDevice(kTestGuid);
  authenticator_removed_loop.Run();
}

TEST_F(FidoHidDiscoveryTest, NewlyAddedFidoDeviceChanged) {
  constexpr char kTestGuid[] = "guid";

  FidoHidDiscovery discovery;
  MockFidoDiscoveryObserver observer;

  // Start discovery. No U2F devices should be discovered.
  base::RunLoop discovery_started_loop;
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true, ElementsAre()))
      .WillOnce(RunClosure(discovery_started_loop.QuitClosure()));
  discovery.set_observer(&observer);
  discovery.Start();
  discovery_started_loop.Run();

  // Add a partially-initialized device. To start, the device only has a single
  // top-level collection with a FIDO usage and should be recognized as a U2F
  // device.
  base::RunLoop authenticator_added_loop;
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_added_loop.QuitClosure()));
  fake_hid_manager_.AddDevice(
      MakeDeviceWithOneCollection(kTestGuid, device::mojom::kPageFido));
  authenticator_added_loop.Run();

  // Update the device with a second top-level collection with vendor-defined
  // usage. The update should be ignored since the device was already added.
  EXPECT_CALL(observer, AuthenticatorAdded).Times(0);
  fake_hid_manager_.ChangeDevice(MakeDeviceWithTwoCollections(
      kTestGuid, device::mojom::kPageFido, device::mojom::kPageVendor));
  task_environment_.RunUntilIdle();

  // Remove the device.
  base::RunLoop authenticator_removed_loop;
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches(kTestGuid)))
      .WillOnce(RunClosure(authenticator_removed_loop.QuitClosure()));
  fake_hid_manager_.RemoveDevice(kTestGuid);
  authenticator_removed_loop.Run();
}

}  // namespace device
