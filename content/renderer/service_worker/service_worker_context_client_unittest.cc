// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_timeout_timer.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "content/renderer/worker_thread_registry.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_request_event_data.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_clients_info.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_registration_object_info.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data_consumer_handle.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace content {

namespace {

// Pipes connected to the context client.
struct ContextClientPipes {
  // From the browser to ServiceWorkerContextClient.
  mojom::ServiceWorkerPtr service_worker;
  mojom::ControllerServiceWorkerPtr controller;
  blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr registration;

  // From ServiceWorkerContextClient to the browser.
  blink::mojom::ServiceWorkerHostAssociatedRequest service_worker_host_request;
  mojom::EmbeddedWorkerInstanceHostAssociatedRequest
      embedded_worker_host_request;
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest
      registration_host_request;
};

class MockWebServiceWorkerContextProxy
    : public blink::WebServiceWorkerContextProxy {
 public:
  ~MockWebServiceWorkerContextProxy() override = default;

  void BindServiceWorkerHost(
      mojo::ScopedInterfaceEndpointHandle service_worker_host) override {}
  void SetRegistration(
      blink::WebServiceWorkerRegistrationObjectInfo info) override {
    DCHECK(!registration_object_info_);
    registration_object_info_ =
        std::make_unique<blink::WebServiceWorkerRegistrationObjectInfo>(
            std::move(info));
  }
  void ReadyToEvaluateScript() override {}
  bool HasFetchEventHandler() override { return false; }
  void DispatchFetchEvent(int fetch_event_id,
                          const blink::WebServiceWorkerRequest& web_request,
                          bool navigation_preload_sent) override {
    fetch_events_.emplace_back(fetch_event_id, web_request);
  }

  void DispatchActivateEvent(int event_id) override { NOTREACHED(); }
  void DispatchBackgroundFetchAbortEvent(
      int event_id,
      const blink::WebBackgroundFetchRegistration& registration) override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchClickEvent(
      int event_id,
      const blink::WebBackgroundFetchRegistration& registration) override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchFailEvent(
      int event_id,
      const blink::WebBackgroundFetchRegistration& registration) override {
    NOTREACHED();
  }
  void DispatchBackgroundFetchSuccessEvent(
      int event_id,
      const blink::WebBackgroundFetchRegistration& registration) override {
    NOTREACHED();
  }
  void DispatchCookieChangeEvent(
      int event_id,
      const blink::WebCanonicalCookie& cookie,
      ::network::mojom::CookieChangeCause change_cause) override {
    NOTREACHED();
  }
  void DispatchExtendableMessageEvent(
      int event_id,
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin,
      const blink::WebServiceWorkerClientInfo&) override {
    NOTREACHED();
  }
  void DispatchExtendableMessageEvent(
      int event_id,
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin,
      blink::WebServiceWorkerObjectInfo) override {
    NOTREACHED();
  }
  void DispatchInstallEvent(int event_id) override { NOTREACHED(); }
  void DispatchNotificationClickEvent(int event_id,
                                      const blink::WebString& notification_id,
                                      const blink::WebNotificationData&,
                                      int action_index,
                                      const blink::WebString& reply) override {
    NOTREACHED();
  }
  void DispatchNotificationCloseEvent(
      int event_id,
      const blink::WebString& notification_id,
      const blink::WebNotificationData&) override {
    NOTREACHED();
  }
  void DispatchPushEvent(int event_id, const blink::WebString& data) override {
    NOTREACHED();
  }
  void DispatchSyncEvent(int sync_event_id,
                         const blink::WebString& tag,
                         bool last_chance) override {
    NOTREACHED();
  }
  void DispatchAbortPaymentEvent(int event_id) override { NOTREACHED(); }
  void DispatchCanMakePaymentEvent(
      int event_id,
      const blink::WebCanMakePaymentEventData&) override {
    NOTREACHED();
  }
  void DispatchPaymentRequestEvent(
      int event_id,
      const blink::WebPaymentRequestEventData&) override {
    NOTREACHED();
  }
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse>,
      mojo::ScopedDataPipeConsumerHandle) override {
    NOTREACHED();
  }
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError>) override {
    NOTREACHED();
  }
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override {
    NOTREACHED();
  }

  const std::vector<
      std::pair<int /* event_id */, blink::WebServiceWorkerRequest>>&
  fetch_events() const {
    return fetch_events_;
  }

 private:
  std::unique_ptr<blink::WebServiceWorkerRegistrationObjectInfo>
      registration_object_info_;
  std::vector<std::pair<int /* event_id */, blink::WebServiceWorkerRequest>>
      fetch_events_;
};

base::RepeatingClosure CreateCallbackWithCalledFlag(bool* out_is_called) {
  return base::BindRepeating([](bool* out_is_called) { *out_is_called = true; },
                             out_is_called);
}

class MockServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost {
 public:
  explicit MockServiceWorkerObjectHost(int64_t version_id)
      : version_id_(version_id) {}
  ~MockServiceWorkerObjectHost() override = default;

  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo() {
    auto info = blink::mojom::ServiceWorkerObjectInfo::New();
    info->version_id = version_id_;
    bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
    info->request = mojo::MakeRequest(&remote_object_);
    return info;
  }

