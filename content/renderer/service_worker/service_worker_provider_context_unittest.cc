// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_feature.mojom.h"

namespace content {
namespace service_worker_provider_context_unittest {

class MockServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost {
 public:
  explicit MockServiceWorkerObjectHost(int64_t version_id)
      : version_id_(version_id) {
    bindings_.set_connection_error_handler(
        base::BindRepeating(&MockServiceWorkerObjectHost::OnConnectionError,
                            base::Unretained(this)));
  }
  ~MockServiceWorkerObjectHost() override = default;

  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo() {
    auto info = blink::mojom::ServiceWorkerObjectInfo::New();
    info->version_id = version_id_;
    bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
    info->request = mojo::MakeRequest(&remote_object_);
    return info;
  }

  void OnConnectionError() {
    if (error_callback_)
      std::move(error_callback_).Run();
  }

  void RunOnConnectionError(base::OnceClosure error_callback) {
    DCHECK(!error_callback_);
    error_callback_ = std::move(error_callback);
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
  base::OnceClosure error_callback_;
};

class MockWebServiceWorkerProviderClientImpl
    : public blink::WebServiceWorkerProviderClient {
 public:
  MockWebServiceWorkerProviderClientImpl() {}

  ~MockWebServiceWorkerProviderClientImpl() override {}

  void SetController(blink::WebServiceWorkerObjectInfo info,
                     bool should_notify_controller_change) override {
    was_set_controller_called_ = true;
  }

  void DispatchMessageEvent(blink::WebServiceWorkerObjectInfo info,
                            blink::TransferableMessage message) override {
    was_dispatch_message_event_called_ = true;
  }

  void CountFeature(blink::mojom::WebFeature feature) override {
    used_features_.insert(feature);
  }

  bool was_set_controller_called() const { return was_set_controller_called_; }

  bool was_dispatch_message_event_called() const {
    return was_dispatch_message_event_called_;
  }

  const std::set<blink::mojom::WebFeature>& used_features() const {
    return used_features_;
  }

 private:
  bool was_set_controller_called_ = false;
  bool was_dispatch_message_event_called_ = false;
  std::set<blink::mojom::WebFeature> used_features_;
};

// S13nServiceWorker: a fake URLLoaderFactory implementation that basically
// does nothing but records the requests.
class FakeURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;
  ~FakeURLLoaderFactory() override = default;

  void AddBinding(network::mojom::URLLoaderFactoryRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    // Does nothing, but just record the request and hold the client (to avoid
    // connection errors).
    last_url_ = url_request.url;
    clients_.push_back(std::move(client));
    if (start_loader_callback_)
      std::move(start_loader_callback_).Run();
  }
  void Clone(network::mojom::URLLoaderFactoryRequest factory) override {
    bindings_.AddBinding(this, std::move(factory));
  }

  void set_start_loader_callback(base::OnceClosure closure) {
    start_loader_callback_ = std::move(closure);
  }

  size_t clients_count() const { return clients_.size(); }
  GURL last_request_url() const { return last_url_; }

 private:
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  std::vector<network::mojom::URLLoaderClientPtr> clients_;
  base::OnceClosure start_loader_callback_;
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLLoaderFactory);
};

// S13nServiceWorker: a fake ControllerServiceWorker implementation that
// basically does nothing but records DispatchFetchEvent calls.
class FakeControllerServiceWorker : public mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;
  ~FakeControllerServiceWorker() override = default;

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    fetch_event_count_++;
    fetch_event_request_ = params->request;
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks());
    if (fetch_event_callback_)
      std::move(fetch_event_callback_).Run();
  }
  void Clone(mojom::ControllerServiceWorkerRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void set_fetch_callback(base::OnceClosure closure) {
    fetch_event_callback_ = std::move(closure);
  }
  int fetch_event_count() const { return fetch_event_count_; }
  const network::ResourceRequest& fetch_event_request() const {
    return fetch_event_request_;
  }

  void Disconnect() { bindings_.CloseAllBindings(); }

 private:
  int fetch_event_count_ = 0;
  network::ResourceRequest fetch_event_request_;
  base::OnceClosure fetch_event_callback_;
  mojo::BindingSet<mojom::ControllerServiceWorker> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeControllerServiceWorker);
};

