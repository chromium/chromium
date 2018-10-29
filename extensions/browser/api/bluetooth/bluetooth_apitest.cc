// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
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
using device::MockBluetoothDiscoverySession;
using extensions::Extension;
using extensions::ResultCatcher;

namespace utils = extension_function_test_utils;
namespace api = extensions::api;

namespace {

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
    EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  }

  void SetUpMockAdapter() {
    // The browser will clean this up when it is torn down
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    event_router()->SetAdapterForTest(mock_adapter_);

    device1_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d1", "11:12:13:14:15:16",
        true /* paired */, true /* connected */));
    device2_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d2", "21:22:23:24:25:26",
        false /* paired */, false /* connected */));
    device3_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d3", "31:32:33:34:35:36",
        false /* paired */, false /* connected */));
  }

  void DiscoverySessionCallback(
      const BluetoothAdapter::DiscoverySessionCallback& callback,
      const BluetoothAdapter::ErrorCallback& error_callback) {
    if (mock_session_.get()) {
      callback.Run(
          std::unique_ptr<BluetoothDiscoverySession>(mock_session_.release()));
      return;
    }
    error_callback.Run();
  }

  template <class T>
  T* setupFunction(T* function) {
    function->set_extension(empty_extension_.get());
    function->set_has_callback(true);
    return function;
  }

 protected:
  testing::StrictMock<MockBluetoothAdapter>* mock_adapter_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDiscoverySession>>
      mock_session_;
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
};

static void StopDiscoverySessionCallback(const base::Closure& callback,
                                         const base::Closure& error_callback) {
  callback.Run();
}

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

  std::unique_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      get_adapter_state.get(), "[]", browser()));
  ASSERT_TRUE(result.get() != NULL);
  api::bluetooth::AdapterState state;
  ASSERT_TRUE(api::bluetooth::AdapterState::Populate(*result, &state));

  EXPECT_FALSE(state.available);
  EXPECT_TRUE(state.powered);
  EXPECT_FALSE(state.discovering);
  EXPECT_EQ(kName, state.name);
  EXPECT_EQ(kAdapterAddress, state.address);
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DeviceEvents) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/device_events")));

  ExtensionTestMessageListener events_received("ready", true);
  event_router()->DeviceAdded(mock_adapter_, device1_.get());
  event_router()->DeviceAdded(mock_adapter_, device2_.get());

  EXPECT_CALL(*device2_, GetName())
      .WillRepeatedly(
          testing::Return(base::Optional<std::string>("the real d2")));
  EXPECT_CALL(*device2_, GetNameForDisplay())
      .WillRepeatedly(testing::Return(base::UTF8ToUTF16("the real d2")));
  event_router()->DeviceChanged(mock_adapter_, device2_.get());

  event_router()->DeviceAdded(mock_adapter_, device3_.get());
  event_router()->DeviceRemoved(mock_adapter_, device1_.get());
  EXPECT_TRUE(events_received.WaitUntilSatisfied());
  events_received.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, Discovery) {
  // Try with a failure to start. This will return an error as we haven't
  // initialied a session object.
  EXPECT_CALL(*mock_adapter_, StartDiscoverySession(testing::_, testing::_))
      .WillOnce(
          testing::Invoke(this, &BluetoothApiTest::DiscoverySessionCallback));

  // StartDiscovery failure will not reference the adapter.
  scoped_refptr<api::BluetoothStartDiscoveryFunction> start_function;
  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  std::string error(
      utils::RunFunctionAndReturnError(start_function.get(), "[]", browser()));
  ASSERT_FALSE(error.empty());

  // Reset the adapter and initiate a discovery session. The ownership of the
  // mock session will be passed to the event router.
  ASSERT_FALSE(mock_session_.get());
  SetUpMockAdapter();

  // Create a mock session to be returned as a result. Get a handle to it as
  // its ownership will be passed and |mock_session_| will be reset.
  mock_session_.reset(new testing::NiceMock<MockBluetoothDiscoverySession>());
  MockBluetoothDiscoverySession* session = mock_session_.get();
  EXPECT_CALL(*mock_adapter_, StartDiscoverySession(testing::_, testing::_))
      .WillOnce(
          testing::Invoke(this, &BluetoothApiTest::DiscoverySessionCallback));
  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  utils::RunFunction(start_function.get(), "[]", browser(),
                     extensions::api_test_utils::NONE);

  // End the discovery session. The StopDiscovery function should succeed.
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  EXPECT_CALL(*session, IsActive()).WillOnce(testing::Return(true));
  EXPECT_CALL(*session, Stop(testing::_, testing::_))
      .WillOnce(testing::Invoke(StopDiscoverySessionCallback));

  // StopDiscovery success will remove the session object, unreferencing the
  // adapter.
  scoped_refptr<api::BluetoothStopDiscoveryFunction> stop_function;
  stop_function = setupFunction(new api::BluetoothStopDiscoveryFunction);
  (void) utils::RunFunctionAndReturnSingleResult(
      stop_function.get(), "[]", browser());

  // Reset the adapter. Simulate failure for stop discovery. The event router
  // still owns the session. Make it appear inactive.
  SetUpMockAdapter();
  EXPECT_CALL(*session, IsActive()).WillOnce(testing::Return(false));
  stop_function = setupFunction(new api::BluetoothStopDiscoveryFunction);
  error =
      utils::RunFunctionAndReturnError(stop_function.get(), "[]", browser());
  ASSERT_FALSE(error.empty());
  SetUpMockAdapter();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DiscoveryCallback) {
  mock_session_.reset(new testing::NiceMock<MockBluetoothDiscoverySession>());
  MockBluetoothDiscoverySession* session = mock_session_.get();
  EXPECT_CALL(*mock_adapter_, StartDiscoverySession(testing::_, testing::_))
      .WillOnce(
          testing::Invoke(this, &BluetoothApiTest::DiscoverySessionCallback));
  EXPECT_CALL(*session, IsActive()).WillOnce(testing::Return(true));
  EXPECT_CALL(*session, Stop(testing::_, testing::_))
      .WillOnce(testing::Invoke(StopDiscoverySessionCallback));

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener discovery_started("ready", true);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_callback")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready", true);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
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

  mock_session_.reset(new testing::NiceMock<MockBluetoothDiscoverySession>());
  MockBluetoothDiscoverySession* session = mock_session_.get();
  EXPECT_CALL(*mock_adapter_, StartDiscoverySession(testing::_, testing::_))
      .WillOnce(
          testing::Invoke(this, &BluetoothApiTest::DiscoverySessionCallback));
  EXPECT_CALL(*session, IsActive()).WillOnce(testing::Return(true));
  EXPECT_CALL(*session, Stop(testing::_, testing::_))
      .WillOnce(testing::Invoke(StopDiscoverySessionCallback));

  ExtensionTestMessageListener discovery_started("ready", true);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_in_progress")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  // Only this should be received. No additional notification should be sent for
  // devices discovered before the discovery session started.
  event_router()->DeviceAdded(mock_adapter_, device2_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready", true);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
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
  ExtensionTestMessageListener listener("ready", true);
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
  ExtensionTestMessageListener listener("ready", true);
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
      .WillRepeatedly(testing::Return(static_cast<BluetoothDevice*>(NULL)));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
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
          testing::Return(base::Optional<std::string>("Chromebook Pixel")));
  EXPECT_CALL(*device1_, GetNameForDisplay())
      .WillRepeatedly(testing::Return(base::UTF8ToUTF16("Chromebook Pixel")));
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
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/device_info")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
