// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/browser_interface_binders.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/device/public/cpp/test/fake_device_posture_provider.h"
#include "services/device/public/mojom/device_posture_provider.mojom.h"

namespace content {

namespace {

using device::FakeDevicePostureProvider;

class DevicePostureBrowserTest : public ContentBrowserTest {
 public:
  DevicePostureBrowserTest() {
    scoped_feature_list_.InitWithFeatures({features::kDevicePosture}, {});

    OverrideDevicePostureProviderBinderForTesting(base::BindRepeating(
        &DevicePostureBrowserTest::BindDevicePostureProviderReceiver,
        base::Unretained(this)));
    fake_device_posture_provider_ =
        std::make_unique<FakeDevicePostureProvider>();
  }

  ~DevicePostureBrowserTest() override {
    OverrideDevicePostureProviderBinderForTesting(base::NullCallback());
  }

  DevicePostureBrowserTest(const DevicePostureBrowserTest&) = delete;
  DevicePostureBrowserTest& operator=(const DevicePostureBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    https_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_embedded_test_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_embedded_test_server_.get());
    https_embedded_test_server_->ServeFilesFromSourceDirectory(
        "content/test/data/");
    https_embedded_test_server_->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void BindDevicePostureProviderReceiver(
      mojo::PendingReceiver<device::mojom::DevicePostureProvider> receiver) {
    fake_device_posture_provider_->Bind(std::move(receiver));
  }

  void set_current_posture(const device::mojom::DevicePostureType& posture) {
    fake_device_posture_provider_->SetCurrentPostureForTesting(posture);
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_embedded_test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeDevicePostureProvider> fake_device_posture_provider_;
};

IN_PROC_BROWSER_TEST_F(DevicePostureBrowserTest, GetPostureDefault) {
  // This basic test will ensure that the default posture is working.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  EXPECT_EQ("continuous", EvalJs(shell(), "navigator.devicePosture.type"));
}

IN_PROC_BROWSER_TEST_F(DevicePostureBrowserTest, PostureChangeEventTest) {
  // This test will emulate a posture change and verify that the JavaScript
  // event handler is properly called and that the new posture has the correct
  // value.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  EXPECT_EQ("continuous", EvalJs(shell(), "navigator.devicePosture.type"));
  EXPECT_EQ(true, ExecJs(shell(),
                         R"(
                           var postureReceived = new Promise(resolve => {
                             navigator.devicePosture.onchange = function() {
                               resolve(navigator.devicePosture.type);
                              }
                           });
                          )"));
  set_current_posture(device::mojom::DevicePostureType::kFolded);
  EXPECT_EQ("folded", EvalJs(shell(), "postureReceived"));
}

IN_PROC_BROWSER_TEST_F(DevicePostureBrowserTest, PostureAddEventListenerTest) {
  // This test will emulate a posture change and verify that the JavaScript
  // event handler is properly called and that the new posture has the correct
  // value.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  EXPECT_EQ("continuous", EvalJs(shell(), "navigator.devicePosture.type"));
  EXPECT_EQ(true, ExecJs(shell(),
                         R"(
                           var postureReceived = new Promise(resolve => {
                             navigator.devicePosture.addEventListener(
                               "change",
                               () => { resolve(navigator.devicePosture.type); }
                              );
                            });
                          )"));
  set_current_posture(device::mojom::DevicePostureType::kFoldedOver);
  EXPECT_EQ("folded-over", EvalJs(shell(), "postureReceived"));
}

}  //  namespace

}  // namespace content