  int GetBindingCount() const { return bindings_.size(); }

 private:
  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessageToServiceWorker(
      ::blink::TransferableMessage message) override {
    NOTREACHED();
  }
  void TerminateForTesting(TerminateForTestingCallback callback) override {
    NOTREACHED();
  }

  const int64_t version_id_;
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerObjectHost> bindings_;
  blink::mojom::ServiceWorkerObjectAssociatedPtr remote_object_;
};

}  // namespace

class ServiceWorkerContextClientTest : public testing::Test {
 public:
  ServiceWorkerContextClientTest() = default;

 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    message_loop_.SetTaskRunner(task_runner_);
    // Use this thread as the worker thread.
    WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
  }

  void TearDown() override {
    ServiceWorkerContextClient::ResetThreadSpecificInstanceForTesting();
    // Unregister this thread from worker threads.
    WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
    task_runner_->RunUntilIdle();
  }

  void EnableServicification() {
    feature_list_.InitWithFeatures({network::features::kNetworkService}, {});
    ASSERT_TRUE(blink::ServiceWorkerUtils::IsServicificationEnabled());
  }

  // Creates an empty struct to initialize ServiceWorkerProviderContext.
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo() {
    auto info = mojom::ServiceWorkerProviderInfoForStartWorker::New();
    info->provider_id = 10;  // dummy
    return info;
  }

  // Creates an ContextClient, whose pipes are connected to |out_pipes|, then
  // simulates that the service worker thread has started with |proxy|.
  std::unique_ptr<ServiceWorkerContextClient> CreateContextClient(
      ContextClientPipes* out_pipes,
      blink::WebServiceWorkerContextProxy* proxy) {
    auto service_worker_request = mojo::MakeRequest(&out_pipes->service_worker);
    auto controller_request = mojo::MakeRequest(&out_pipes->controller);
    mojom::EmbeddedWorkerInstanceHostAssociatedPtr embedded_worker_host_ptr;
    out_pipes->embedded_worker_host_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&embedded_worker_host_ptr);
    const GURL kScope("https://example.com");
    const GURL kScript("https://example.com/SW.js");
    std::unique_ptr<ServiceWorkerContextClient> context_client =
        std::make_unique<ServiceWorkerContextClient>(
            1 /* embedded_worker_id */, 1 /* service_worker_version_id */,
            kScope, kScript, false /* is_script_streaming */,
            RendererPreferences(), std::move(service_worker_request),
            std::move(controller_request),
            embedded_worker_host_ptr.PassInterface(), CreateProviderInfo(),
            nullptr /* embedded_worker_client */,
            mojom::EmbeddedWorkerStartTiming::New(),
            nullptr /* preference_watcher_request */,
            nullptr /* subresource_loaders */,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting());

    context_client->WorkerContextStarted(proxy);

    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host;
    out_pipes->service_worker_host_request =
        mojo::MakeRequest(&service_worker_host);
    auto registration_info =
        blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    registration_info->registration_id = 100;  // dummy
    registration_info->options =
        blink::mojom::ServiceWorkerRegistrationOptions::New(
            kScope, blink::mojom::ScriptType::kClassic,
            blink::mojom::ServiceWorkerUpdateViaCache::kAll);
    out_pipes->registration_host_request =
        mojo::MakeRequest(&registration_info->host_ptr_info);
    registration_info->request = mojo::MakeRequest(&out_pipes->registration);
    out_pipes->service_worker->InitializeGlobalScope(
        std::move(service_worker_host), std::move(registration_info));
    task_runner()->RunUntilIdle();
    return context_client;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerContextClientTest, Ping) {
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);

  bool is_called = false;
  pipes.service_worker->Ping(CreateCallbackWithCalledFlag(&is_called));
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(is_called);
}

