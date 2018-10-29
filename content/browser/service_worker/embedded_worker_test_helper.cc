// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/renderer.mojom.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {

namespace {

void OnFetchEventCommon(
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response_callback->OnResponse(
      std::move(response), blink::mojom::ServiceWorkerFetchEventTiming::New());
  std::move(finish_callback)
      .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
           base::TimeTicks::Now());
}

}  // namespace

// A URLLoaderFactory that returns 200 OK with a simple body to any request.
class EmbeddedWorkerTestHelper::MockNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  MockNetworkURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    std::string headers = "HTTP/1.1 200 OK\n\n";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    network::ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response);

    std::string body = "this body came from the network";
    uint32_t bytes_written = body.size();
    mojo::DataPipe data_pipe;
    data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  DISALLOW_COPY_AND_ASSIGN(MockNetworkURLLoaderFactory);
};

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper)
    : helper_(helper), binding_(this) {}

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    ~MockEmbeddedWorkerInstanceClient() {}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StartWorker(
    mojom::EmbeddedWorkerStartParamsPtr params) {
  if (!helper_)
    return;

  embedded_worker_id_ = params->embedded_worker_id;

  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(params->embedded_worker_id);
  ASSERT_TRUE(worker);

  helper_->OnStartWorkerStub(std::move(params));
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StopWorker() {
  if (!helper_)
    return;

  ASSERT_TRUE(embedded_worker_id_);
  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(embedded_worker_id_.value());
  // |worker| is possible to be null when corresponding EmbeddedWorkerInstance
  // is removed right after sending StopWorker.
  if (worker)
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  helper_->OnStopWorkerStub(embedded_worker_id_.value());
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    ResumeAfterDownload() {
  helper_->OnResumeAfterDownloadStub(embedded_worker_id_.value());
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    AddMessageToConsole(blink::WebConsoleMessage::Level level,
                        const std::string& message) {
  // TODO(shimazu): Pass these arguments to the test helper when a test is
  // necessary to check them individually.
}

// static
void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::Bind(
    const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
    mojom::EmbeddedWorkerInstanceClientRequest request) {
  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>* clients =
      helper->mock_instance_clients();
  size_t next_client_index = helper->mock_instance_clients_next_index_;

  ASSERT_GE(clients->size(), next_client_index);
  if (clients->size() == next_client_index) {
    clients->push_back(
        std::make_unique<MockEmbeddedWorkerInstanceClient>(helper));
  }

  std::unique_ptr<MockEmbeddedWorkerInstanceClient>& client =
      clients->at(next_client_index);
  helper->mock_instance_clients_next_index_ = next_client_index + 1;
  if (client)
    client->binding_.Bind(std::move(request));
}

class EmbeddedWorkerTestHelper::MockServiceWorker
    : public mojom::ServiceWorker {
 public:
  static void Create(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     int embedded_worker_id,
                     mojom::ServiceWorkerRequest request) {
    mojo::MakeStrongBinding(
        std::make_unique<MockServiceWorker>(helper, embedded_worker_id),
        std::move(request));
  }

  MockServiceWorker(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                    int embedded_worker_id)
      : helper_(helper), embedded_worker_id_(embedded_worker_id) {}

  ~MockServiceWorker() override {}

  void InitializeGlobalScope(
      blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info)
      override {
    if (!helper_)
      return;
    helper_->OnInitializeGlobalScope(embedded_worker_id_,
                                     std::move(service_worker_host),
                                     std::move(registration_info));
  }

  void DispatchInstallEvent(
      DispatchInstallEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnInstallEventStub(std::move(callback));
  }

  void DispatchActivateEvent(DispatchActivateEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnActivateEventStub(std::move(callback));
  }

  void DispatchBackgroundFetchAbortEvent(
      const BackgroundFetchRegistration& registration,
      DispatchBackgroundFetchAbortEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchAbortEventStub(registration, std::move(callback));
  }

  void DispatchBackgroundFetchClickEvent(
      const BackgroundFetchRegistration& registration,
      DispatchBackgroundFetchClickEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchClickEventStub(registration, std::move(callback));
  }

  void DispatchBackgroundFetchFailEvent(
      const BackgroundFetchRegistration& registration,
      DispatchBackgroundFetchFailEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchFailEventStub(registration, std::move(callback));
  }

  void DispatchBackgroundFetchSuccessEvent(
      const BackgroundFetchRegistration& registration,
      DispatchBackgroundFetchSuccessEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchSuccessEventStub(registration,
                                               std::move(callback));
  }

  void DispatchCookieChangeEvent(
      const net::CanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause cause,
      DispatchCookieChangeEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnCookieChangeEventStub(cookie, cause, std::move(callback));
  }

  void DispatchFetchEvent(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnFetchEventStub(
        embedded_worker_id_, params->request, std::move(params->preload_handle),
        std::move(response_callback), std::move(callback));
  }

  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationClickEventStub(notification_id, notification_data,
                                          action_index, reply,
                                          std::move(callback));
  }

  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationCloseEventStub(notification_id, notification_data,
                                          std::move(callback));
  }

  void DispatchPushEvent(const base::Optional<std::string>& payload,
                         DispatchPushEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnPushEventStub(payload, std::move(callback));
  }

  void DispatchSyncEvent(const std::string& tag,
                         bool last_chance,
                         base::TimeDelta timeout,
                         DispatchSyncEventCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DispatchAbortPaymentEvent(
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchAbortPaymentEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnAbortPaymentEventStub(std::move(response_callback),
                                     std::move(callback));
  }

  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchCanMakePaymentEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnCanMakePaymentEventStub(std::move(event_data),
                                       std::move(response_callback),
                                       std::move(callback));
  }

  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchPaymentRequestEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnPaymentRequestEventStub(std::move(event_data),
                                       std::move(response_callback),
                                       std::move(callback));
  }

  void DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnExtendableMessageEventStub(std::move(event),
                                          std::move(callback));
  }

  void DispatchExtendableMessageEventWithCustomTimeout(
      mojom::ExtendableMessageEventPtr event,
      base::TimeDelta timeout,
      DispatchExtendableMessageEventWithCustomTimeoutCallback callback)
      override {
    if (!helper_)
      return;
    helper_->OnExtendableMessageEventStub(std::move(event),
                                          std::move(callback));
  }

  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  void SetIdleTimerDelayToZero() override {
    if (!helper_)
      return;
    helper_->OnSetIdleTimerDelayToZero(embedded_worker_id_);
  }

 private:
  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  const int embedded_worker_id_;
};

