// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-blink.h"
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
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

const std::string kServer = "https://a.test";
const std::string kNotFoundScriptURL = kServer + "/sw-404.js";
const std::string kTimedOutURL = kServer + "/timedout.js";
const std::string kEmptyURL = kServer + "/empty.js";

// A fake URLLoader which is used for off-main-thread script fetch tests.
class FakeURLLoader final : public URLLoader {
 public:
  FakeURLLoader() = default;
  ~FakeURLLoader() override = default;

  void LoadSynchronously(std::unique_ptr<network::ResourceRequest> request,
                         scoped_refptr<const SecurityOrigin> top_frame_origin,
                         bool download_to_blob,
                         bool no_mime_sniffing,
                         base::TimeDelta timeout_interval,
                         URLLoaderClient*,
                         WebURLResponse&,
                         std::optional<WebURLError>&,
                         scoped_refptr<SharedBuffer>&,
                         int64_t& encoded_data_length,
                         uint64_t& encoded_body_length,
                         scoped_refptr<BlobDataHandle>& downloaded_blob,
                         std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
                             resource_load_info_notifier_wrapper) override {
    NOTREACHED_IN_MIGRATION();
  }

  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<const SecurityOrigin> top_frame_origin,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      CodeCacheHost* code_cache_host,
      URLLoaderClient* client) override {
    const std::string url = request->url.spec();
    if (url == kNotFoundScriptURL) {
      WebURLResponse response;
      response.SetMimeType("text/javascript");
      response.SetHttpStatusCode(404);
      client->DidReceiveResponse(response,
                                 /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                                 /*cached_metadata=*/std::nullopt);
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
      return;
    }
    if (url == kEmptyURL) {
      WebURLResponse response;
      response.SetMimeType("text/javascript");
      response.SetHttpHeaderField(http_names::kContentType, "text/javascript");
      response.SetCurrentRequestUrl(url_test_helpers::ToKURL(kEmptyURL));
      response.SetHttpStatusCode(200);
      client->DidReceiveResponse(response,
                                 /*body=*/mojo::ScopedDataPipeConsumerHandle(),
                                 /*cached_metadata=*/std::nullopt);
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
      return;
    }
    if (url == kTimedOutURL) {
      // Don't handle other requests intentionally to emulate ongoing load.
      return;
    }
    NOTREACHED();
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
      const network::ResourceRequest&,
      scoped_refptr<base::SingleThreadTaskRunner>,
      scoped_refptr<base::SingleThreadTaskRunner>,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle>,
      BackForwardCacheLoaderHelper*,
      Vector<std::unique_ptr<URLLoaderThrottle>> throttles) override {
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
  void FinalizeRequest(WebURLRequest&) override {}
  WebVector<std::unique_ptr<URLLoaderThrottle>> CreateThrottles(
      const network::ResourceRequest& request) override {
    return {};
  }

  mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override {
    return mojom::ControllerServiceWorkerMode::kNoController;
  }
  net::SiteForCookies SiteForCookies() const override {
    return net::SiteForCookies();
  }
  std::optional<WebSecurityOrigin> TopFrameOrigin() const override {
    return std::optional<WebSecurityOrigin>();
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

class OnFallbackReceiver
    : public mojom::blink::ServiceWorkerFetchResponseCallback {
 public:
  mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
  BindNewPipeAndPassRemote() {
    return response_callback_receiver_.BindNewPipeAndPassRemote();
  }

  std::optional<network::DataElementChunkedDataPipe> WaitFallbackRequestBody() {
    run_loop_.Run();
    CHECK(fallback_request_body_);
    return std::move(*fallback_request_body_);
  }

 private:
  // mojom::blink::ServiceWorkerFetchResponseCallback overrides:
  void OnResponse(
      mojom::blink::FetchAPIResponsePtr response,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing) override {
    NOTREACHED();
  }
  void OnResponseStream(
      mojom::blink::FetchAPIResponsePtr response,
      mojom::blink::ServiceWorkerStreamHandlePtr body_as_stream,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing) override {
    NOTREACHED();
  }
  void OnFallback(
      std::optional<network::DataElementChunkedDataPipe> request_body,
      mojom::blink::ServiceWorkerFetchEventTimingPtr timing) override {
    fallback_request_body_ = std::move(request_body);
    response_callback_receiver_.reset();
    run_loop_.Quit();
  }

  mojo::Receiver<mojom::blink::ServiceWorkerFetchResponseCallback>
      response_callback_receiver_{this};
  base::RunLoop run_loop_;
  std::optional<std::optional<network::DataElementChunkedDataPipe>>
      fallback_request_body_;
};

class MojoHandleWatcher {
 public:
  explicit MojoHandleWatcher(mojo::Handle handle)
      : handle_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                        base::SequencedTaskRunner::GetCurrentDefault()) {
    handle_watcher_.Watch(handle,
                          MOJO_HANDLE_SIGNAL_READABLE |
                              MOJO_HANDLE_SIGNAL_WRITABLE |
                              MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                          base::BindRepeating(&MojoHandleWatcher::OnReady,
                                              base::Unretained(this)));
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    handle_watcher_.ArmOrNotify();
    run_loop_->Run();
  }

  typedef base::OnceCallback<void(void)> DoneCallBack;
  void WaitAsync(DoneCallBack callback) {
    done_callback_ = std::move(callback);
    handle_watcher_.ArmOrNotify();
  }

 private:
  void OnReady(MojoResult result) {
    CHECK_EQ(result, MOJO_RESULT_OK);
    if (done_callback_) {
      std::move(done_callback_).Run();
      return;
    }
    run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  mojo::SimpleWatcher handle_watcher_;
  DoneCallBack done_callback_;
};

class TestDataUploader : public network::mojom::blink::ChunkedDataPipeGetter {
 public:
  explicit TestDataUploader(const std::string& upload_contents)
      : upload_contents_(upload_contents) {}

  mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
  BindNewPipeAndPassRemote() {
    auto pending_remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
        [&](uint32_t reason, const std::string& description) {
          LOG(INFO) << "TestDataUploader Mojo closed reason" << reason
                    << ", desc=" << description;
        }));
    return pending_remote;
  }

  void CallGetSizeCallback() {
    std::move(get_size_callback_).Run(0, upload_contents_.size());
  }

 private:
  // network::mojom::blink::ChunkedDataPipeGetter implementation:
  void GetSize(GetSizeCallback get_size_callback) override {
    get_size_callback_ = std::move(get_size_callback);
  }
  void StartReading(mojo::ScopedDataPipeProducerHandle producer) override {
    producer_ = std::move(producer);

    handle_watcher_ = std::make_unique<MojoHandleWatcher>(producer_.get());
    handle_watcher_->WaitAsync(
        base::BindOnce(&TestDataUploader::OnMojoReady, base::Unretained(this)));
  }

  void OnMojoReady() {
    size_t bytes_written = 0;
    CHECK_EQ(MOJO_RESULT_OK,
             producer_->WriteData(base::as_byte_span(upload_contents_)
                                      .subspan(0u, upload_contents_.size()),
                                  MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
    CHECK_EQ(upload_contents_.size(), bytes_written);
  }

  const std::string upload_contents_;
  mojo::ScopedDataPipeProducerHandle producer_;
  std::unique_ptr<MojoHandleWatcher> handle_watcher_;
  mojo::Receiver<network::mojom::blink::ChunkedDataPipeGetter> receiver_{this};
  GetSizeCallback get_size_callback_;
};

class TestDataPipeReader {
 public:
  explicit TestDataPipeReader(
      mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
          chunked_data_pipe_getter,
      uint32_t capacity_read_pipe_size)
      : chunked_data_pipe_getter_(std::move(chunked_data_pipe_getter)) {
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(capacity_read_pipe_size,
                                                  producer_, consumer_));
    handle_watcher_ = std::make_unique<MojoHandleWatcher>(consumer_.get());

    chunked_data_pipe_getter_.set_disconnect_with_reason_handler(
        base::BindLambdaForTesting(
            [&](uint32_t reason, const std::string& description) {
              LOG(INFO) << "TestDataPipeReader Mojo closed reason" << reason
                        << ", desc=" << description;
            }));

    chunked_data_pipe_getter_->GetSize(
        base::BindLambdaForTesting([](int32_t status, uint64_t size) {}));
    chunked_data_pipe_getter_->StartReading(std::move(producer_));
  }
  TestDataPipeReader(TestDataPipeReader&&) = default;

  std::string Read() {
    handle_watcher_->Wait();
    std::string buffer(20u, '\0');
    size_t actually_read_bytes = 0;
    CHECK_EQ(MOJO_RESULT_OK,
             consumer_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                 base::as_writable_byte_span(buffer),
                                 actually_read_bytes));
    return buffer.substr(0, actually_read_bytes);
  }

  bool IsConnected() const { return chunked_data_pipe_getter_.is_connected(); }

 private:
  mojo::ScopedDataPipeProducerHandle producer_;
  mojo::ScopedDataPipeConsumerHandle consumer_;
  std::unique_ptr<MojoHandleWatcher> handle_watcher_;

  mojo::Remote<network::mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter_;
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

  void SetWebPolicyContainer(WebPolicyContainer* web_policy_container) {
    web_policy_container_ = web_policy_container;
  }

  void WorkerContextStarted(
      WebServiceWorkerContextProxy* proxy,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) override {
    worker_task_runner_ = std::move(worker_task_runner);
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

    mojo::PendingAssociatedRemote<mojom::blink::AssociatedInterfaceProvider>
        associated_interfaces_remote_from_browser;
    auto associated_interfaces_recevier_from_browser =
        associated_interfaces_remote_from_browser
            .InitWithNewEndpointAndPassReceiver();

    mojo::PendingAssociatedRemote<mojom::blink::AssociatedInterfaceProvider>
        associated_interfaces_remote_to_browser;
    auto associated_interfaces_recevier_to_browser =
        associated_interfaces_remote_to_browser
            .InitWithNewEndpointAndPassReceiver();

    // Simulates calling blink.mojom.ServiceWorker.InitializeGlobalScope() to
    // unblock the service worker script evaluation.
    mojo::Remote<mojom::blink::ServiceWorker> service_worker;
    proxy->BindServiceWorker(service_worker.BindNewPipeAndPassReceiver());
    service_worker->InitializeGlobalScope(
        std::move(host_remote),
        std::move(associated_interfaces_remote_from_browser),
        std::move(associated_interfaces_recevier_to_browser),
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
        /*ancestor_frame_type=*/mojom::blink::AncestorFrameType::kNormalFrame,
        blink::BlinkStorageKey());

    MockPolicyContainerHost mock_policy_container_host;
    web_policy_container_->remote =
        mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote();
    web_policy_container_ = nullptr;

    // ControllerServiceWorker requires Clone to ensure
    // CrossOriginResourcePolicyChecker. See
    // ServiceWorkerGlobalScope::DispatchFetchEventForSubresource().
    mojo::Remote<mojom::blink::ControllerServiceWorker>
        stub_controller_service_worker;
    proxy->BindControllerServiceWorker(
        stub_controller_service_worker.BindNewPipeAndPassReceiver());
    stub_controller_service_worker->Clone(
        controller_service_worker_.BindNewPipeAndPassReceiver(),
        network::CrossOriginEmbedderPolicy(), mojo::NullRemote());

    // To make the other side callable.
    host_receiver.EnableUnassociatedUsage();
    associated_interfaces_recevier_from_browser.EnableUnassociatedUsage();
    associated_interfaces_remote_to_browser.EnableUnassociatedUsage();
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

  TestDataPipeReader DispatchFetchEventForSubresourceAndCreateReader(
      const std::string& upload_contents,
      uint32_t capacity_read_pipe_size) {
    OnFallbackReceiver on_fallback_receiver;
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MockServiceWorkerContextClient::
                           DispatchFetchEventForSubresourceonWorkerThread,
                       base::Unretained(this),
                       base::Unretained(&on_fallback_receiver),
                       upload_contents));
    auto fallback_request_body = on_fallback_receiver.WaitFallbackRequestBody();
    return TestDataPipeReader(
        fallback_request_body->ReleaseChunkedDataPipeGetter(),
        capacity_read_pipe_size);
  }

  void DispatchFetchEventForSubresourceonWorkerThread(
      OnFallbackReceiver* on_fallback_receiver,
      const std::string& upload_contents) {
    auto request = mojom::blink::FetchAPIRequest::New();
    request->url = url_test_helpers::ToKURL(kServer);
    request->method = "POST";
    request->is_main_resource_load = false;

    test_data_uploader_ = std::make_unique<TestDataUploader>(upload_contents);
    ResourceRequestBody src(test_data_uploader_->BindNewPipeAndPassRemote());
    request->body = std::move(src);
    auto params = mojom::blink::DispatchFetchEventParams::New();
    params->request = std::move(request);
    params->client_id = "foo";
    params->resulting_client_id = "bar";

    controller_service_worker_->DispatchFetchEventForSubresource(
        std::move(params), on_fallback_receiver->BindNewPipeAndPassRemote(),
        base::DoNothing());
  }

  void CollectAllGarbageOnWorkerThread() {
    base::RunLoop run_loop;
    worker_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          blink::WebHeap::CollectAllGarbageForTesting();
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void CallUploaderGetSizeCallback() {
    base::RunLoop run_loop;
    worker_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    test_data_uploader_->CallGetSizeCallback();
                                    run_loop.Quit();
                                  }));
    run_loop.Run();
  }

  void WorkerContextDestroyed() override {
    test_data_uploader_.reset();
    controller_service_worker_.reset();
    termination_event_.Signal();
  }

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

  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  mojo::Remote<mojom::blink::ControllerServiceWorker>
      controller_service_worker_;
  std::unique_ptr<TestDataUploader> test_data_uploader_;
  raw_ptr<WebPolicyContainer> web_policy_container_;
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_client_ = std::make_unique<MockServiceWorkerContextClient>();
    worker_ = std::make_unique<WebEmbeddedWorkerImpl>(mock_client_.get());
  }

  std::unique_ptr<WebEmbeddedWorkerStartData> CreateStartData() {
    const WebURL script_url = url_test_helpers::ToKURL(kTimedOutURL);
    WebFetchClientSettingsObject outside_settings_object(
        network::mojom::ReferrerPolicy::kDefault,
        /*outgoing_referrer=*/script_url,
        blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);
    auto start_data = std::make_unique<WebEmbeddedWorkerStartData>(
        std::move(outside_settings_object));
    start_data->script_url = script_url;
    start_data->user_agent = WebString("dummy user agent");
    start_data->script_type = mojom::blink::ScriptType::kClassic;
    start_data->wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    start_data->policy_container = std::make_unique<WebPolicyContainer>();
    mock_client_->SetWebPolicyContainer(start_data->policy_container.get());
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

  test::TaskEnvironment task_environment_;
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

TEST_F(WebEmbeddedWorkerImplTest, GCOnWorkerThreadShouldNotCauseUploadFail) {
  std::unique_ptr<WebEmbeddedWorkerStartData> start_data = CreateStartData();
  start_data->script_url = url_test_helpers::ToKURL(kEmptyURL);
  FakeBrowserInterfaceBroker browser_interface_broker;
  worker_->StartWorkerContext(
      std::move(start_data),
      // CreateStartData(),
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::NullRemote(),
      /*cache_storage_remote=*/mojo::NullRemote(),
      browser_interface_broker.BindNewPipeAndPassRemote(),
      InterfaceRegistry::GetEmptyInterfaceRegistry(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  mock_client_->WaitUntilScriptEvaluated();

  // We need to fulfill mojo pipe to let BytesUploader await it and
  // not to have Oilpan references. See the loop in
  // BytesUploader::WriteDataOnPipe().
  TestDataPipeReader reader =
      mock_client_->DispatchFetchEventForSubresourceAndCreateReader(
          /*upload_contents=*/"foobarbaz",
          /*capacity_read_pipe_size=*/3u);
  // Confirm mojo piping is connected.
  EXPECT_EQ("foo", reader.Read());

  mock_client_->CollectAllGarbageOnWorkerThread();
  EXPECT_TRUE(reader.IsConnected());

  EXPECT_EQ("bar", reader.Read());
  EXPECT_EQ("baz", reader.Read());
  mock_client_->CallUploaderGetSizeCallback();
  mock_client_->CollectAllGarbageOnWorkerThread();
  EXPECT_FALSE(reader.IsConnected());

  // Terminate the worker for cleanup.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

}  // namespace blink
