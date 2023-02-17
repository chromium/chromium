// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

const char* kNotFoundScriptURL = "https://www.example.com/sw-404.js";

// A fake URLLoader which is used for off-main-thread script fetch tests.
class FakeURLLoader final : public URLLoader {
 public:
  FakeURLLoader() = default;
  ~FakeURLLoader() override = default;

  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      URLLoaderClient*,
      WebURLResponse&,
      absl::optional<WebURLError>&,
      scoped_refptr<SharedBuffer>&,
      int64_t& encoded_data_length,
      uint64_t& encoded_body_length,
      scoped_refptr<BlobDataHandle>& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    NOTREACHED();
  }

  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      URLLoaderClient* client) override {
    if (request->url.spec() == kNotFoundScriptURL) {
      WebURLResponse response;
      response.SetMimeType("text/javascript");
      response.SetHttpStatusCode(404);
      client->DidReceiveResponse(response);
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
      return;
    }
    // Don't handle other requests intentionally to emulate ongoing load.
  }

  void Freeze(LoaderFreezeMode) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

// A fake URLLoaderFactory which is used for off-main-thread script fetch tests.
class FakeURLLoaderFactory final : public URLLoaderFactory {
 public:
  std::unique_ptr<URLLoader> CreateURLLoader(
      const WebURLRequest&,
      scoped_refptr<base::SingleThreadTaskRunner>,
      scoped_refptr<base::SingleThreadTaskRunner>,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle>,
      BackForwardCacheLoaderHelper*) override {
    return std::make_unique<FakeURLLoader>();
  }
};

// A fake WebServiceWorkerFetchContext which is used for off-main-thread script
// fetch tests.
class FakeWebServiceWorkerFetchContext final
    : public WebServiceWorkerFetchContext {
 public:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override {}
  void InitializeOnWorkerThread(AcceptLanguagesWatcher*) override {}
  URLLoaderFactory* GetURLLoaderFactory() override {
    return &fake_url_loader_factory_;
  }
  std::unique_ptr<URLLoaderFactory> WrapURLLoaderFactory(
      CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
          url_loader_factory) override {
    return nullptr;
  }
  void WillSendRequest(WebURLRequest&) override {}
  mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override {
    return mojom::ControllerServiceWorkerMode::kNoController;
  }
  net::SiteForCookies SiteForCookies() const override {
    return net::SiteForCookies();
  }
  absl::optional<WebSecurityOrigin> TopFrameOrigin() const override {
    return absl::optional<WebSecurityOrigin>();
  }
  WebString GetAcceptLanguages() const override { return WebString(); }
  void SetIsOfflineMode(bool is_offline_mode) override {}

 private:
  FakeURLLoaderFactory fake_url_loader_factory_;
};

class FakeBrowserInterfaceBroker final
    : public mojom::blink::BrowserInterfaceBroker {
 public:
  FakeBrowserInterfaceBroker() = default;
  ~FakeBrowserInterfaceBroker() override = default;

  void GetInterface(mojo::GenericPendingReceiver) override {}

  mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::blink::BrowserInterfaceBroker> receiver_{this};
};