TEST_F(ServiceWorkerContextClientTest, DispatchFetchEvent) {
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);
  context_client->DidEvaluateScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  const GURL expected_url("https://example.com/expected");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
  blink::mojom::ServiceWorkerFetchResponseCallbackRequest
      fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = *request;
  pipes.service_worker->DispatchFetchEvent(
      std::move(params), std::move(fetch_callback_ptr),
      base::BindOnce(
          [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
  task_runner()->RunUntilIdle();

  ASSERT_EQ(1u, mock_proxy.fetch_events().size());
  EXPECT_EQ(expected_url,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
}

class HeaderContentRendererClient : public ContentRendererClient {
  bool IsExcludedHeaderForServiceWorkerFetchEvent(
      const std::string& header_name) override {
    return header_name == "x-bye-bye";
  }
};

TEST_F(ServiceWorkerContextClientTest, DispatchFetchEvent_Headers) {
  HeaderContentRendererClient header_client;
  auto* old_client = SetRendererClientForTesting(&header_client);

  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);
  context_client->DidEvaluateScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  const GURL expected_url("https://example.com/expected");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  request->headers.SetHeader("x-bye-bye", "excluded");
  request->headers.SetHeader("x-hi-hi", "present");
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
  blink::mojom::ServiceWorkerFetchResponseCallbackRequest
      fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = *request;
  pipes.service_worker->DispatchFetchEvent(
      std::move(params), std::move(fetch_callback_ptr),
      base::BindOnce(
          [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
  task_runner()->RunUntilIdle();

  ASSERT_EQ(1u, mock_proxy.fetch_events().size());
  const blink::WebServiceWorkerRequest& received_request =
      mock_proxy.fetch_events()[0].second;
  ServiceWorkerHeaderMap header_map;
  GetServiceWorkerHeaderMapFromWebRequest(received_request, &header_map);

  EXPECT_EQ(expected_url, static_cast<GURL>(received_request.Url()));
  EXPECT_TRUE(header_map.find("x-bye-bye") == header_map.end());
  auto iter = header_map.find("x-hi-hi");
  ASSERT_TRUE(iter != header_map.end());
  EXPECT_EQ("present", iter->second);

  SetRendererClientForTesting(old_client);
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_NotRequestedTermination) {
  EnableServicification();
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);
  context_client->DidEvaluateScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->GetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // The dispatched fetch event should be recorded by |mock_proxy|.
  const GURL expected_url("https://example.com/expected");
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
  blink::mojom::ServiceWorkerFetchResponseCallbackRequest
      fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = expected_url;
  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = *request;
  context_client->DispatchOrQueueFetchEvent(
      std::move(params), std::move(fetch_callback_ptr),
      base::BindOnce(
          [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
  task_runner()->RunUntilIdle();

  EXPECT_FALSE(context_client->RequestedTermination());
  ASSERT_EQ(1u, mock_proxy.fetch_events().size());
  EXPECT_EQ(expected_url,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_RequestedTerminationAndDie) {
  EnableServicification();
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);
  context_client->DidEvaluateScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->GetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // Ensure the idle state.
  EXPECT_FALSE(context_client->RequestedTermination());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(context_client->RequestedTermination());

  const GURL expected_url("https://example.com/expected");

  // FetchEvent dispatched directly from the controlled clients through
  // mojom::ControllerServiceWorker should be queued in the idle state.
  {
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    blink::mojom::ServiceWorkerFetchResponseCallbackRequest
        fetch_callback_request = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url;
    auto params = blink::mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.controller->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  // Destruction of |context_client| should not hit any DCHECKs.
  context_client.reset();
}

TEST_F(ServiceWorkerContextClientTest,
       DispatchOrQueueFetchEvent_RequestedTerminationAndWakeUp) {
  EnableServicification();
  ContextClientPipes pipes;
  MockWebServiceWorkerContextProxy mock_proxy;
  std::unique_ptr<ServiceWorkerContextClient> context_client =
      CreateContextClient(&pipes, &mock_proxy);
  context_client->DidEvaluateScript(true /* success */);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(mock_proxy.fetch_events().empty());
  bool is_idle = false;
  auto timer = std::make_unique<ServiceWorkerTimeoutTimer>(
      CreateCallbackWithCalledFlag(&is_idle),
      task_runner()->GetMockTickClock());
  context_client->SetTimeoutTimerForTesting(std::move(timer));

  // Ensure the idle state.
  EXPECT_FALSE(context_client->RequestedTermination());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(context_client->RequestedTermination());

  const GURL expected_url_1("https://example.com/expected_1");
  const GURL expected_url_2("https://example.com/expected_2");
  blink::mojom::ServiceWorkerFetchResponseCallbackRequest
      fetch_callback_request_1;
  blink::mojom::ServiceWorkerFetchResponseCallbackRequest
      fetch_callback_request_2;

  // FetchEvent dispatched directly from the controlled clients through
  // mojom::ControllerServiceWorker should be queued in the idle state.
  {
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    fetch_callback_request_1 = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url_1;
    auto params = blink::mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.controller->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_TRUE(mock_proxy.fetch_events().empty());

  // Another event dispatched to mojom::ServiceWorker wakes up
  // the context client.
  {
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr fetch_callback_ptr;
    fetch_callback_request_2 = mojo::MakeRequest(&fetch_callback_ptr);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = expected_url_2;
    auto params = blink::mojom::DispatchFetchEventParams::New();
    params->request = *request;
    pipes.service_worker->DispatchFetchEvent(
        std::move(params), std::move(fetch_callback_ptr),
        base::BindOnce(
            [](blink::mojom::ServiceWorkerEventStatus, base::TimeTicks) {}));
    task_runner()->RunUntilIdle();
  }
  EXPECT_FALSE(context_client->RequestedTermination());

  // All events should fire. The order of events should be kept.
  ASSERT_EQ(2u, mock_proxy.fetch_events().size());
  EXPECT_EQ(expected_url_1,
            static_cast<GURL>(mock_proxy.fetch_events()[0].second.Url()));
  EXPECT_EQ(expected_url_2,
            static_cast<GURL>(mock_proxy.fetch_events()[1].second.Url()));
}

}  // namespace content
