// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_context.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId[] = "test extension id";
const device::BluetoothUUID kAudioProfileUuid("1234");
const device::BluetoothUUID kHealthProfileUuid("4321");

MATCHER_P(IsFilterEqual, a, "") {
  return arg.Equals(*a);
}
}  // namespace

namespace extensions {

namespace bluetooth = api::bluetooth;

class BluetoothEventRouterTest : public ExtensionsTest {
 public:
  BluetoothEventRouterTest()
      : mock_adapter_(new testing::StrictMock<device::MockBluetoothAdapter>()) {
  }

  void SetUp() override {
    ExtensionsTest::SetUp();
    router_ = std::make_unique<BluetoothEventRouter>(browser_context());
    router_->SetAdapterForTest(mock_adapter_);
  }

  void TearDown() override {
    // It's important to destroy the router before the browser context keyed
    // services so it removes itself as an ExtensionRegistry observer.
    router_.reset(NULL);
    ExtensionsTest::TearDown();
  }

 protected:
  testing::StrictMock<device::MockBluetoothAdapter>* mock_adapter_;
  std::unique_ptr<BluetoothEventRouter> router_;
};

TEST_F(BluetoothEventRouterTest, BluetoothEventListener) {
  EventListenerInfo info("", "", GURL(), nullptr);
  router_->OnListenerAdded(info);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_->OnListenerRemoved(info);
}

TEST_F(BluetoothEventRouterTest, MultipleBluetoothEventListeners) {
  // TODO(rkc/stevenjb): Test multiple extensions and WebUI.
  EventListenerInfo info("", "", GURL(), nullptr);
  router_->OnListenerAdded(info);
  router_->OnListenerAdded(info);
  router_->OnListenerAdded(info);
  router_->OnListenerRemoved(info);
  router_->OnListenerRemoved(info);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_->OnListenerRemoved(info);
}

TEST_F(BluetoothEventRouterTest, UnloadExtension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "BT event router test")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Build())
          .SetID(kTestExtensionId)
          .Build();

  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUnloaded(extension.get(), UnloadedExtensionReason::DISABLE);

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

// This test check that calling SetDiscoveryFilter before StartDiscoverySession
// for given extension will start session with proper filter.
TEST_F(BluetoothEventRouterTest, SetDiscoveryFilter) {
  auto discovery_filter = std::make_unique<device::BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  discovery_filter->SetRSSI(-80);
  device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
  device_filter.uuids.insert(device::BluetoothUUID("1000"));
  discovery_filter->AddDeviceFilter(std::move(device_filter));

  device::BluetoothDiscoveryFilter df(device::BLUETOOTH_TRANSPORT_LE);
  df.CopyFrom(*discovery_filter);

  router_->SetDiscoveryFilter(std::move(discovery_filter), mock_adapter_,
                              kTestExtensionId, base::DoNothing(),
                              base::DoNothing());

  EXPECT_CALL(
      *mock_adapter_,
      StartScanWithFilter_(testing::Pointee(IsFilterEqual(&df)), testing::_))
      .WillOnce(testing::Invoke(
          [](const device::BluetoothDiscoveryFilter* filter,
             base::OnceCallback<void(
                 /*is_error*/ bool,
                 device::UMABluetoothDiscoverySessionOutcome)>& callback) {
            std::move(callback).Run(
                false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
          }));

  // RemoveDiscoverySession will be called when the BluetoothDiscoverySession
  // is destroyed
  EXPECT_CALL(*mock_adapter_, StopScan(testing::_)).Times(1);

  router_->StartDiscoverySession(mock_adapter_, kTestExtensionId,
                                 base::DoNothing(), base::DoNothing());

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

}  // namespace extensions
