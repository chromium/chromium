// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
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

device::mojom::HidDeviceInfoPtr MakeFidoHidDevice(std::string guid) {
  auto c_info = device::mojom::HidCollectionInfo::New();
  c_info->usage = device::mojom::HidUsageAndPage::New(1, 0xf1d0);

  auto u2f_device = device::mojom::HidDeviceInfo::New();
  u2f_device->guid = std::move(guid);
  u2f_device->product_name = "Test Fido Device";
  u2f_device->serial_number = "123FIDO";
  u2f_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  u2f_device->collections.push_back(std::move(c_info));
  u2f_device->max_input_report_size = 64;
  u2f_device->max_output_report_size = 64;
  return u2f_device;
}

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
 public:
  base::test::ScopedTaskEnvironment& scoped_task_environment() {
    return scoped_task_environment_;
  }

  void SetUp() override {
    fake_hid_manager_ = std::make_unique<FakeHidManager>();

    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::Bind(&FakeHidManager::AddBinding,
                   base::Unretained(fake_hid_manager_.get())));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<FakeHidManager> fake_hid_manager_;
};

TEST_F(FidoHidDiscoveryTest, TestAddRemoveDevice) {
  FidoHidDiscovery discovery(connector_.get());
  MockFidoDiscoveryObserver observer;

  fake_hid_manager_->AddDevice(MakeFidoHidDevice("known"));

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.set_observer(&observer);
  discovery.Start();

  // Devices initially known to the service before discovery started should be
  // reported as KNOWN.
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches("known")));
  scoped_task_environment().RunUntilIdle();

  // Devices added during the discovery should be reported as ADDED.
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, IdMatches("added")));
  fake_hid_manager_->AddDevice(MakeFidoHidDevice("added"));
  scoped_task_environment().RunUntilIdle();

  // Added non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorAdded(_, _)).Times(0);
  fake_hid_manager_->AddDevice(MakeOtherDevice("other"));

  // Removed non-U2F devices should not be reported at all.
  EXPECT_CALL(observer, AuthenticatorRemoved(_, _)).Times(0);
  fake_hid_manager_->RemoveDevice("other");
  scoped_task_environment().RunUntilIdle();

  // Removed U2F devices should be reported as REMOVED.
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("known")));
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, IdMatches("added")));
  fake_hid_manager_->RemoveDevice("known");
  fake_hid_manager_->RemoveDevice("added");
  scoped_task_environment().RunUntilIdle();
}

}  // namespace device
