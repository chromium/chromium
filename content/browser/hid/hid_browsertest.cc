// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "content/browser/hid/hid_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

using testing::_;
using testing::ByMove;
using testing::Exactly;
using testing::Return;

namespace content {

namespace {

class HidTest : public ContentBrowserTest {
 public:
  HidTest() {
    ON_CALL(delegate(), GetHidManager).WillByDefault(Return(&hid_manager_));
  }

  ~HidTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    original_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDownOnMainThread() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  MockHidDelegate& delegate() { return test_client_.delegate(); }
  device::FakeHidManager* hid_manager() { return &hid_manager_; }

 private:
  HidTestContentBrowserClient test_client_;
  ContentBrowserClient* original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HidTest, GetDevices) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  // Three devices are added but only two will have permission granted.
  for (int i = 0; i < 3; i++) {
    auto device = device::mojom::HidDeviceInfo::New();
    device->guid = base::StringPrintf("test-guid-%02d", i);
    hid_manager()->AddDevice(std::move(device));
  }

  EXPECT_CALL(delegate(), HasDevicePermission(_, _, _))
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

  EXPECT_CALL(delegate(), CanRequestDevicePermission(_, _))
      .WillOnce(Return(true));

  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = "test-guid";
  EXPECT_CALL(delegate(), RunChooserInternal)
      .WillOnce(Return(ByMove(std::move(device))));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let device = await navigator.hid.requestDevice({filters:[]});
               return device instanceof HIDDevice;
             })())"));
}

IN_PROC_BROWSER_TEST_F(HidTest, DisallowRequestDevice) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  EXPECT_CALL(delegate(), CanRequestDevicePermission(_, _))
      .WillOnce(Return(false));
  EXPECT_CALL(delegate(), RunChooserInternal).Times(Exactly(0));

  EXPECT_EQ(false, EvalJs(shell(),
                          R"((async () => {
                            try {
                              await navigator.hid.requestDevice({filters:[]});
                              return true;
                            } catch (e) {
                              return false;
                            }
                          })())"));
}

}  // namespace content