class MockServiceWorkerContextClient final
    : public WebServiceWorkerContextClient {
 public:
  MockServiceWorkerContextClient() = default;
  ~MockServiceWorkerContextClient() override = default;

  MOCK_METHOD2(
      WorkerReadyForInspectionOnInitiatorThread,
      void(CrossVariantMojoRemote<mojom::DevToolsAgentInterfaceBase>
               devtools_agent_remote,
           CrossVariantMojoReceiver<mojom::DevToolsAgentHostInterfaceBase>));

  void WorkerContextStarted(WebServiceWorkerContextProxy* proxy,
                            scoped_refptr<base::SequencedTaskRunner>) override {
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerHost> host_remote;
    auto host_receiver = host_remote.InitWithNewEndpointAndPassReceiver();

    mojo::PendingAssociatedRemote<
        mojom::blink::ServiceWorkerRegistrationObjectHost>
        registration_object_host;
    auto registration_object_host_receiver =
        registration_object_host.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerRegistrationObject>
        registration_object;

    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerObjectHost>
        service_worker_object_host;
    auto service_worker_object_host_receiver =
        service_worker_object_host.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerObject>
        service_worker_object;

    // Simulates calling blink.mojom.ServiceWorker.InitializeGlobalScope() to
    // unblock the service worker script evaluation.
    mojo::Remote<mojom::blink::ServiceWorker> service_worker;
    proxy->BindServiceWorker(service_worker.BindNewPipeAndPassReceiver());
    service_worker->InitializeGlobalScope(
        std::move(host_remote),
        mojom::blink::ServiceWorkerRegistrationObjectInfo::New(
            2 /* registration_id */, KURL("https://example.com"),
            mojom::blink::ServiceWorkerUpdateViaCache::kImports,
            std::move(registration_object_host),
            registration_object.InitWithNewEndpointAndPassReceiver(), nullptr,
            nullptr, nullptr),
        mojom::blink::ServiceWorkerObjectInfo::New(
            1 /* service_worker_version_id */,
            mojom::blink::ServiceWorkerState::kParsed,
            KURL("https://example.com"), std::move(service_worker_object_host),
            service_worker_object.InitWithNewEndpointAndPassReceiver()),
        mojom::blink::FetchHandlerExistence::EXISTS,
        /*reporting_observer_receiver=*/mojo::NullReceiver(),
        /*ancestor_frame_type=*/mojom::blink::AncestorFrameType::kNormalFrame);

    // To make the other side callable.
    host_receiver.EnableUnassociatedUsage();
    registration_object_host_receiver.EnableUnassociatedUsage();
    service_worker_object_host_receiver.EnableUnassociatedUsage();
  }

  void FailedToFetchClassicScript() override {
    classic_script_load_failure_event_.Signal();
  }

  void DidEvaluateScript(bool /* success */) override {
    script_evaluated_event_.Signal();
  }

  scoped_refptr<WebServiceWorkerFetchContext>
  CreateWorkerFetchContextOnInitiatorThread() override {
    return base::MakeRefCounted<FakeWebServiceWorkerFetchContext>();
  }

  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse> response,
      mojo::ScopedDataPipeConsumerHandle data_pipe) override {}

  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override {}

  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError> error) override {}

  void WorkerContextDestroyed() override { termination_event_.Signal(); }

  // These methods must be called on the main thread.
  void WaitUntilScriptEvaluated() { script_evaluated_event_.Wait(); }
  void WaitUntilThreadTermination() { termination_event_.Wait(); }
  void WaitUntilFailedToLoadClassicScript() {
    classic_script_load_failure_event_.Wait();
  }

 private:
  base::WaitableEvent script_evaluated_event_;
  base::WaitableEvent termination_event_;
  base::WaitableEvent classic_script_load_failure_event_;
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_client_ = std::make_unique<MockServiceWorkerContextClient>();
    worker_ = std::make_unique<WebEmbeddedWorkerImpl>(mock_client_.get());

    script_url_ = url_test_helpers::ToKURL("https://www.example.com/sw.js");
    WebURLResponse response(script_url_);
    response.SetMimeType("text/javascript");
    response.SetHttpStatusCode(200);
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(script_url_, "",
                                                              response);
  }

  std::unique_ptr<WebEmbeddedWorkerStartData> CreateStartData() {
    WebFetchClientSettingsObject outside_settings_object(
        network::mojom::ReferrerPolicy::kDefault,
        /*outgoing_referrer=*/WebURL(script_url_),
        blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);
    auto start_data = std::make_unique<WebEmbeddedWorkerStartData>(
        std::move(outside_settings_object));
    start_data->script_url = script_url_;
    start_data->user_agent = WebString("dummy user agent");
    start_data->script_type = mojom::blink::ScriptType::kClassic;
    start_data->wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    start_data->policy_container = std::make_unique<WebPolicyContainer>();
    return start_data;
  }

  void TearDown() override {
    // Drain queued tasks posted from the worker thread in order to avoid tasks
    // bound with unretained objects from running after tear down. Worker
    // termination may post such tasks (see https://crbug,com/1007616).
    // TODO(nhiroki): Stop using synchronous WaitableEvent, and instead use
    // QuitClosure to wait until all the tasks run before test completion.
    test::RunPendingTasks();

    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  WebURL script_url_;
  std::unique_ptr<MockServiceWorkerContextClient> mock_client_;
  std::unique_ptr<WebEmbeddedWorkerImpl> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  FakeBrowserInterfaceBroker browser_interface_broker;
  worker_->StartWorkerContext(
      CreateStartData(),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::NullRemote(),
      /*cache_storage_remote=*/mojo::NullRemote(),
      browser_interface_broker.BindNewPipeAndPassRemote(),
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate the worker immediately after start.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  std::unique_ptr<WebEmbeddedWorkerStartData> start_data = CreateStartData();
  start_data->wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  FakeBrowserInterfaceBroker browser_interface_broker;
  worker_->StartWorkerContext(
      std::move(start_data),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::NullRemote(),
      /*cache_storage_remote=*/mojo::NullRemote(),
      browser_interface_broker.BindNewPipeAndPassRemote(),
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate the worker while waiting for the debugger.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  WebURL script_url = url_test_helpers::ToKURL(kNotFoundScriptURL);
  url_test_helpers::RegisterMockedErrorURLLoad(script_url);
  std::unique_ptr<WebEmbeddedWorkerStartData> start_data = CreateStartData();
  start_data->script_url = script_url;
  FakeBrowserInterfaceBroker browser_interface_broker;

  // Start worker and load the script.
  worker_->StartWorkerContext(
      std::move(start_data),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::NullRemote(),
      /*cache_storage_remote=*/mojo::NullRemote(),
      browser_interface_broker.BindNewPipeAndPassRemote(),
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  mock_client_->WaitUntilFailedToLoadClassicScript();

  // Terminate the worker for cleanup.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

}  // namespace blink