class FakeServiceWorkerContainerHost
    : public mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      mojom::ServiceWorkerContainerHostAssociatedRequest request)
      : associated_binding_(this, std::move(request)) {}
  ~FakeServiceWorkerContainerHost() override = default;

  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrations(GetRegistrationsCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override {
    NOTIMPLEMENTED();
  }
  void EnsureControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest request,
      mojom::ControllerServiceWorkerPurpose purpose) override {
    NOTIMPLEMENTED();
  }
  void CloneContainerHost(
      mojom::ServiceWorkerContainerHostRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }
  void Ping(PingCallback callback) override { NOTIMPLEMENTED(); }
  void HintToUpdateServiceWorker() override { NOTIMPLEMENTED(); }

 private:
  mojo::BindingSet<mojom::ServiceWorkerContainerHost> bindings_;
  mojo::AssociatedBinding<mojom::ServiceWorkerContainerHost>
      associated_binding_;
  DISALLOW_COPY_AND_ASSIGN(FakeServiceWorkerContainerHost);
};

class ServiceWorkerProviderContextTest : public testing::Test {
 public:
  ServiceWorkerProviderContextTest() = default;

  void EnableS13nServiceWorker() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
    network::mojom::URLLoaderFactoryPtr fake_loader_factory;
    fake_loader_factory_.AddBinding(MakeRequest(&fake_loader_factory));
    loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(fake_loader_factory));
  }

  void StartRequest(network::mojom::URLLoaderFactory* factory,
                    const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.resource_type = static_cast<int>(RESOURCE_TYPE_SUB_RESOURCE);
    network::mojom::URLLoaderPtr loader;
    network::TestURLLoaderClient loader_client;
    factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0, 0, network::mojom::kURLLoadOptionNone,
        request, loader_client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void FlushControllerConnector(
      ServiceWorkerProviderContext* provider_context) {
    provider_context->state_for_client_->controller_connector.FlushForTesting();
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment;

  // S13nServiceWorker:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeURLLoaderFactory fake_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContextTest);
};

TEST_F(ServiceWorkerProviderContextTest, SetController) {
  const int kProviderId = 10;

  {
    auto mock_service_worker_object_host =
        std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
    ASSERT_EQ(0, mock_service_worker_object_host->GetBindingCount());
    blink::mojom::ServiceWorkerObjectInfoPtr object_info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());

    // (1) In the case there is no WebSWProviderClient but SWProviderContext for
    // the provider, the passed reference should be adopted and owned by the
    // provider context.
    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
        std::move(container_request), nullptr /* host_ptr_info */,
        nullptr /* controller_info */, nullptr /* loader_factory*/);

    auto info = mojom::ControllerServiceWorkerInfo::New();
    info->mode = blink::mojom::ControllerServiceWorkerMode::kControlled;
    info->object_info = std::move(object_info);
    container_ptr->SetController(std::move(info),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();

    // Destruction of the provider context should release references to the
    // the controller.
    provider_context = nullptr;
    base::RunLoop().RunUntilIdle();
    // ServiceWorkerObjectHost Mojo connection got broken.
    EXPECT_EQ(0, mock_service_worker_object_host->GetBindingCount());
  }

  {
    auto mock_service_worker_object_host =
        std::make_unique<MockServiceWorkerObjectHost>(201 /* version_id */);
    ASSERT_EQ(0, mock_service_worker_object_host->GetBindingCount());
    blink::mojom::ServiceWorkerObjectInfoPtr object_info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());

    // (2) In the case there are both SWProviderContext and SWProviderClient for
    // the provider, the passed reference should be adopted by the provider
    // context and then be transfered ownership to the provider client, after
    // that due to limitation of the mock implementation, the reference
    // immediately gets released.
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
    mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
        mojo::MakeRequest(&host_ptr_info);

    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
        std::move(container_request), std::move(host_ptr_info),
        nullptr /* controller_info */, nullptr /* loader_factory*/);
    auto provider_impl =
        std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
    auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
    provider_impl->SetClient(client.get());
    ASSERT_FALSE(client->was_set_controller_called());

    auto info = mojom::ControllerServiceWorkerInfo::New();
    info->mode = blink::mojom::ControllerServiceWorkerMode::kControlled;
    info->object_info = std::move(object_info);
    container_ptr->SetController(std::move(info),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(client->was_set_controller_called());
    // ServiceWorkerObjectHost Mojo connection got broken.
    EXPECT_EQ(0, mock_service_worker_object_host->GetBindingCount());
  }
}

// Test that clearing the controller by sending a nullptr object info results in
// the provider context having a null controller.
TEST_F(ServiceWorkerProviderContextTest, SetController_Null) {
  const int kProviderId = 10;

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());

  container_ptr->SetController(mojom::ControllerServiceWorkerInfo::New(),
                               std::vector<blink::mojom::WebFeature>(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider_context->TakeController());
  EXPECT_TRUE(client->was_set_controller_called());
}

