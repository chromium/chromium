// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/browser_interface_binders.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
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
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_embedded_test_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_embedded_test_server_.get());
    https_embedded_test_server_->ServeFilesFromSourceDirectory(
        "content/test/data/");
    https_embedded_test_server_->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
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
  set_current_posture(device::mojom::DevicePostureType::kFolded);
  EXPECT_EQ("folded", EvalJs(shell(), "postureReceived"));
}

IN_PROC_BROWSER_TEST_F(DevicePostureBrowserTest, PostureMediaQueries) {
  // This test will check that device posture MQs are evaluated correctly.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  EXPECT_EQ("continuous", EvalJs(shell(), "navigator.devicePosture.type"));
  EXPECT_EQ(
      true,
      EvalJs(shell(),
             R"(window.matchMedia('(device-posture: continuous)').matches)"));
  EXPECT_EQ(false, EvalJs(shell(), R"(
    var foldedMQL = window.matchMedia('(device-posture: folded)');
    foldedMQL.matches;
  )"));

  EXPECT_EQ(true, ExecJs(shell(), R"(
    var mediaQueryPostureChanged = new Promise(resolve => {
      foldedMQL.addEventListener('change', () => {
        resolve(foldedMQL.matches);
      });
    });
  )"));

  set_current_posture(device::mojom::DevicePostureType::kFolded);
  EXPECT_EQ(true, EvalJs(shell(), "mediaQueryPostureChanged"));
}

}  //  namespace

}  // namespace content
