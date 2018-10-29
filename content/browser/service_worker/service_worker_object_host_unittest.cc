// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom.h"

namespace content {
namespace service_worker_object_host_unittest {

static void SaveStatusCallback(bool* called,
                               blink::ServiceWorkerStatusCode* out,
                               blink::ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

void SetUpDummyMessagePort(std::vector<blink::MessagePortChannel>* ports) {
  // Let the other end of the pipe close.
  mojo::MessagePipe pipe;
  ports->push_back(blink::MessagePortChannel(std::move(pipe.handle0)));
}

// A helper that holds on to ExtendableMessageEventPtr so it doesn't get
// destroyed after the message event handler runs.
class ExtendableMessageEventTestHelper : public EmbeddedWorkerTestHelper {
 public:
  ExtendableMessageEventTestHelper()
      : EmbeddedWorkerTestHelper(base::FilePath()) {}

  void OnExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      mojom::ServiceWorker::DispatchExtendableMessageEventCallback callback)
      override {
    events_.push_back(std::move(event));
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks::Now());
  }

  const std::vector<mojom::ExtendableMessageEventPtr>& events() {
    return events_;
  }

 private:
  std::vector<mojom::ExtendableMessageEventPtr> events_;
};

class FailToStartWorkerTestHelper : public ExtendableMessageEventTestHelper {
 public:
  FailToStartWorkerTestHelper() : ExtendableMessageEventTestHelper() {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerRequest service_worker_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    mojom::EmbeddedWorkerInstanceHostAssociatedPtr instance_host_ptr;
    instance_host_ptr.Bind(std::move(instance_host));
    instance_host_ptr->OnStopped();
    base::RunLoop().RunUntilIdle();
  }
};

class MockServiceWorkerObject : public blink::mojom::ServiceWorkerObject {
 public:
  explicit MockServiceWorkerObject(
      blink::mojom::ServiceWorkerObjectInfoPtr info)
      : info_(std::move(info)),
        state_(info_->state),
        binding_(this, std::move(info_->request)) {}
  ~MockServiceWorkerObject() override = default;

  blink::mojom::ServiceWorkerState state() const { return state_; }

 private:
  // Implements blink::mojom::ServiceWorkerObject.
  void StateChanged(blink::mojom::ServiceWorkerState state) override {
    state_ = state;
  }

  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  blink::mojom::ServiceWorkerState state_;
  mojo::AssociatedBinding<blink::mojom::ServiceWorkerObject> binding_;
};

class ServiceWorkerObjectHostTest : public testing::Test {
 public:
  ServiceWorkerObjectHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void Initialize(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_ = std::move(helper);
  }

  void SetUpRegistration(const GURL& scope, const GURL& script_url) {
    helper_->context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    registration_ = new ServiceWorkerRegistration(
        options, helper_->context()->storage()->NewRegistrationId(),
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url, blink::mojom::ScriptType::kClassic,
        helper_->context()->storage()->NewVersionId(),
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(net::HttpResponseInfo());
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    // Make the registration findable via storage functions.
    base::Optional<blink::ServiceWorkerStatusCode> status;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  }

  void TearDown() override {
    registration_ = nullptr;
    version_ = nullptr;
    helper_.reset();
  }

  void CallDispatchExtendableMessageEvent(
      ServiceWorkerObjectHost* object_host,
      ::blink::TransferableMessage message,
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback) {
    object_host->DispatchExtendableMessageEvent(std::move(message),
                                                std::move(callback));
  }

  size_t GetBindingsCount(ServiceWorkerObjectHost* object_host) {
    return object_host->bindings_.size();
  }

  ServiceWorkerObjectHost* GetServiceWorkerObjectHost(
      ServiceWorkerProviderHost* provider_host,
      int64_t version_id) {
    auto iter = provider_host->service_worker_object_hosts_.find(version_id);
    if (iter != provider_host->service_worker_object_hosts_.end())
      return iter->second.get();
    return nullptr;
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  GetRegistrationFromRemote(mojom::ServiceWorkerContainerHost* container_host,
                            const GURL& scope) {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info;
    base::RunLoop run_loop;
    container_host->GetRegistration(
        scope, base::BindOnce(
                   [](base::OnceClosure quit_closure,
                      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr*
                          out_registration_info,
                      blink::mojom::ServiceWorkerErrorType error,
                      const base::Optional<std::string>& error_msg,
                      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                          registration) {
                     ASSERT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
                               error);
                     *out_registration_info = std::move(registration);
                     std::move(quit_closure).Run();
                   },
                   run_loop.QuitClosure(), &registration_info));
    run_loop.Run();
    EXPECT_TRUE(registration_info);
    return registration_info;
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerObjectHostTest);
};

TEST_F(ServiceWorkerObjectHostTest, OnVersionStateChanged) {
  const int64_t kProviderId = 99;
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  SetUpRegistration(scope, script_url);
  registration_->SetInstallingVersion(version_);

  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      CreateProviderHostForWindow(
          helper_->mock_render_process_id(), kProviderId,
          true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
          &remote_endpoint);
  provider_host->SetDocumentUrl(scope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), scope);
  // |version_| is the installing version of |registration_| now.
  EXPECT_TRUE(registration_info->installing);
  EXPECT_EQ(version_->version_id(), registration_info->installing->version_id);
  auto mock_object = std::make_unique<MockServiceWorkerObject>(
      std::move(registration_info->installing));

