// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace content {

namespace {

using device::FakeSensorProvider;

class DeviceSensorBrowserTest : public ContentBrowserTest {
 public:
  DeviceSensorBrowserTest() {
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(&DeviceSensorBrowserTest::BindSensorProvider,
                            base::Unretained(this)));
  }

  ~DeviceSensorBrowserTest() override {
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
  }

  void WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta delay) {
    ShellJavaScriptDialogManager* dialog_manager =
        static_cast<ShellJavaScriptDialogManager*>(
            shell()->GetJavaScriptDialogManager(shell()->web_contents()));

    base::RunLoop run_loop;
    dialog_manager->set_dialog_request_callback(base::BindOnce(
        [](base::TimeDelta delay, base::OnceClosure quit_closure) {
          base::PlatformThread::Sleep(delay);
          std::move(quit_closure).Run();
        },
        delay, run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }

  std::unique_ptr<FakeSensorProvider> sensor_provider_;
  std::unique_ptr<net::EmbeddedTestServer> https_embedded_test_server_;

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
        "content/test/data/device_sensors");
    https_embedded_test_server_->StartAcceptingConnections();

    sensor_provider_ = std::make_unique<FakeSensorProvider>();
    sensor_provider_->SetAccelerometerData(4, 5, 6);
    sensor_provider_->SetLinearAccelerationSensorData(1, 2, 3);
    sensor_provider_->SetGyroscopeData(7, 8, 9);
    sensor_provider_->SetRelativeOrientationSensorData(1, 2, 3);
    sensor_provider_->SetAbsoluteOrientationSensorData(4, 5, 6);
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

  void BindSensorProvider(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    sensor_provider_->Bind(std::move(receiver));
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       OrientationFallbackToAbsoluteTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  //
  // Here the relative orientation sensor is not available, but the absolute
  // orientation sensor is available, so orientation event will provide the
  // absolute orientation data.
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(true);
  GURL test_url = GetTestUrl(
      "device_sensors", "device_orientation_fallback_to_absolute_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteTest) {
  // The test page will register an event handler for absolute orientation
  // events, expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_absolute_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationNullTest) {
  // The test page registers an event handler for orientation events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  //
  // Here it needs to set both the relative and absolute orientation sensors
  // unavailable, since orientation event will fallback to absolute orientation
  // sensor if it is available.
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationAbsoluteNullTest) {
  // The test page registers an event handler for absolute orientation events
  // and expects to get an event with null values, because no sensor data can be
  // provided.
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_orientation_absolute_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionNullTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  sensor_provider_->set_accelerometer_is_available(false);
  sensor_provider_->set_linear_acceleration_sensor_is_available(false);
  sensor_provider_->set_gyroscope_is_available(false);
  GURL test_url = GetTestUrl("device_sensors", "device_motion_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       MotionOnlySomeSensorsAreAvailableTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with only the gyroscope and linear acceleration
  // sensor values, because no accelerometer values can be provided.
  sensor_provider_->set_accelerometer_is_available(false);
  GURL test_url =
      GetTestUrl("device_sensors",
                 "device_motion_only_some_sensors_are_available_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, NullTestWithAlert) {
  // The test page registers an event handlers for motion/orientation events and
  // expects to get events with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-events still propagate to
  // window after the alert is dismissed and the callbacks are invoked which
  // eventually navigate to #pass.
  sensor_provider_->set_relative_orientation_sensor_is_available(false);
  sensor_provider_->set_absolute_orientation_sensor_is_available(false);
  sensor_provider_->set_accelerometer_is_available(false);
  sensor_provider_->set_linear_acceleration_sensor_is_available(false);
  sensor_provider_->set_gyroscope_is_available(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url =
      GetTestUrl("device_sensors", "device_sensors_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::Milliseconds(500));

  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       DeviceMotionCrossOriginIframeForbiddenTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/device_motion_test.html?failure_timeout=100");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("fail", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       DeviceMotionCrossOriginIframeAllowedTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url =
      https_embedded_test_server_->GetURL("b.com", "/device_motion_test.html");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  // Now allow 'accelerometer' and 'gyroscope' policy features.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.getElementById('cross_origin_iframe')."
                     "allow='accelerometer; gyroscope'"));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("pass", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       DeviceOrientationCrossOriginIframeForbiddenTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/device_orientation_test.html?failure_timeout=100");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("fail", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       DeviceOrientationCrossOriginIframeAllowedTest) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/device_orientation_test.html");

  // Wait for the main frame, subframe, and the #pass/#fail commits.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 3);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  // Now allow 'accelerometer' and 'gyroscope' policy features.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.getElementById('cross_origin_iframe')."
                     "allow='accelerometer; gyroscope'"));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  navigation_observer.Wait();

  content::RenderFrameHost* iframe =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ("pass", iframe->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest,
                       DeviceOrientationPermissionsPolicyWarning) {
  // Main frame is on a.com, iframe is on b.com.
  GURL main_frame_url =
      https_embedded_test_server_->GetURL("a.com", "/cross_origin_iframe.html");
  GURL iframe_url = https_embedded_test_server_->GetURL(
      "b.com", "/device_orientation_absolute_test.html");

  const char kWarningMessage[] =
      "The deviceorientationabsolute events are blocked by "
      "permissions policy. See "
      "https://github.com/w3c/webappsec-permissions-policy/blob/master/"
      "features.md#sensor-features";

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(kWarningMessage);

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(),
                                  "cross_origin_iframe", iframe_url));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(kWarningMessage, console_observer.GetMessageAt(0u));
}

}  //  namespace

}  //  namespace content
