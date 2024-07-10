// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/base/synchronization/one_writer_seqlock.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace content {

namespace {

using device::FakeSensorProvider;

class GenericSensorBrowserTest : public ContentBrowserTest {
 public:
  GenericSensorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGenericSensorExtraClasses}, {});

    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(
            &GenericSensorBrowserTest::BindSensorProviderReceiver,
            base::Unretained(this)));
  }

  GenericSensorBrowserTest(const GenericSensorBrowserTest&) = delete;
  GenericSensorBrowserTest& operator=(const GenericSensorBrowserTest&) = delete;

  ~GenericSensorBrowserTest() override {
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_embedded_test_server_;

  void set_sensor_provider_available(bool sensor_provider_available) {
    sensor_provider_available_ = sensor_provider_available;
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_embedded_test_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_embedded_test_server_.get());
    https_embedded_test_server_->ServeFilesFromSourceDirectory(
        "content/test/data/generic_sensor");
    https_embedded_test_server_->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

  void BindSensorProviderReceiver(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    if (!sensor_provider_available_)
      return;

    if (!fake_sensor_provider_) {
      fake_sensor_provider_ = std::make_unique<FakeSensorProvider>();
      fake_sensor_provider_->SetAmbientLightSensorData(50);
    }

    fake_sensor_provider_->Bind(std::move(receiver));
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool sensor_provider_available_ = true;
  std::unique_ptr<FakeSensorProvider> fake_sensor_provider_;
};

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, AmbientLightSensorTest) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get events with fake values then navigates to #pass.
  GURL test_url =
      GetTestUrl("generic_sensor", "ambient_light_sensor_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest,
                       AmbientLightSensorCrossOriginIframeTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/ambient_light_sensor_cross_origin_iframe_test.html");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("pass", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, SensorProviderUnavailable) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get a sensor error then navigates to #pass.
  set_sensor_provider_available(false);
  GURL test_url = GetTestUrl("generic_sensor",
                             "ambient_light_sensor_unavailable_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