class EmbeddedWorkerTestHelper::MockRendererInterface : public mojom::Renderer {
 public:
  explicit MockRendererInterface(base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : helper_(helper) {}

  void AddBinding(mojom::RendererAssociatedRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void CreateEmbedderRendererService(
      service_manager::mojom::ServiceRequest service_request) override {
    NOTREACHED();
  }
  void CreateView(mojom::CreateViewParamsPtr) override { NOTREACHED(); }
  void CreateFrame(mojom::CreateFrameParamsPtr) override { NOTREACHED(); }
  void SetUpEmbeddedWorkerChannelForServiceWorker(
      mojom::EmbeddedWorkerInstanceClientRequest client_request) override {
    MockEmbeddedWorkerInstanceClient::Bind(helper_, std::move(client_request));
  }
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      int32_t opener_routing_id,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& devtools_frame_token) override {
    NOTREACHED();
  }
  void OnNetworkConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type,
      double max_bandwidth_mbps) override {
    NOTREACHED();
  }
  void OnNetworkQualityChanged(net::EffectiveConnectionType type,
                               base::TimeDelta http_rtt,
                               base::TimeDelta transport_rtt,
                               double bandwidth_kbps) override {
    NOTREACHED();
  }
  void SetWebKitSharedTimersSuspended(bool suspend) override { NOTREACHED(); }
  void SetUserAgent(const std::string& user_agent) override { NOTREACHED(); }
  void UpdateScrollbarTheme(
      mojom::UpdateScrollbarThemeParamsPtr params) override {
    NOTREACHED();
  }
  void OnSystemColorsChanged(int32_t aqua_color_variant,
                             const std::string& highlight_text_color,
                             const std::string& highlight_color) override {
    NOTREACHED();
  }
  void PurgePluginListCache(bool reload_pages) override { NOTREACHED(); }
  void SetProcessBackgrounded(bool backgrounded) override { NOTREACHED(); }
  void SetSchedulerKeepActive(bool keep_active) override { NOTREACHED(); }
  void ProcessPurgeAndSuspend() override { NOTREACHED(); }
  void SetIsLockedToSite() override { NOTREACHED(); }
  void EnableV8LowMemoryMode() override { NOTREACHED(); }

  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  mojo::AssociatedBindingSet<mojom::Renderer> bindings_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(std::make_unique<TestBrowserContext>()),
      render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      mock_instance_clients_next_index_(0),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_getter_(
          base::MakeRefCounted<URLLoaderFactoryGetter>()),
      weak_factory_(this) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  wrapper_->InitInternal(user_data_directory, std::move(database_task_runner),
                         nullptr, nullptr, nullptr,
                         url_loader_factory_getter_.get());
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());

  // Install a mocked mojom::Renderer interface to catch requests to
  // establish Mojo connection for EWInstanceClient.
  mock_renderer_interface_ =
      std::make_unique<MockRendererInterface>(AsWeakPtr());

  auto renderer_interface_ptr =
      std::make_unique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeRequestAssociatedWithDedicatedPipe(
          renderer_interface_ptr.get()));
  render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(renderer_interface_ptr));

  auto new_renderer_interface_ptr =
      std::make_unique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeRequestAssociatedWithDedicatedPipe(
          new_renderer_interface_ptr.get()));
  new_render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(new_renderer_interface_ptr));

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    default_network_loader_factory_ =
        std::make_unique<MockNetworkURLLoaderFactory>();
    SetNetworkFactory(default_network_loader_factory_.get());
  }
}