  // ...update state to installed.
  EXPECT_EQ(blink::mojom::ServiceWorkerState::kInstalling,
            mock_object->state());
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::ServiceWorkerState::kInstalled, mock_object->state());
}

TEST_F(ServiceWorkerObjectHostTest,
       DispatchExtendableMessageEvent_FromServiceWorker) {
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<ExtendableMessageEventTestHelper>());
  SetUpRegistration(scope, script_url);

  base::SimpleTestTickClock tick_clock;
  // Set mock clock on version_ to check timeout behavior.
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  version_->SetTickClockForTesting(&tick_clock);

  // Make sure worker has a non-zero timeout.
  bool called = false;
  blink::ServiceWorkerStatusCode status =
      blink::ServiceWorkerStatusCode::kErrorFailed;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::ACTIVATE, base::DoNothing(),
      base::TimeDelta::FromSeconds(10), ServiceWorkerVersion::KILL_ON_TIMEOUT);

  // Advance clock by a couple seconds.
  tick_clock.Advance(base::TimeDelta::FromSeconds(4));
  base::TimeDelta remaining_time = version_->remaining_timeout();
  EXPECT_EQ(base::TimeDelta::FromSeconds(6), remaining_time);

  // Prepare a ServiceWorkerObjectHost corresponding to a JavaScript
  // ServiceWorker object in the service worker execution context for
  // |version_|.
  ServiceWorkerProviderHost* provider_host = version_->provider_host();
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerObjectHost(version_)
          ->CreateCompleteObjectInfoToSend();
  ServiceWorkerObjectHost* sender_worker_object_host =
      GetServiceWorkerObjectHost(provider_host, version_->version_id());
  EXPECT_EQ(1u, GetBindingsCount(sender_worker_object_host));

  // Dispatch an ExtendableMessageEvent simulating calling
  // ServiceWorker#postMessage() on the ServiceWorker object corresponding to
  // |service_worker_object_host|.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  called = false;
  status = blink::ServiceWorkerStatusCode::kErrorFailed;
  CallDispatchExtendableMessageEvent(
      sender_worker_object_host, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  // The dispatched ExtendableMessageEvent should be kept in
  // ExtendableMessageEventTestHelper, and the source service worker object info
  // should correspond to the pair (|version_->provider_host()|, |version_|),
  // means it should correspond to |sender_worker_object_host|.
  EXPECT_EQ(2u, GetBindingsCount(sender_worker_object_host));
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_client);
  EXPECT_TRUE(events[0]->source_info_for_service_worker);
  EXPECT_EQ(version_->version_id(),
            events[0]->source_info_for_service_worker->version_id);

  // Timeout of message event should not have extended life of service worker.
  EXPECT_EQ(remaining_time, version_->remaining_timeout());
  // Clean up.
  base::RunLoop stop_loop;
  version_->StopWorker(stop_loop.QuitClosure());
  stop_loop.Run();
}

