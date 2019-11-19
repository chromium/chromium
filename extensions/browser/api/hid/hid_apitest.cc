// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using base::ThreadTaskRunnerHandle;
using device::FakeHidManager;
using device::HidReportDescriptor;

const char* const kTestDeviceGuids[] = {"A", "B", "C", "D", "E"};

// These report descriptors define two devices with 8-byte input, output and
// feature reports. The first implements usage page 0xFF00 and has a single
// report without and ID. The second implements usage page 0xFF01 and has a
// single report with ID 1.
const uint8_t kReportDescriptor[] = {0x06, 0x00, 0xFF, 0x08, 0xA1, 0x01, 0x15,
                                     0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
                                     0x08, 0x08, 0x81, 0x02, 0x08, 0x91, 0x02,
                                     0x08, 0xB1, 0x02, 0xC0};
const uint8_t kReportDescriptorWithIDs[] = {
    0x06, 0x01, 0xFF, 0x08, 0xA1, 0x01, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x85, 0x01, 0x75, 0x08, 0x95, 0x08, 0x08,
    0x81, 0x02, 0x08, 0x91, 0x02, 0x08, 0xB1, 0x02, 0xC0};

namespace extensions {

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  ~TestDevicePermissionsPrompt() override { prompt()->SetObserver(nullptr); }

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDeviceAdded(size_t index, const base::string16& device_name) override {
    OnDevicesChanged();
  }

  void OnDeviceRemoved(size_t index,
                       const base::string16& device_name) override {
    OnDevicesChanged();
  }

 private:
  void OnDevicesChanged() {
    if (prompt()->multiple()) {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        prompt()->GrantDevicePermission(i);
      }
      prompt()->Dismissed();
    } else {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        // Always choose the device whose serial number is "A".
        if (prompt()->GetDeviceSerialNumber(i) == base::UTF8ToUTF16("A")) {
          prompt()->GrantDevicePermission(i);
          prompt()->Dismissed();
          return;
        }
      }
    }
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return std::make_unique<TestDevicePermissionsPrompt>(web_contents);
  }
};

class HidApiTest : public ShellApiTest {
 public:
  HidApiTest() {
#if defined(OS_CHROMEOS)
    // Required for DevicePermissionsPrompt:
    chromeos::PermissionBrokerClient::InitializeFake();
#endif
    // Because Device Service also runs in this process (browser process), we
    // can set our binder to intercept requests for HidManager interface to it.
    fake_hid_manager_ = std::make_unique<FakeHidManager>();
    service_manager::ServiceBinding::OverrideInterfaceBinderForTesting(
        device::mojom::kServiceName,
        base::Bind(&FakeHidManager::Bind,
                   base::Unretained(fake_hid_manager_.get())));
  }

  ~HidApiTest() override {
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::HidManager>(device::mojom::kServiceName);
#if defined(OS_CHROMEOS)
    chromeos::PermissionBrokerClient::Shutdown();
#endif
  }

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    AddDevice(kTestDeviceGuids[0], 0x18D1, 0x58F0, false, "A");
    AddDevice(kTestDeviceGuids[1], 0x18D1, 0x58F0, true, "B");
    AddDevice(kTestDeviceGuids[2], 0x18D1, 0x58F1, false, "C");
  }

  void AddDevice(const std::string& device_guid,
                 int vendor_id,
                 int product_id,
                 bool report_id,
                 std::string serial_number) {
    std::vector<uint8_t> report_descriptor;
    if (report_id) {
      report_descriptor.insert(
          report_descriptor.begin(), kReportDescriptorWithIDs,
          kReportDescriptorWithIDs + sizeof(kReportDescriptorWithIDs));
    } else {
      report_descriptor.insert(report_descriptor.begin(), kReportDescriptor,
                               kReportDescriptor + sizeof(kReportDescriptor));
    }

    std::vector<device::mojom::HidCollectionInfoPtr> collections;
    bool has_report_id;
    size_t max_input_report_size;
    size_t max_output_report_size;
    size_t max_feature_report_size;

    HidReportDescriptor descriptor_parser(report_descriptor);
    descriptor_parser.GetDetails(
        &collections, &has_report_id, &max_input_report_size,
        &max_output_report_size, &max_feature_report_size);

    auto device = device::mojom::HidDeviceInfo::New(
        device_guid, vendor_id, product_id, "Test Device", serial_number,
        device::mojom::HidBusType::kHIDBusTypeUSB, report_descriptor,
        std::move(collections), has_report_id, max_input_report_size,
        max_output_report_size, max_feature_report_size, "");

    fake_hid_manager_->AddDevice(std::move(device));
  }

  FakeHidManager* GetFakeHidManager() { return fake_hid_manager_.get(); }

 protected:
  std::unique_ptr<FakeHidManager> fake_hid_manager_;
};

IN_PROC_BROWSER_TEST_F(HidApiTest, HidApp) {
  ASSERT_TRUE(RunAppTest("api_test/hid/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Add a blocked device first so that the test will fail if a notification is
  // received.
  AddDevice(kTestDeviceGuids[3], 0x18D1, 0x58F1, false, "A");
  AddDevice(kTestDeviceGuids[4], 0x18D1, 0x58F0, false, "A");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Device C was not returned by chrome.hid.getDevices, the app will not get
  // a notification.
  GetFakeHidManager()->RemoveDevice(kTestDeviceGuids[2]);
  // Device A was returned, the app will get a notification.
  GetFakeHidManager()->RemoveDevice(kTestDeviceGuids[0]);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener open_listener("opened_device", false);

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/hid/get_user_selected_devices"));
  ASSERT_TRUE(open_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener remove_listener("removed", false);
  GetFakeHidManager()->RemoveDevice(kTestDeviceGuids[0]);
  ASSERT_TRUE(remove_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener add_listener("added", false);
  AddDevice(kTestDeviceGuids[0], 0x18D1, 0x58F0, true, "A");
  ASSERT_TRUE(add_listener.WaitUntilSatisfied());
}

}  // namespace extensions
