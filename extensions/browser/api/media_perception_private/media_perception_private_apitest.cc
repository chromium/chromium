// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/media_analytics/fake_media_analytics_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/media_perception/media_perception.pb.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "extensions/browser/api/media_perception_private/media_perception_private_api.h"
#include "extensions/common/api/media_perception_private.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace extensions {

namespace media_perception = extensions::api::media_perception_private;

namespace {

class TestMediaPerceptionAPIDelegate : public MediaPerceptionAPIDelegate {
 public:
  void LoadCrOSComponent(const media_perception::ComponentType& type,
                         LoadCrOSComponentCallback load_callback) override {
    // For testing both success and failure cases, test class has the LIGHT
    // component succeed install and the others fail.
    if (type == media_perception::COMPONENT_TYPE_LIGHT) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(load_callback),
              media_perception::COMPONENT_INSTALLATION_ERROR_NONE,
              base::FilePath("/run/imageloader/rtanalytics-light/1.0")));
      return;
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(load_callback),
                       media_perception::COMPONENT_INSTALLATION_ERROR_NOT_FOUND,
                       base::FilePath()));
  }

  void BindVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override {
    NOTIMPLEMENTED();
  }

  void SetMediaPerceptionRequestHandler(
      MediaPerceptionRequestHandler handler) override {
    NOTIMPLEMENTED();
  }

  void ForwardMediaPerceptionReceiver(
      mojo::PendingReceiver<chromeos::media_perception::mojom::MediaPerception>
          receiver,
      content::RenderFrameHost* render_frame_host) override {
    NOTIMPLEMENTED();
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  MediaPerceptionAPIDelegate* GetMediaPerceptionAPIDelegate() override {
    if (!test_media_perception_api_delegate_) {
      test_media_perception_api_delegate_ =
          std::make_unique<TestMediaPerceptionAPIDelegate>();
    }
    return test_media_perception_api_delegate_.get();
  }

 private:
  std::unique_ptr<TestMediaPerceptionAPIDelegate>
      test_media_perception_api_delegate_;
};

}  // namespace

class MediaPerceptionPrivateApiTest : public ShellApiTest {
 public:
  MediaPerceptionPrivateApiTest() {}
  ~MediaPerceptionPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShellApiTest::SetUpCommandLine(command_line);
    // Whitelist of the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUpInProcessBrowserTestFixture() override {
    // MediaAnalyticsClient and UpstartClient are required by
    // MediaPerceptionAPIManager.
    chromeos::MediaAnalyticsClient::InitializeFake();
    chromeos::UpstartClient::InitializeFake();
  }

  void TearDownInProcessBrowserTestFixture() override {
    chromeos::UpstartClient::Shutdown();
    chromeos::MediaAnalyticsClient::Shutdown();
  }

  void SetUpOnMainThread() override {
    session_feature_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::FeatureSessionType::KIOSK);
    ShellApiTest::SetUpOnMainThread();
  }

 private:
  std::unique_ptr<base::AutoReset<extensions::FeatureSessionType>>
      session_feature_type_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateApiTest);
};

// Verify that we can execute the setAnalyticsComponent API and deal with
// failures.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, SetAnalyticsComponent) {
  // Constructing a TestExtensionsAPIClient to set the behavior of the
  // ExtensionsAPIClient.
  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(RunAppTest("media_perception_private/component")) << message_;
}

// Verify that we can use the new interface to set the process state of the
// media perception component.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest,
                       SetComponentProcessState) {
  // Constructing a TestExtensionsAPIClient to set the behavior of the
  // ExtensionsAPIClient.
  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(RunAppTest("media_perception_private/process_state")) << message_;
}

// Verify that we can set and get mediaPerception system state.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, State) {
  ASSERT_TRUE(RunAppTest("media_perception_private/state")) << message_;
}

// Verify that we can request Diagnostics.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, GetDiagnostics) {
  // Allows us to validate that the right data comes through the code path.
  mri::Diagnostics diagnostics;
  diagnostics.add_perception_sample()->mutable_frame_perception()->set_frame_id(
      1);
  chromeos::FakeMediaAnalyticsClient::Get()->SetDiagnostics(diagnostics);

  ASSERT_TRUE(RunAppTest("media_perception_private/diagnostics")) << message_;
}

// Verify that we can listen for MediaPerceptionDetection signals and handle
// them.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, MediaPerception) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener handler_registered_listener(
      "mediaPerceptionListenerSet", false);
  ASSERT_TRUE(LoadApp("media_perception_private/media_perception")) << message_;
  ASSERT_TRUE(handler_registered_listener.WaitUntilSatisfied());

  mri::MediaPerception media_perception;
  media_perception.add_frame_perception()->set_frame_id(1);
  ASSERT_TRUE(
      chromeos::FakeMediaAnalyticsClient::Get()->FireMediaPerceptionEvent(
          media_perception));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
