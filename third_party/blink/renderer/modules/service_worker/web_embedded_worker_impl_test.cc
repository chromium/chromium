// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/modules/service_worker/thread_safe_script_container.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/waitable_event.h"

namespace blink {
namespace {

class MockServiceWorkerContextClient : public WebServiceWorkerContextClient {
 public:
  MockServiceWorkerContextClient() = default;
  ~MockServiceWorkerContextClient() override = default;

  MOCK_METHOD0(WorkerReadyForInspection, void());
  MOCK_METHOD0(WorkerContextFailedToStart, void());
  MOCK_METHOD0(WorkerScriptLoaded, void());

  void WorkerContextStarted(WebServiceWorkerContextProxy* proxy) override {
    // In production code, ReadyToEvaluateScript() is called when
    // ServiceWorkerContextClient receives the InitializeGlobalScope() IPC
    // message.
    proxy->ReadyToEvaluateScript();
  }
  void DidEvaluateScript(bool /* success */) override {
    script_evaluated_event_.Signal();
  }

  // Work-around for mocking a method that return unique_ptr.
  MOCK_METHOD0(CreateServiceWorkerNetworkProviderProxy,
               WebServiceWorkerNetworkProvider*());
  std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() override {
    return std::unique_ptr<WebServiceWorkerNetworkProvider>(
        CreateServiceWorkerNetworkProviderProxy());
  }
  void WorkerContextDestroyed() override { termination_event_.Signal(); }

  void WaitUntilScriptEvaluated() { script_evaluated_event_.Wait(); }
  void WaitUntilThreadTermination() { termination_event_.Wait(); }

 private:
  WaitableEvent script_evaluated_event_;
  WaitableEvent termination_event_;
};

class MockServiceWorkerInstalledScriptsManager
    : public ServiceWorkerInstalledScriptsManager {
 public:
  MockServiceWorkerInstalledScriptsManager()
      : ServiceWorkerInstalledScriptsManager(
            Vector<KURL>() /* installed_urls */,
            mojom::blink::ServiceWorkerInstalledScriptsManagerRequest(
                mojo::MessagePipe().handle1),
            mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtrInfo(
                mojo::MessagePipe().handle0,
                mojom::blink::ServiceWorkerInstalledScriptsManagerHost::
                    Version_),
            // Pass a temporary task runner to ensure
            // ServiceWorkerInstalledScriptsManager construction succeeds.
            Platform::Current()
                ->CreateThread(ThreadCreationParams(WebThreadType::kTestThread)
                                   .SetThreadNameForTest("io thread"))
                ->GetTaskRunner()){};
  MOCK_CONST_METHOD1(IsScriptInstalled, bool(const KURL& script_url));
  MOCK_METHOD1(GetRawScriptData,
               std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>(
                   const KURL& script_url));
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    auto client = std::make_unique<MockServiceWorkerContextClient>();
    auto installed_scripts_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>();
    mock_client_ = client.get();
    mock_installed_scripts_manager_ = installed_scripts_manager.get();
    worker_ = WebEmbeddedWorkerImpl::CreateForTesting(
        std::move(client), std::move(installed_scripts_manager));

    WebURL script_url =
        url_test_helpers::ToKURL("https://www.example.com/sw.js");
    WebURLResponse response(script_url);
    response.SetMIMEType("text/javascript");
    response.SetHTTPStatusCode(200);
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(script_url,
                                                                response, "");

    start_data_.script_url = script_url;
    start_data_.user_agent = WebString("dummy user agent");
    start_data_.script_type = mojom::ScriptType::kClassic;
    start_data_.pause_after_download_mode =
        WebEmbeddedWorkerStartData::kDontPauseAfterDownload;
    start_data_.wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    start_data_.v8_cache_options = WebSettings::V8CacheOptions::kDefault;
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  WebEmbeddedWorkerStartData start_data_;
  MockServiceWorkerContextClient* mock_client_;
  MockServiceWorkerInstalledScriptsManager* mock_installed_scripts_manager_;
  std::unique_ptr<WebEmbeddedWorkerImpl> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate before loading the script.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhilePausedAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));

  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Terminate before resuming after download.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  WebURL script_url =
      url_test_helpers::ToKURL("https://www.example.com/sw-404.js");
  WebURLResponse response;
  response.SetMIMEType("text/javascript");
  response.SetHTTPStatusCode(404);
  ResourceError error = ResourceError::Failure(script_url);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      script_url, response, error);
  start_data_.script_url = script_url;

  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));

  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(0);
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
}

// The running worker is detected as a memory leak. crbug.com/586897 and
// crbug.com/807754.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DontPauseAfterDownload DISABLED_DontPauseAfterDownload
#else
#define MAYBE_DontPauseAfterDownload DontPauseAfterDownload
#endif
TEST_F(WebEmbeddedWorkerImplTest, MAYBE_DontPauseAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  mock_client_->WaitUntilScriptEvaluated();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

// The running worker is detected as a memory leak. crbug.com/586897 and
// crbug.com/807754.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PauseAfterDownload DISABLED_PauseAfterDownload
#else
#define MAYBE_PauseAfterDownload PauseAfterDownload
#endif
TEST_F(WebEmbeddedWorkerImplTest, MAYBE_PauseAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Resume after download.
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->ResumeAfterDownload();
  mock_client_->WaitUntilScriptEvaluated();
  testing::Mock::VerifyAndClearExpectations(mock_client_);
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

}  // namespace blink