TEST_F(ServiceWorkerObjectHostTest, DispatchExtendableMessageEvent_FromClient) {
  const int64_t kProviderId = 99;
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<ExtendableMessageEventTestHelper>());
  SetUpRegistration(scope, script_url);

  // Prepare a ServiceWorkerProviderHost for a window client. A
  // WebContents/RenderFrameHost must be created too because it's needed for
  // DispatchExtendableMessageEvent to populate ExtendableMessageEvent#source.
  RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(helper_->browser_context(),
                                               nullptr));
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  mojom::ServiceWorkerProviderHostInfoPtr provider_host_info =
      CreateProviderHostInfoForWindow(kProviderId, frame_host->GetRoutingID());
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::Create(frame_host->GetProcess()->GetID(),
                                        std::move(provider_host_info),
                                        helper_->context()->AsWeakPtr());
  provider_host->SetDocumentUrl(scope);
  // Prepare a ServiceWorkerObjectHost for the above |provider_host|.
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerObjectHost(version_)
          ->CreateCompleteObjectInfoToSend();
  ServiceWorkerObjectHost* object_host =
      GetServiceWorkerObjectHost(provider_host.get(), version_->version_id());

  // Simulate dispatching an ExtendableMessageEvent.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  bool called = false;
  blink::ServiceWorkerStatusCode status =
      blink::ServiceWorkerStatusCode::kErrorFailed;
  CallDispatchExtendableMessageEvent(
      object_host, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  // The dispatched ExtendableMessageEvent should be kept in
  // ExtendableMessageEventTestHelper, and its source client info should
  // correspond to |provider_host|.
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(1u, events.size());
  EXPECT_FALSE(events[0]->source_info_for_service_worker);
  EXPECT_TRUE(events[0]->source_info_for_client);
  EXPECT_EQ(provider_host->client_uuid(),
            events[0]->source_info_for_client->client_uuid);
  EXPECT_EQ(provider_host->client_type(),
            events[0]->source_info_for_client->client_type);
}

TEST_F(ServiceWorkerObjectHostTest, DispatchExtendableMessageEvent_Fail) {
  const int64_t kProviderId = 99;
  const GURL scope("https://www.example.com/");
  const GURL script_url("https://www.example.com/service_worker.js");
  Initialize(std::make_unique<FailToStartWorkerTestHelper>());
  SetUpRegistration(scope, script_url);

  // Prepare a ServiceWorkerProviderHost for a window client. A
  // WebContents/RenderFrameHost must be created too because it's needed for
  // DispatchExtendableMessageEvent to populate ExtendableMessageEvent#source.
  RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(helper_->browser_context(),
                                               nullptr));
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  mojom::ServiceWorkerProviderHostInfoPtr provider_host_info =
      CreateProviderHostInfoForWindow(kProviderId, frame_host->GetRoutingID());
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::Create(frame_host->GetProcess()->GetID(),
                                        std::move(provider_host_info),
                                        helper_->context()->AsWeakPtr());
  provider_host->SetDocumentUrl(scope);
  // Prepare a ServiceWorkerObjectHost for the above |provider_host|.
  blink::mojom::ServiceWorkerObjectInfoPtr info =
      provider_host->GetOrCreateServiceWorkerObjectHost(version_)
          ->CreateCompleteObjectInfoToSend();
  ServiceWorkerObjectHost* object_host =
      GetServiceWorkerObjectHost(provider_host.get(), version_->version_id());

  // Try to dispatch ExtendableMessageEvent. This should fail to start the
  // worker and to dispatch the event.
  blink::TransferableMessage message;
  SetUpDummyMessagePort(&message.ports);
  bool called = false;
  blink::ServiceWorkerStatusCode status = blink::ServiceWorkerStatusCode::kOk;
  CallDispatchExtendableMessageEvent(
      object_host, std::move(message),
      base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed, status);
  // No ExtendableMessageEvent has been dispatched.
  const std::vector<mojom::ExtendableMessageEventPtr>& events =
      static_cast<ExtendableMessageEventTestHelper*>(helper_.get())->events();
  EXPECT_EQ(0u, events.size());
}

}  // namespace service_worker_object_host_unittest
}  // namespace content