// S13nServiceWorker: Test that SetController correctly sets (or resets)
// the controller service worker for clients.
TEST_F(ServiceWorkerProviderContextTest, SetControllerServiceWorker) {
  EnableS13nServiceWorker();
  const int kProviderId = 10;

  // Make the ServiceWorkerContainerHost implementation and
  // ServiceWorkerContainer request.
  mojom::ServiceWorkerContainerHostAssociatedPtr host_ptr;
  FakeServiceWorkerContainerHost host(
      mojo::MakeRequestAssociatedWithDedicatedPipe(&host_ptr));
  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);

  // (1) Test if setting the controller via the CTOR works.

  // Make the object host for .controller.
  auto object_host1 =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
  EXPECT_EQ(0, object_host1->GetBindingCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info1 =
      object_host1->CreateObjectInfo();
  EXPECT_EQ(1, object_host1->GetBindingCount());

  // Make the ControllerServiceWorkerInfo.
  FakeControllerServiceWorker fake_controller1;
  auto controller_info1 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr1;
  fake_controller1.Clone(mojo::MakeRequest(&controller_ptr1));
  controller_info1->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info1->object_info = std::move(object_info1);
  controller_info1->endpoint = controller_ptr1.PassInterface();

  // Make the ServiceWorkerProviderContext, pasing it the controller, container,
  // and container host.
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), host_ptr.PassInterface(),
      std::move(controller_info1), loader_factory_);

  // The subresource loader factory must be available.
  network::mojom::URLLoaderFactory* subresource_loader_factory1 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory1);

  // Performing a request should reach the controller.
  const GURL kURL1("https://www.example.com/foo.png");
  base::RunLoop loop1;
  fake_controller1.set_fetch_callback(loop1.QuitClosure());
  StartRequest(subresource_loader_factory1, kURL1);
  loop1.Run();
  EXPECT_EQ(kURL1, fake_controller1.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller1.fetch_event_count());

  // (2) Test if resetting the controller to a new one via SetController
  // works.

  // Setup the new controller.
  auto object_host2 =
      std::make_unique<MockServiceWorkerObjectHost>(201 /* version_id */);
  ASSERT_EQ(0, object_host2->GetBindingCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info2 =
      object_host2->CreateObjectInfo();
  EXPECT_EQ(1, object_host2->GetBindingCount());
  FakeControllerServiceWorker fake_controller2;
  auto controller_info2 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr2;
  fake_controller2.Clone(mojo::MakeRequest(&controller_ptr2));
  controller_info2->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info2->object_info = std::move(object_info2);
  controller_info2->endpoint = controller_ptr2.PassInterface();

  // Resetting the controller will trigger many things happening, including the
  // object binding being broken.
  base::RunLoop drop_binding_loop;
  object_host1->RunOnConnectionError(drop_binding_loop.QuitClosure());
  container_ptr->SetController(std::move(controller_info2),
                               std::vector<blink::mojom::WebFeature>(), true);
  container_ptr.FlushForTesting();
  drop_binding_loop.Run();
  EXPECT_EQ(0, object_host1->GetBindingCount());

  // Subresource loader factory must be available, and should be the same
  // one as we got before.
  network::mojom::URLLoaderFactory* subresource_loader_factory2 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory2);
  EXPECT_EQ(subresource_loader_factory1, subresource_loader_factory2);

  // The SetController() call results in another Mojo call to
  // ControllerServiceWorkerConnector.UpdateController(). Flush that interface
  // pointer to ensure the message was received.
  FlushControllerConnector(provider_context.get());

  // Performing a request should reach the new controller.
  const GURL kURL2("https://www.example.com/foo2.png");
  base::RunLoop loop2;
  fake_controller2.set_fetch_callback(loop2.QuitClosure());
  StartRequest(subresource_loader_factory2, kURL2);
  loop2.Run();
  EXPECT_EQ(kURL2, fake_controller2.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller2.fetch_event_count());
  // The request should not go to the previous controller.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());

  // (3) Test if resetting the controller to nullptr works.
  base::RunLoop drop_binding_loop2;
  object_host2->RunOnConnectionError(drop_binding_loop2.QuitClosure());
  container_ptr->SetController(mojom::ControllerServiceWorkerInfo::New(),
                               std::vector<blink::mojom::WebFeature>(), true);

  // The controller is reset. References to the old controller must be
  // released.
  container_ptr.FlushForTesting();
  drop_binding_loop2.Run();
  EXPECT_EQ(0, object_host2->GetBindingCount());

  // Subresource loader factory must not be available.
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactory());

  // The SetController() call results in another Mojo call to
  // ControllerServiceWorkerConnector.UpdateController(). Flush that interface
  // pointer to ensure the message was received.
  FlushControllerConnector(provider_context.get());

  // Performing a request using the subresource factory obtained before
  // falls back to the network.
  const GURL kURL3("https://www.example.com/foo3.png");
  base::RunLoop loop3;
  fake_loader_factory_.set_start_loader_callback(loop3.QuitClosure());
  EXPECT_EQ(0UL, fake_loader_factory_.clients_count());
  StartRequest(subresource_loader_factory2, kURL3);
  loop3.Run();
  EXPECT_EQ(kURL3, fake_loader_factory_.last_request_url());
  EXPECT_EQ(1UL, fake_loader_factory_.clients_count());

  // The request should not go to the previous controllers.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());
  EXPECT_EQ(1, fake_controller2.fetch_event_count());

  // (4) Test if resetting the controller to yet another one via SetController
  // works.
  auto object_host4 =
      std::make_unique<MockServiceWorkerObjectHost>(202 /* version_id */);
  ASSERT_EQ(0, object_host4->GetBindingCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info4 =
      object_host4->CreateObjectInfo();
  EXPECT_EQ(1, object_host4->GetBindingCount());
  FakeControllerServiceWorker fake_controller4;
  auto controller_info4 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr4;
  fake_controller4.Clone(mojo::MakeRequest(&controller_ptr4));
  controller_info4->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info4->object_info = std::move(object_info4);
  controller_info4->endpoint = controller_ptr4.PassInterface();
  container_ptr->SetController(std::move(controller_info4),
                               std::vector<blink::mojom::WebFeature>(), true);
  container_ptr.FlushForTesting();

  // Subresource loader factory must be available.
  auto* subresource_loader_factory4 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory4);

  // The SetController() call results in another Mojo call to
  // ControllerServiceWorkerConnector.UpdateController(). Flush that interface
  // pointer to ensure the message was received.
  FlushControllerConnector(provider_context.get());

  // Performing a request should reach the new controller.
  const GURL kURL4("https://www.example.com/foo4.png");
  base::RunLoop loop4;
  fake_controller4.set_fetch_callback(loop4.QuitClosure());
  StartRequest(subresource_loader_factory4, kURL4);
  loop4.Run();
  EXPECT_EQ(kURL4, fake_controller4.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller4.fetch_event_count());

  // The request should not go to the previous controllers.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());
  EXPECT_EQ(1, fake_controller2.fetch_event_count());
  // The request should not go to the network.
  EXPECT_EQ(1UL, fake_loader_factory_.clients_count());

  // Perform a request again, but then drop the controller connection.
  // The outcome is not deterministic but should not crash.
  StartRequest(subresource_loader_factory4, kURL4);
  fake_controller4.Disconnect();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerProviderContextTest, ControllerWithoutFetchHandler) {
  EnableS13nServiceWorker();
  const int kProviderId = 10;
  auto object_host =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);

  // Set a controller without ControllerServiceWorker ptr to emulate no
  // fetch event handler.
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      object_host->CreateObjectInfo();
  auto controller_info = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr;
  controller_info->mode =
      blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler;
  controller_info->object_info = std::move(object_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), nullptr /* host_ptr_info */,
      std::move(controller_info), loader_factory_);
  base::RunLoop().RunUntilIdle();

  // Subresource loader factory must not be available.
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactory());
}