void EmbeddedWorkerTestHelper::SetNetworkFactory(
    network::mojom::URLLoaderFactory* factory) {
  if (!factory)
    factory = default_network_loader_factory_.get();

  // Reset factory in URLLoaderFactoryGetter so that we don't hit DCHECK()
  // there.
  url_loader_factory_getter_->SetNetworkFactoryForTesting(nullptr);
  url_loader_factory_getter_->SetNetworkFactoryForTesting(factory);

  render_process_host_->OverrideURLLoaderFactory(factory);
  new_render_process_host_->OverrideURLLoaderFactory(factory);
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_.get())
    wrapper_->Shutdown();
}

void EmbeddedWorkerTestHelper::RegisterMockInstanceClient(
    std::unique_ptr<MockEmbeddedWorkerInstanceClient> client) {
  mock_instance_clients_.push_back(std::move(client));
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = nullptr;
}

// static
net::HttpResponseInfo EmbeddedWorkerTestHelper::CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, arraysize(data)));
  return info;
}

void EmbeddedWorkerTestHelper::OnStartWorker(
    int embedded_worker_id,
    int64_t service_worker_version_id,
    const GURL& scope,
    const GURL& script_url,
    bool pause_after_download,
    mojom::ServiceWorkerRequest service_worker_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  MockServiceWorker::Create(AsWeakPtr(), embedded_worker_id,
                            std::move(service_worker_request));
  embedded_worker_id_service_worker_version_id_map_[embedded_worker_id] =
      service_worker_version_id;
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id].Bind(
      std::move(instance_host));
  embedded_worker_id_installed_scripts_info_map_[embedded_worker_id] =
      std::move(installed_scripts_info);
  ServiceWorkerRemoteProviderEndpoint* provider_endpoint =
      &embedded_worker_id_remote_provider_map_[embedded_worker_id];
  provider_endpoint->BindWithProviderInfo(std::move(provider_info));

  SimulateWorkerReadyForInspection(embedded_worker_id);
  SimulateWorkerScriptCached(
      embedded_worker_id,
      base::BindOnce(&EmbeddedWorkerTestHelper::DidSimulateWorkerScriptCached,
                     AsWeakPtr(), embedded_worker_id, pause_after_download));
}

