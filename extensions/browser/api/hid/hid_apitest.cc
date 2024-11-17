// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

namespace {

using ::device::FakeHidManager;
using ::device::HidReportDescriptor;

const char* const kTestDeviceGuids[] = {"A", "B", "C", "D", "E"};
const char* const kTestPhysicalDeviceIds[] = {"1", "2", "3", "4", "5"};

// These report descriptors define two devices with 8-byte input, output and
// feature reports. The first implements usage page 0xFF00 and has a single
// report without and ID. The second implements usage page 0xFF01 and has a
// single report with ID 1.
const uint8_t kReportDescriptor[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x08,              // Usage
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x08,        //   Report Count (8)
    0x08,              //   Usage
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                       //   No Null Position)
    0x08,              //   Usage
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,
                       //   No Null Position,Non-volatile)
    0x08,              //   Usage
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                       //   State,No Null Position,Non-volatile)
    0xC0,              // End Collection
};
const uint8_t kReportDescriptorWithIDs[] = {
    0x06, 0x01, 0xFF,  // Usage Page (Vendor Defined 0xFF01)
    0x08,              // Usage
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x85, 0x01,        //   Report ID (1)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x08,        //   Report Count (8)
    0x08,              //   Usage
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                       //   No Null Position)
    0x08,              //   Usage
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,
                       //   No Null Position,Non-volatile)
    0x08,              //   Usage
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                       //   State,No Null Position,Non-volatile)
    0xC0,              // End Collection
};

// Device IDs for the device granted permission by the manifest.
constexpr uint16_t kTestVendorId = 0x18D1;
constexpr uint16_t kTestProductId = 0x58F0;

}  // namespace

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  ~TestDevicePermissionsPrompt() override { prompt()->SetObserver(nullptr); }

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDevicesInitialized() override {
    if (prompt()->multiple()) {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        prompt()->GrantDevicePermission(i);
      }
      prompt()->Dismissed();
    } else {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        // Always choose the device whose serial number is "A".
        if (prompt()->GetDeviceSerialNumber(i) == u"A") {
          prompt()->GrantDevicePermission(i);
          prompt()->Dismissed();
          return;
        }
      }
    }
  }

  void OnDeviceAdded(size_t index, const std::u16string& device_name) override {
  }

  void OnDeviceRemoved(size_t index,
                       const std::u16string& device_name) override {}
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
#if BUILDFLAG(IS_CHROMEOS)
    // Required for DevicePermissionsPrompt:
    chromeos::PermissionBrokerClient::InitializeFake();
#endif
    // Because Device Service also runs in this process (browser process), we
    // can set our binder to intercept requests for HidManager interface to it.
    fake_hid_manager_ = std::make_unique<FakeHidManager>();
    auto binder = base::BindRepeating(
        &FakeHidManager::Bind, base::Unretained(fake_hid_manager_.get()));
    HidDeviceManager::OverrideHidManagerBinderForTesting(binder);
    DevicePermissionsPrompt::OverrideHidManagerBinderForTesting(binder);
  }

  ~HidApiTest() override {
    HidDeviceManager::OverrideHidManagerBinderForTesting(base::NullCallback());
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::PermissionBrokerClient::Shutdown();
#endif
  }

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    AddDevice(kTestDeviceGuids[0], kTestPhysicalDeviceIds[0], kTestVendorId,
              kTestProductId, false, "A");
    AddDevice(kTestDeviceGuids[1], kTestPhysicalDeviceIds[1], kTestVendorId,
              kTestProductId, true, "B");
    AddDevice(kTestDeviceGuids[2], kTestPhysicalDeviceIds[2], kTestVendorId,
              kTestProductId + 1, false, "C");
  }

  void AddDevice(const std::string& device_guid,
                 const std::string& physical_device_id,
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
        device_guid, physical_device_id, vendor_id, product_id, "Test Device",
        serial_number, device::mojom::HidBusType::kHIDBusTypeUSB,
        report_descriptor, std::move(collections), has_report_id,
        max_input_report_size, max_output_report_size, max_feature_report_size,
        /*device_path=*/"",
        /*protected_input_report_ids=*/std::vector<uint8_t>{},
        /*protected_output_report_ids=*/std::vector<uint8_t>{},
        /*protected_feature_report_ids=*/std::vector<uint8_t>{});

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
  ExtensionTestMessageListener load_listener("loaded");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Add a blocked device first so that the test will fail if a notification is
  // received.
  AddDevice(kTestDeviceGuids[3], kTestPhysicalDeviceIds[3], kTestVendorId,
            kTestProductId + 1, false, "A");
  AddDevice(kTestDeviceGuids[4], kTestPhysicalDeviceIds[4], kTestVendorId,
            kTestProductId, false, "A");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded");
  ExtensionTestMessageListener result_listener("success");
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

namespace {

device::mojom::HidDeviceInfoPtr CreateDeviceWithOneCollection(
    const std::string& guid) {
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = guid;
  device_info->vendor_id = kTestVendorId;
  device_info->product_id = kTestProductId;
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage =
      device::mojom::HidUsageAndPage::New(1, device::mojom::kPageVendor);
  auto report = device::mojom::HidReportDescription::New();
  collection->input_reports.push_back(std::move(report));
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoCollections(
    const std::string guid) {
  auto device_info = CreateDeviceWithOneCollection(guid);
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage =
      device::mojom::HidUsageAndPage::New(2, device::mojom::kPageVendor);
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(HidApiTest, DeviceAddedChangedRemoved) {
  constexpr char kTestGuid[] = "guid";

  ExtensionTestMessageListener load_listener("loaded");
  ExtensionTestMessageListener add_listener("added", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener change_listener("changed");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/add_change_remove"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Add a device with one collection.
  GetFakeHidManager()->AddDevice(CreateDeviceWithOneCollection(kTestGuid));
  ASSERT_TRUE(add_listener.WaitUntilSatisfied());

  // Update the device info to add a second collection. No event is generated,
  // so we will reply to the |add_listener| to signal to the test that the
  // change is complete.
  GetFakeHidManager()->ChangeDevice(CreateDeviceWithTwoCollections(kTestGuid));
  add_listener.Reply("device info updated");
  ASSERT_TRUE(change_listener.WaitUntilSatisfied());

  // Remove the device.
  GetFakeHidManager()->RemoveDevice(kTestGuid);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

}  // namespace extensions