TEST_F(ServiceWorkerProviderContextTest, PostMessageToClient) {
  const int kProviderId = 10;

  auto mock_service_worker_object_host =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
  ASSERT_EQ(0, mock_service_worker_object_host->GetBindingCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      mock_service_worker_object_host->CreateObjectInfo();
  EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());
  ASSERT_FALSE(client->was_dispatch_message_event_called());

  container_ptr->PostMessageToClient(std::move(object_info),
                                     blink::TransferableMessage());
  base::RunLoop().RunUntilIdle();

  // The passed reference should be owned by the provider client (but the
  // reference is immediately released by the mock provider client).
  EXPECT_TRUE(client->was_dispatch_message_event_called());
  EXPECT_EQ(0, mock_service_worker_object_host->GetBindingCount());
}

TEST_F(ServiceWorkerProviderContextTest, CountFeature) {
  const int kProviderId = 10;

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();

  container_ptr->CountFeature(blink::mojom::WebFeature::kWorkerStart);
  provider_impl->SetClient(client.get());
  base::RunLoop().RunUntilIdle();

  // Calling CountFeature() before client is set will save the feature usage in
  // the set, and once SetClient() is called it gets propagated to the client.
  ASSERT_EQ(1UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWorkerStart,
            *(client->used_features().begin()));

  container_ptr->CountFeature(blink::mojom::WebFeature::kWindowEvent);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWindowEvent,
            *(++(client->used_features().begin())));
}

}  // namespace service_worker_provider_context_unittest
}  // namespace content
