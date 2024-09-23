// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothDeviceType;
using device::BluetoothDiscoverySession;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using extensions::Extension;
using extensions::ResultCatcher;

namespace utils = extensions::api_test_utils;
namespace api = extensions::api;

namespace {

using testing::_;
using testing::Invoke;

static const char* kAdapterAddress = "A1:A2:A3:A4:A5:A6";
static const char* kName = "whatsinaname";

class BluetoothApiTest : public extensions::ExtensionApiTest {
 public:
  BluetoothApiTest() {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    empty_extension_ = extensions::ExtensionBuilder("Test").Build();
    SetUpMockAdapter();
  }

  void TearDownOnMainThread() override {
    EXPECT_CALL(*mock_adapter_, RemoveObserver(_));
  }

  void SetUpMockAdapter() {
    // The browser will clean this up when it is torn down
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    event_router()->SetAdapterForTest(mock_adapter_);

    device1_ = std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
        mock_adapter_, 0, "d1", "11:12:13:14:15:16", true /* paired */,
        true /* connected */);
    device2_ = std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
        mock_adapter_, 0, "d2", "21:22:23:24:25:26", false /* paired */,
        false /* connected */);
    device3_ = std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
        mock_adapter_, 0, "d3", "31:32:33:34:35:36", false /* paired */,
        false /* connected */);
  }

  void StartScanOverride(
      const device::BluetoothDiscoveryFilter* filter,
      base::OnceCallback<void(/*is_error*/ bool,
                              device::UMABluetoothDiscoverySessionOutcome)>&
          callback) {
    if (fail_next_call_) {
      std::move(callback).Run(
          true, device::UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      fail_next_call_ = false;
      return;
    }
    std::move(callback).Run(
        false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

  void StopScanOverride(
      device::BluetoothAdapter::DiscoverySessionResultCallback callback) {
    if (fail_next_call_) {
      std::move(callback).Run(
          /*is_error=*/true,
          device::UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      fail_next_call_ = false;
      return;
    }
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

  void FailNextCall() { fail_next_call_ = true; }

  template <class T>
  T* setupFunction(T* function) {
    function->set_extension(empty_extension_.get());
    function->set_has_callback(true);
    return function;
  }

 protected:
  raw_ptr<testing::StrictMock<MockBluetoothAdapter>,
          AcrossTasksDanglingUntriaged>
      mock_adapter_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDevice>> device1_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDevice>> device2_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDevice>> device3_;

  extensions::BluetoothEventRouter* event_router() {
    return bluetooth_api()->event_router();
  }

  extensions::BluetoothAPI* bluetooth_api() {
    return extensions::BluetoothAPI::Get(browser()->profile());
  }

 private:
  scoped_refptr<const Extension> empty_extension_;
  bool fail_next_call_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetAdapterState) {
  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(false));

  scoped_refptr<api::BluetoothGetAdapterStateFunction> get_adapter_state;
  get_adapter_state = setupFunction(new api::BluetoothGetAdapterStateFunction);

  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      get_adapter_state.get(), "[]", browser()->profile());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  auto state = api::bluetooth::AdapterState::FromValue(result->GetDict());
  ASSERT_TRUE(state);

  EXPECT_FALSE(state->available);
  EXPECT_TRUE(state->powered);
  EXPECT_FALSE(state->discovering);
  EXPECT_EQ(kName, state->name);
  EXPECT_EQ(kAdapterAddress, state->address);
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DeviceEvents) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/device_events")));

  ExtensionTestMessageListener events_received("ready",
                                               ReplyBehavior::kWillReply);
  event_router()->DeviceAdded(mock_adapter_, device1_.get());
  event_router()->DeviceAdded(mock_adapter_, device2_.get());

  EXPECT_CALL(*device2_, GetName())
      .WillRepeatedly(
          testing::Return(std::optional<std::string>("the real d2")));
  EXPECT_CALL(*device2_, GetNameForDisplay())
      .WillRepeatedly(testing::Return(u"the real d2"));
  event_router()->DeviceChanged(mock_adapter_, device2_.get());

  event_router()->DeviceAdded(mock_adapter_, device3_.get());
  event_router()->DeviceRemoved(mock_adapter_, device1_.get());
  EXPECT_TRUE(events_received.WaitUntilSatisfied());
  events_received.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, Discovery) {
  // Simulate start discovery failure
  EXPECT_CALL(*mock_adapter_, StartScanWithFilter_(_, _))
      .WillOnce(Invoke(this, &BluetoothApiTest::StartScanOverride));
  FailNextCall();
  scoped_refptr<api::BluetoothStartDiscoveryFunction> start_function;
  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  std::string error(utils::RunFunctionAndReturnError(start_function.get(), "[]",
                                                     browser()->profile()));

  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  // Simulate successful start discovery
  EXPECT_CALL(*mock_adapter_, StartScanWithFilter_(_, _))
      .WillOnce(Invoke(this, &BluetoothApiTest::StartScanOverride));
  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  utils::RunFunction(start_function.get(), "[]", browser()->profile(),
                     extensions::api_test_utils::FunctionMode::kNone);

  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  // Simulate stop discovery with a failure
  EXPECT_CALL(*mock_adapter_, StopScan(_))
      .WillOnce(Invoke(this, &BluetoothApiTest::StopScanOverride));
  FailNextCall();
  scoped_refptr<api::BluetoothStopDiscoveryFunction> stop_function;
  stop_function = setupFunction(new api::BluetoothStopDiscoveryFunction);
  [[maybe_unused]] auto result = utils::RunFunctionAndReturnSingleResult(
      stop_function.get(), "[]", browser()->profile());
  SetUpMockAdapter();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DiscoveryCallback) {
  EXPECT_CALL(*mock_adapter_, StartScanWithFilter_(_, _))
      .WillOnce(Invoke(this, &BluetoothApiTest::StartScanOverride));
  EXPECT_CALL(*mock_adapter_, StopScan(_))
      .WillOnce(Invoke(this, &BluetoothApiTest::StopScanOverride));
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener discovery_started("ready",
                                                 ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_callback")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready",
                                                 ReplyBehavior::kWillReply);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_));
  EXPECT_TRUE(discovery_stopped.WaitUntilSatisfied());

  SetUpMockAdapter();
  event_router()->DeviceAdded(mock_adapter_, device2_.get());
  discovery_stopped.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DiscoveryInProgress) {
  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));

  // Fake that the adapter is discovering
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterDiscoveringChanged(mock_adapter_, true);

  // Cache a device before the extension starts discovering
  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, StartScanWithFilter_(_, _))
      .WillOnce(Invoke(this, &BluetoothApiTest::StartScanOverride));
  EXPECT_CALL(*mock_adapter_, StopScan(_))
      .WillOnce(Invoke(this, &BluetoothApiTest::StopScanOverride));

  ExtensionTestMessageListener discovery_started("ready",
                                                 ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_in_progress")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  // Only this should be received. No additional notification should be sent for
  // devices discovered before the discovery session started.
  event_router()->DeviceAdded(mock_adapter_, device2_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready",
                                                 ReplyBehavior::kWillReply);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_));
  EXPECT_TRUE(discovery_stopped.WaitUntilSatisfied());

  SetUpMockAdapter();
  // This should never be received.
  event_router()->DeviceAdded(mock_adapter_, device2_.get());
  discovery_stopped.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, OnAdapterStateChanged) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(
          test_data_dir_.AppendASCII("bluetooth/on_adapter_state_changed")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(false));
  event_router()->AdapterPoweredChanged(mock_adapter_, false);

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterPresentChanged(mock_adapter_, true);

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterDiscoveringChanged(mock_adapter_, true);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetDevices) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  BluetoothAdapter::ConstDeviceList devices;
  devices.push_back(device1_.get());
  devices.push_back(device2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevices())
      .Times(1)
      .WillRepeatedly(testing::Return(devices));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/get_devices")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetDevice) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(device1_->GetAddress()))
      .WillOnce(testing::Return(device1_.get()));
  EXPECT_CALL(*mock_adapter_, GetDevice(device2_->GetAddress()))
      .Times(1)
      .WillRepeatedly(testing::Return(static_cast<BluetoothDevice*>(nullptr)));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/get_device")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DeviceInfo) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Set up the first device object to reflect a real-world device.
  BluetoothAdapter::ConstDeviceList devices;

  EXPECT_CALL(*device1_, GetAddress())
      .WillRepeatedly(testing::Return("A4:17:31:00:00:00"));
  EXPECT_CALL(*device1_, GetName())
      .WillRepeatedly(
          testing::Return(std::optional<std::string>("Chromebook Pixel")));
  EXPECT_CALL(*device1_, GetNameForDisplay())
      .WillRepeatedly(testing::Return(u"Chromebook Pixel"));
  EXPECT_CALL(*device1_, GetBluetoothClass())
      .WillRepeatedly(testing::Return(0x080104));
  EXPECT_CALL(*device1_, GetDeviceType())
      .WillRepeatedly(testing::Return(BluetoothDeviceType::COMPUTER));
  EXPECT_CALL(*device1_, GetVendorIDSource())
      .WillRepeatedly(testing::Return(BluetoothDevice::VENDOR_ID_BLUETOOTH));
  EXPECT_CALL(*device1_, GetVendorID()).WillRepeatedly(testing::Return(0x00E0));
  EXPECT_CALL(*device1_, GetProductID())
      .WillRepeatedly(testing::Return(0x240A));
  EXPECT_CALL(*device1_, GetDeviceID()).WillRepeatedly(testing::Return(0x0400));

  BluetoothDevice::UUIDSet uuids;
  uuids.insert(BluetoothUUID("1105"));
  uuids.insert(BluetoothUUID("1106"));

  EXPECT_CALL(*device1_, GetUUIDs()).WillOnce(testing::Return(uuids));

  devices.push_back(device1_.get());

  // Leave the second largely empty so we can check a device without
  // available information.
  devices.push_back(device2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevices())
      .Times(1)
      .WillRepeatedly(testing::Return(devices));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/device_info")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
