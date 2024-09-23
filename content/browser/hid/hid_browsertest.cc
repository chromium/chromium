// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "content/browser/hid/hid_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

using testing::ByMove;
using testing::Exactly;
using testing::Return;

namespace content {

namespace {

// Create a device with a single collection containing an input report and an
// output report. Both reports have report ID 0.
device::mojom::HidDeviceInfoPtr CreateTestDeviceWithInputAndOutputReports() {
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(0x0001, 0xff00);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());

  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = "test-guid";
  device->collections.push_back(std::move(collection));
  return device;
}

class HidBrowserTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  MockHidDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  HidDelegate* GetHidDelegate() override { return &delegate_; }

 private:
  MockHidDelegate delegate_;
};

class HidTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    test_client_ = std::make_unique<HidBrowserTestContentBrowserClient>();
    ON_CALL(delegate(), GetHidManager).WillByDefault(Return(&hid_manager_));
  }

  void TearDownOnMainThread() override { test_client_.reset(); }

  MockHidDelegate& delegate() { return test_client_->delegate(); }
  device::FakeHidManager* hid_manager() { return &hid_manager_; }

 private:
  std::unique_ptr<HidBrowserTestContentBrowserClient> test_client_;
  device::FakeHidManager hid_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HidTest, GetDevices) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  // Three devices are added but only two will have permission granted.
  for (int i = 0; i < 3; i++) {
    auto device = CreateTestDeviceWithInputAndOutputReports();
    device->guid = base::StringPrintf("test-guid-%02d", i);
    hid_manager()->AddDevice(std::move(device));
  }

  EXPECT_CALL(delegate(), HasDevicePermission)
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_EQ(
      2,
      EvalJs(shell(),
             R"(navigator.hid.getDevices().then(devices => devices.length))"));
}

IN_PROC_BROWSER_TEST_F(HidTest, RequestDevice) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  EXPECT_CALL(delegate(), CanRequestDevicePermission).WillOnce(Return(true));

  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  devices.push_back(CreateTestDeviceWithInputAndOutputReports());
  EXPECT_CALL(delegate(), RunChooserInternal)
      .WillOnce(Return(ByMove(std::move(devices))));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let devices = await navigator.hid.requestDevice({filters:[]});
               return devices instanceof Array
                      && devices.length == 1
                      && devices[0] instanceof HIDDevice;
             })())"));
}

IN_PROC_BROWSER_TEST_F(HidTest, DisallowRequestDevice) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  EXPECT_CALL(delegate(), CanRequestDevicePermission).WillOnce(Return(false));
  EXPECT_CALL(delegate(), RunChooserInternal).Times(Exactly(0));

  EXPECT_EQ(0, EvalJs(shell(),
                      R"((async () => {
               let devices = await navigator.hid.requestDevice({filters:[]});
               return devices.length;
             })())"));
}

IN_PROC_BROWSER_TEST_F(HidTest, ProtectedReportsAreFiltered) {
  LOG(ERROR) << "HidTest.ProtectedReportsAreFiltered";
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  auto device = CreateTestDeviceWithInputAndOutputReports();

  // Mark the input report as protected.
  device->protected_input_report_ids = std::vector<uint8_t>{0};

  hid_manager()->AddDevice(std::move(device));

  EXPECT_CALL(delegate(), HasDevicePermission).WillOnce(Return(true));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
             let devices = await navigator.hid.getDevices();
             return devices instanceof Array
                    && devices.length == 1
                    && devices[0] instanceof HIDDevice
                    && devices[0].collections instanceof Array
                    && devices[0].collections.length == 1
                    && devices[0].collections[0].inputReports instanceof Array
                    && devices[0].collections[0].inputReports.length == 0
                    && devices[0].collections[0].outputReports instanceof Array
                    && devices[0].collections[0].outputReports.length == 1;
           })())"));
}

IN_PROC_BROWSER_TEST_F(HidTest, DeviceWithAllProtectedReportsIsExcluded) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  auto device = CreateTestDeviceWithInputAndOutputReports();

  // Mark both the input and output reports as protected.
  device->protected_input_report_ids = std::vector<uint8_t>{0};
  device->protected_output_report_ids = std::vector<uint8_t>{0};

  hid_manager()->AddDevice(std::move(device));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let devices = await navigator.hid.getDevices();
               return devices instanceof Array && devices.length == 0;
             })())"));
}

class HidFencedFramesBrowserTest : public HidTest {
 public:
  HidFencedFramesBrowserTest() = default;
  HidFencedFramesBrowserTest(const HidFencedFramesBrowserTest&) = delete;
  HidFencedFramesBrowserTest& operator=(const HidFencedFramesBrowserTest&) =
      delete;
  ~HidFencedFramesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    HidTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  WebContents* GetWebContents() { return shell()->web_contents(); }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(HidFencedFramesBrowserTest, BlockFromFencedFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/simple_page.html")));

  // Three devices are added but only two will have permission granted.
  for (int i = 0; i < 3; i++) {
    auto device = CreateTestDeviceWithInputAndOutputReports();
    device->guid = base::StringPrintf("test-guid-%02d", i);
    hid_manager()->AddDevice(std::move(device));
  }

  EXPECT_CALL(delegate(), HasDevicePermission)
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_EQ(
      2,
      EvalJs(shell(),
             R"(navigator.hid.getDevices().then(devices => devices.length))"));

  // Loads a fenced frame
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/fenced_frames/empty.html");
  content::RenderFrameHost* render_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  ASSERT_NE(nullptr, render_frame_host);

  // Tries to request a device from the fenced frame, which must cause an error.
  constexpr char kFencedFrameError[] =
      "Access to the feature \"hid\" is disallowed by permissions policy.";
  auto result = content::EvalJs(
      render_frame_host,
      R"(navigator.hid.getDevices().then(devices => devices.length))");
  EXPECT_THAT(result.error, ::testing::HasSubstr(kFencedFrameError));
}

}  // namespace content