void EmbeddedWorkerTestHelper::DidSimulateWorkerScriptCached(
    int embedded_worker_id,
    bool pause_after_download) {
  SimulateWorkerScriptLoaded(embedded_worker_id);
  if (!pause_after_download)
    OnResumeAfterDownload(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownload(int embedded_worker_id) {
  SimulateScriptEvaluationStart(embedded_worker_id);
  SimulateWorkerStarted(
      embedded_worker_id,
      blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
      GetNextThreadId());
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnActivateEvent(
    mojom::ServiceWorker::DispatchActivateEventCallback callback) {
  dispatched_events()->push_back(Event::Activate);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    mojom::ServiceWorker::DispatchCookieChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    mojom::ServiceWorker::DispatchExtendableMessageEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnInstallEvent(
    mojom::ServiceWorker::DispatchInstallEventCallback callback) {
  dispatched_events()->push_back(Event::Install);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          true /* has_fetch_handler */, base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int /* embedded_worker_id */,
    const network::ResourceRequest& /* request */,
    blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  // TODO(falken): In-line common into here.
  OnFetchEventCommon(std::move(response_callback), std::move(finish_callback));
}

void EmbeddedWorkerTestHelper::OnPushEvent(
    base::Optional<std::string> payload,
    mojom::ServiceWorker::DispatchPushEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    mojom::ServiceWorker::DispatchNotificationClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    mojom::ServiceWorker::DispatchNotificationCloseEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback) {
  response_callback->OnResponseForAbortPayment(true, base::TimeTicks::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchCanMakePaymentEventCallback callback) {
  bool can_make_payment = false;
  for (const auto& method_data : event_data->method_data) {
    if (method_data->supported_method == "test-method") {
      can_make_payment = true;
      break;
    }
  }
  response_callback->OnResponseForCanMakePayment(can_make_payment,
                                                 base::TimeTicks::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchPaymentRequestEventCallback callback) {
  response_callback->OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponse::New(), base::TimeTicks::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::TimeTicks::Now());
}

void EmbeddedWorkerTestHelper::OnSetIdleTimerDelayToZero(
    int embedded_worker_id) {
  // Subclasses may implement this method.
}

void EmbeddedWorkerTestHelper::SimulateWorkerReadyForInspection(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnReadyForInspection();
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptCached(
    int embedded_worker_id,
    base::OnceClosure callback) {
  int64_t version_id =
      embedded_worker_id_service_worker_version_id_map_[embedded_worker_id];
  ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
  if (!version) {
    std::move(callback).Run();
    return;
  }
  if (!version->script_cache_map()->size()) {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    // Add a dummy ResourceRecord for the main script to the script cache map of
    // the ServiceWorkerVersion.
    records.push_back(WriteToDiskCacheAsync(
        context()->storage(), version->script_url(),
        context()->storage()->NewResourceId(), {} /* headers */, "I'm a body",
        "I'm a meta data", std::move(callback)));
    version->script_cache_map()->SetResources(records);
  }
  if (!version->GetMainScriptHttpResponseInfo())
    version->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
  // Call |callback| if |version| already has ResourceRecords.
  if (!callback.is_null())
    std::move(callback).Run();
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptLoaded(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnScriptLoaded();
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateScriptEvaluationStart(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnScriptEvaluationStart();
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(
    int embedded_worker_id,
    blink::mojom::ServiceWorkerStartStatus status,
    int thread_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]->OnStarted(
      status, thread_id, mojom::EmbeddedWorkerStartTiming::New());
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker) {
    ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
    embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]->OnStopped();
    embedded_worker_id_remote_provider_map_.erase(embedded_worker_id);
    base::RunLoop().RunUntilIdle();
  }
}

void EmbeddedWorkerTestHelper::SimulateRequestTermination(
    int embedded_worker_id,
    base::OnceCallback<void(bool)> callback) {
  base::RunLoop loop;
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->RequestTermination(std::move(callback));
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::OnInitializeGlobalScope(
    int embedded_worker_id,
    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info) {
  embedded_worker_id_host_map_[embedded_worker_id].Bind(
      std::move(service_worker_host));
  // To enable the caller end points to make calls safely with no need to pass
  // these associated interface requests through a message pipe endpoint.
  mojo::AssociateWithDisconnectedPipe(registration_info->request.PassHandle());
  if (registration_info->installing) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->installing->request.PassHandle());
  }
  if (registration_info->waiting) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->waiting->request.PassHandle());
  }
  if (registration_info->active) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->active->request.PassHandle());
  }
  // Keep all Mojo connections alive.
  embedded_worker_id_registration_info_map_[embedded_worker_id] =
      std::move(registration_info);
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    mojom::EmbeddedWorkerStartParamsPtr params) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params->embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EmbeddedWorkerTestHelper::OnStartWorker, AsWeakPtr(),
          params->embedded_worker_id, params->service_worker_version_id,
          params->scope, params->script_url, params->pause_after_download,
          std::move(params->service_worker_request),
          std::move(params->controller_request),
          std::move(params->instance_host), std::move(params->provider_info),
          std::move(params->installed_scripts_info)));
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownloadStub(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnResumeAfterDownload,
                     AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnStopWorker,
                                AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(
    mojom::ServiceWorker::DispatchActivateEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnActivateEvent,
                                AsWeakPtr(), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEventStub(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent,
                     AsWeakPtr(), registration, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEventStub(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent,
                     AsWeakPtr(), registration, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEventStub(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent,
                     AsWeakPtr(), registration, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEventStub(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEvent,
                     AsWeakPtr(), registration, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnCookieChangeEventStub(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    mojom::ServiceWorker::DispatchCookieChangeEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnCookieChangeEvent,
                     AsWeakPtr(), cookie, cause, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    mojom::ExtendableMessageEventPtr event,
    mojom::ServiceWorker::DispatchExtendableMessageEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                     AsWeakPtr(), std::move(event), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(
    mojom::ServiceWorker::DispatchInstallEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnInstallEvent,
                                AsWeakPtr(), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int embedded_worker_id,
    const network::ResourceRequest& request,
    blink::mojom::FetchEventPreloadHandlePtr preload_handle,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnFetchEvent, AsWeakPtr(),
                     embedded_worker_id, request, std::move(preload_handle),
                     std::move(response_callback), std::move(finish_callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationClickEventStub(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    mojom::ServiceWorker::DispatchNotificationClickEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationClickEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     action_index, reply, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEventStub(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    mojom::ServiceWorker::DispatchNotificationCloseEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationCloseEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    base::Optional<std::string> payload,
    mojom::ServiceWorker::DispatchPushEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnPushEvent, AsWeakPtr(),
                     std::move(payload), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEventStub(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnAbortPaymentEvent,
                                AsWeakPtr(), std::move(response_callback),
                                std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEventStub(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchCanMakePaymentEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnCanMakePaymentEvent,
                     AsWeakPtr(), std::move(event_data),
                     std::move(response_callback), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEventStub(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorker::DispatchPaymentRequestEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnPaymentRequestEvent,
                     AsWeakPtr(), std::move(event_data),
                     std::move(response_callback), std::move(callback)));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

}  // namespace content
