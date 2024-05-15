// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_features.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {
namespace service_worker_provider_context_unittest {

class MockServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost {
 public:
  explicit MockServiceWorkerObjectHost(int64_t version_id)
      : version_id_(version_id) {
    receivers_.set_disconnect_handler(
        base::BindRepeating(&MockServiceWorkerObjectHost::OnConnectionError,
                            base::Unretained(this)));
  }
  ~MockServiceWorkerObjectHost() override = default;

  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo() {
    auto info = blink::mojom::ServiceWorkerObjectInfo::New();
    info->version_id = version_id_;
    receivers_.Add(this,
                   info->host_remote.InitWithNewEndpointAndPassReceiver());
    info->receiver = remote_object_.BindNewEndpointAndPassReceiver();
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

  int GetReceiverCount() const { return receivers_.size(); }

 private:
  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessageToServiceWorker(
      ::blink::TransferableMessage message) override {
    NOTREACHED_IN_MIGRATION();
  }
  void TerminateForTesting(TerminateForTestingCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  const int64_t version_id_;
  mojo::AssociatedReceiverSet<blink::mojom::ServiceWorkerObjectHost> receivers_;
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerObject> remote_object_;
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

  void ReceiveMessage(blink::WebServiceWorkerObjectInfo info,
                      blink::TransferableMessage message) override {
    was_receive_message_called_ = true;
  }

  void CountFeature(blink::mojom::WebFeature feature) override {
    used_features_.insert(feature);
  }

  bool was_set_controller_called() const { return was_set_controller_called_; }

  bool was_receive_message_called() const {
    return was_receive_message_called_;
  }

  const std::set<blink::mojom::WebFeature>& used_features() const {
    return used_features_;
  }

 private:
  bool was_set_controller_called_ = false;
  bool was_receive_message_called_ = false;
  std::set<blink::mojom::WebFeature> used_features_;
};

// A fake URLLoaderFactory implementation that basically does nothing but
// records the requests.
class FakeURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;

  FakeURLLoaderFactory(const FakeURLLoaderFactory&) = delete;
  FakeURLLoaderFactory& operator=(const FakeURLLoaderFactory&) = delete;

  ~FakeURLLoaderFactory() override = default;

  void AddReceiver(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    // Does nothing, but just record the request and hold the client (to avoid
    // connection errors).
    last_url_ = url_request.url;
    clients_.push_back(std::move(client));
    if (start_loader_callback_)
      std::move(start_loader_callback_).Run();
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override {
    receivers_.Add(this, std::move(factory));
  }

  void set_start_loader_callback(base::OnceClosure closure) {
    start_loader_callback_ = std::move(closure);
  }

  size_t clients_count() const { return clients_.size(); }
  GURL last_request_url() const { return last_url_; }

 private:
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  std::vector<mojo::PendingRemote<network::mojom::URLLoaderClient>> clients_;
  base::OnceClosure start_loader_callback_;
  GURL last_url_;
};

// A fake ControllerServiceWorker implementation that basically does nothing but
// records DispatchFetchEvent calls.
class FakeControllerServiceWorker
    : public blink::mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;

  FakeControllerServiceWorker(const FakeControllerServiceWorker&) = delete;
  FakeControllerServiceWorker& operator=(const FakeControllerServiceWorker&) =
      delete;

  ~FakeControllerServiceWorker() override = default;

  // blink::mojom::ControllerServiceWorker:
  void DispatchFetchEventForSubresource(
      blink::mojom::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
          response_callback,
      DispatchFetchEventForSubresourceCallback callback) override {
    fetch_event_count_++;
    fetch_event_request_ = std::move(params->request);
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
    if (fetch_event_callback_)
      std::move(fetch_event_callback_).Run();
  }
  void Clone(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      const network::CrossOriginEmbedderPolicy&,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>)
      override {
    receivers_.Add(this, std::move(receiver));
  }

  void set_fetch_callback(base::OnceClosure closure) {
    fetch_event_callback_ = std::move(closure);
  }
  int fetch_event_count() const { return fetch_event_count_; }
  const blink::mojom::FetchAPIRequest& fetch_event_request() const {
    return *fetch_event_request_;
  }

  void Disconnect() { receivers_.Clear(); }

 private:
  int fetch_event_count_ = 0;
  blink::mojom::FetchAPIRequestPtr fetch_event_request_;
  base::OnceClosure fetch_event_callback_;
  mojo::ReceiverSet<blink::mojom::ControllerServiceWorker> receivers_;
};

class FakeServiceWorkerContainerHost
    : public blink::mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          receiver)
      : associated_receiver_(this, std::move(receiver)) {}

  FakeServiceWorkerContainerHost(const FakeServiceWorkerContainerHost&) =
      delete;
  FakeServiceWorkerContainerHost& operator=(
      const FakeServiceWorkerContainerHost&) = delete;

  ~FakeServiceWorkerContainerHost() override = default;

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
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
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override {
    NOTIMPLEMENTED();
  }
  void CloneContainerHost(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver)
      override {
    receivers_.Add(this, std::move(receiver));
  }
  void HintToUpdateServiceWorker() override { NOTIMPLEMENTED(); }
  void EnsureFileAccess(const std::vector<base::FilePath>& files,
                        EnsureFileAccessCallback callback) override {
    std::move(callback).Run();
  }
  void OnExecutionReady() override {}

 private:
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost> receivers_;
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      associated_receiver_;
};

class ServiceWorkerProviderContextTest : public testing::Test {
 public:
  ServiceWorkerProviderContextTest() = default;

  ServiceWorkerProviderContextTest(const ServiceWorkerProviderContextTest&) =
      delete;
  ServiceWorkerProviderContextTest& operator=(
      const ServiceWorkerProviderContextTest&) = delete;

  void EnableNetworkService() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fake_loader_factory;
    fake_loader_factory_.AddReceiver(
        fake_loader_factory.InitWithNewPipeAndPassReceiver());
    loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(fake_loader_factory));
  }

  void StartRequest(network::mojom::URLLoaderFactory* factory,
                    const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.destination = network::mojom::RequestDestination::kEmpty;
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    network::TestURLLoaderClient loader_client;
    factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), NextRequestId(),
        network::mojom::kURLLoadOptionNone, request,
        loader_client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void FlushControllerConnector(
      ServiceWorkerProviderContext* provider_context) {
    provider_context->controller_connector_.FlushForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment;
  FakeURLLoaderFactory fake_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

 private:
  int NextRequestId() { return request_id_++; }

  int request_id_ = 0;
};

TEST_F(ServiceWorkerProviderContextTest, SetController) {
  {
    auto mock_service_worker_object_host =
        std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
    ASSERT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
    blink::mojom::ServiceWorkerObjectInfoPtr object_info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(1, mock_service_worker_object_host->GetReceiverCount());

    mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>
        host_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();

    // (1) In the case there is no WebSWProviderClient but SWProviderContext for
    // the provider, the passed reference should be adopted and owned by the
    // provider context.
    mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote;
    auto container_receiver =
        container_remote.BindNewEndpointAndPassDedicatedReceiver();
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        blink::mojom::ServiceWorkerContainerType::kForWindow,
        std::move(container_receiver), host_remote.Unbind(),
        nullptr /* controller_info */, nullptr /* loader_factory*/);

    auto info = blink::mojom::ControllerServiceWorkerInfo::New();
    info->mode = blink::mojom::ControllerServiceWorkerMode::kControlled;
    info->fetch_handler_type =
        blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
    info->object_info = std::move(object_info);
    container_remote->SetController(std::move(info), true);
    base::RunLoop().RunUntilIdle();

    // Destruction of the provider context should release references to the
    // the controller.
    provider_context = nullptr;
    base::RunLoop().RunUntilIdle();
    // ServiceWorkerObjectHost Mojo connection got broken.
    EXPECT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
  }

  {
    auto mock_service_worker_object_host =
        std::make_unique<MockServiceWorkerObjectHost>(201 /* version_id */);
    ASSERT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
    blink::mojom::ServiceWorkerObjectInfoPtr object_info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(1, mock_service_worker_object_host->GetReceiverCount());

    // (2) In the case there are both SWProviderContext and SWProviderClient for
    // the provider, the passed reference should be adopted by the provider
    // context and then be transfered ownership to the provider client, after
    // that due to limitation of the mock implementation, the reference
    // immediately gets released.
    mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>
        host_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();

    mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote;
    auto container_receiver =
        container_remote.BindNewEndpointAndPassDedicatedReceiver();
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        blink::mojom::ServiceWorkerContainerType::kForWindow,
        std::move(container_receiver), host_remote.Unbind(),
        nullptr /* controller_info */, nullptr /* loader_factory*/);
    auto provider_impl =
        std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
    auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
    provider_impl->SetClient(client.get());
    ASSERT_FALSE(client->was_set_controller_called());

    auto info = blink::mojom::ControllerServiceWorkerInfo::New();
    info->mode = blink::mojom::ControllerServiceWorkerMode::kControlled;
    info->fetch_handler_type =
        blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
    info->object_info = std::move(object_info);
    container_remote->SetController(std::move(info), true);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(client->was_set_controller_called());
    // ServiceWorkerObjectHost Mojo connection got broken.
    EXPECT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
  }
}

// Test that clearing the controller by sending a nullptr object info results in
// the provider context having a null controller.
TEST_F(ServiceWorkerProviderContextTest, SetController_Null) {
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());

  container_remote->SetController(
      blink::mojom::ControllerServiceWorkerInfo::New(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider_context->TakeController());
  EXPECT_TRUE(client->was_set_controller_called());
}

// Test that SetController correctly sets (or resets) the controller service
// worker for clients.
TEST_F(ServiceWorkerProviderContextTest, SetControllerServiceWorker) {
  EnableNetworkService();

  // Make the ServiceWorkerContainerHost implementation and
  // ServiceWorkerContainer request.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  FakeServiceWorkerContainerHost host(
      host_remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();

  // (1) Test if setting the controller via the CTOR works.

  // Make the object host for .controller.
  auto object_host1 =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
  EXPECT_EQ(0, object_host1->GetReceiverCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info1 =
      object_host1->CreateObjectInfo();
  EXPECT_EQ(1, object_host1->GetReceiverCount());

  // Make the ControllerServiceWorkerInfo.
  FakeControllerServiceWorker fake_controller1;
  auto controller_info1 = blink::mojom::ControllerServiceWorkerInfo::New();
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller1;
  fake_controller1.Clone(remote_controller1.BindNewPipeAndPassReceiver(),
                         network::CrossOriginEmbedderPolicy(),
                         mojo::NullRemote());
  controller_info1->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info1->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  controller_info1->object_info = std::move(object_info1);
  controller_info1->remote_controller = remote_controller1.Unbind();

  // Make the ServiceWorkerProviderContext, passing it the controller,
  // container, and container host.
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      std::move(controller_info1), loader_factory_);

  // The subresource loader factory must be available.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      wrapped_loader_factory1 = provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, wrapped_loader_factory1);
  network::mojom::URLLoaderFactory* subresource_loader_factory1 =
      provider_context->GetSubresourceLoaderFactoryInternal();
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
  ASSERT_EQ(0, object_host2->GetReceiverCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info2 =
      object_host2->CreateObjectInfo();
  EXPECT_EQ(1, object_host2->GetReceiverCount());
  FakeControllerServiceWorker fake_controller2;
  auto controller_info2 = blink::mojom::ControllerServiceWorkerInfo::New();
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller2;
  fake_controller2.Clone(remote_controller2.BindNewPipeAndPassReceiver(),
                         network::CrossOriginEmbedderPolicy(),
                         mojo::NullRemote());
  controller_info2->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info2->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  controller_info2->object_info = std::move(object_info2);
  controller_info2->remote_controller = remote_controller2.Unbind();

  // Resetting the controller will trigger many things happening, including the
  // object binding being broken.
  base::RunLoop drop_binding_loop;
  object_host1->RunOnConnectionError(drop_binding_loop.QuitClosure());
  container_remote->SetController(std::move(controller_info2), true);
  container_remote.FlushForTesting();
  drop_binding_loop.Run();
  EXPECT_EQ(0, object_host1->GetReceiverCount());

  // Subresource loader factory must be available, and should be the same
  // one as we got before.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      wrapped_loader_factory2 = provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, wrapped_loader_factory2);
  EXPECT_EQ(wrapped_loader_factory1, wrapped_loader_factory2);
  network::mojom::URLLoaderFactory* subresource_loader_factory2 =
      provider_context->GetSubresourceLoaderFactoryInternal();
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
  container_remote->SetController(
      blink::mojom::ControllerServiceWorkerInfo::New(), true);

  // The controller is reset. References to the old controller must be
  // released.
  container_remote.FlushForTesting();
  drop_binding_loop2.Run();
  EXPECT_EQ(0, object_host2->GetReceiverCount());

  // Subresource loader factory must not be available.
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactory());
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactoryInternal());

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
  ASSERT_EQ(0, object_host4->GetReceiverCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info4 =
      object_host4->CreateObjectInfo();
  EXPECT_EQ(1, object_host4->GetReceiverCount());
  FakeControllerServiceWorker fake_controller4;
  auto controller_info4 = blink::mojom::ControllerServiceWorkerInfo::New();
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller4;
  fake_controller4.Clone(remote_controller4.BindNewPipeAndPassReceiver(),
                         network::CrossOriginEmbedderPolicy(),
                         mojo::NullRemote());
  controller_info4->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info4->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  controller_info4->object_info = std::move(object_info4);
  controller_info4->remote_controller = remote_controller4.Unbind();
  container_remote->SetController(std::move(controller_info4), true);
  container_remote.FlushForTesting();

  // Subresource loader factory must be available.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      wrapped_loader_factory4 = provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, wrapped_loader_factory4);
  auto* subresource_loader_factory4 =
      provider_context->GetSubresourceLoaderFactoryInternal();
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
  EnableNetworkService();
  auto object_host =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);

  // Set a controller without ControllerServiceWorker ptr to emulate no
  // fetch event handler.
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      object_host->CreateObjectInfo();
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->mode =
      blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler;
  controller_info->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
  controller_info->object_info = std::move(object_info);

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver),
      mojo::NullAssociatedRemote() /* host_remote */,
      std::move(controller_info), loader_factory_);
  base::RunLoop().RunUntilIdle();

  // Subresource loader factory must not be available.
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactory());
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactoryInternal());
}

TEST_F(ServiceWorkerProviderContextTest, PostMessageToClient) {
  auto mock_service_worker_object_host =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
  ASSERT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      mock_service_worker_object_host->CreateObjectInfo();
  EXPECT_EQ(1, mock_service_worker_object_host->GetReceiverCount());

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());
  ASSERT_FALSE(client->was_receive_message_called());

  blink::TransferableMessage message;
  message.sender_agent_cluster_id = base::UnguessableToken::Create();
  container_remote->PostMessageToClient(std::move(object_info),
                                        std::move(message));
  base::RunLoop().RunUntilIdle();

  // The passed reference should be owned by the provider client (but the
  // reference is immediately released by the mock provider client).
  EXPECT_TRUE(client->was_receive_message_called());
  EXPECT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
}

TEST_F(ServiceWorkerProviderContextTest, CountFeature) {
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      nullptr /* controller_info */, nullptr /* loader_factory*/);
  auto provider_impl =
      std::make_unique<WebServiceWorkerProviderImpl>(provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();

  container_remote->CountFeature(blink::mojom::WebFeature::kWorkerStart);
  provider_impl->SetClient(client.get());
  base::RunLoop().RunUntilIdle();

  // Calling CountFeature() before client is set will save the feature usage in
  // the set, and once SetClient() is called it gets propagated to the client.
  ASSERT_EQ(1UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWorkerStart,
            *(client->used_features().begin()));

  container_remote->CountFeature(blink::mojom::WebFeature::kWindowEvent);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWindowEvent,
            *(++(client->used_features().begin())));
}

TEST_F(ServiceWorkerProviderContextTest, OnNetworkProviderDestroyed) {
  // Make the object host for .controller.
  auto object_host =
      std::make_unique<MockServiceWorkerObjectHost>(200 /* version_id */);
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      object_host->CreateObjectInfo();

  // Make the ControllerServiceWorkerInfo.
  FakeControllerServiceWorker fake_controller;
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller;
  fake_controller.Clone(remote_controller.BindNewPipeAndPassReceiver(),
                        network::CrossOriginEmbedderPolicy(),
                        mojo::NullRemote());
  controller_info->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  controller_info->object_info = std::move(object_info);
  controller_info->remote_controller = remote_controller.Unbind();

  // Make the container host and container pointers.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();

  // Make the provider context.
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      std::move(controller_info), loader_factory_);

  // Put it in the weird state to test.
  provider_context->OnNetworkProviderDestroyed();

  // Calling these in the weird state shouldn't crash.
  EXPECT_FALSE(provider_context->container_host());
  EXPECT_FALSE(provider_context->CloneRemoteContainerHost());
  provider_context->DispatchNetworkQuiet();
  provider_context->NotifyExecutionReady();
}

TEST_F(ServiceWorkerProviderContextTest,
       SubresourceLoaderFactoryUseableAfterContextDestructs) {
  EnableNetworkService();

  // Make the object host for .controller.
  auto mock_service_worker_object_host =
      std::make_unique<MockServiceWorkerObjectHost>(201 /* version_id */);
  ASSERT_EQ(0, mock_service_worker_object_host->GetReceiverCount());
  blink::mojom::ServiceWorkerObjectInfoPtr object_info =
      mock_service_worker_object_host->CreateObjectInfo();
  EXPECT_EQ(1, mock_service_worker_object_host->GetReceiverCount());

  // Make the ControllerServiceWorkerInfo.
  FakeControllerServiceWorker fake_controller;
  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller;
  fake_controller.Clone(remote_controller.BindNewPipeAndPassReceiver(),
                        network::CrossOriginEmbedderPolicy(),
                        mojo::NullRemote());
  controller_info->mode =
      blink::mojom::ControllerServiceWorkerMode::kControlled;
  controller_info->fetch_handler_type =
          blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
  controller_info->object_info = std::move(object_info);
  controller_info->remote_controller = remote_controller.Unbind();

  // Make the container host and container pointers.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost> host_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver = host_remote.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_remote;
  auto container_receiver =
      container_remote.BindNewEndpointAndPassDedicatedReceiver();

  // Make the ServiceWorkerProviderContext, passing it the controller,
  // container, and container host.
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      blink::mojom::ServiceWorkerContainerType::kForWindow,
      std::move(container_receiver), host_remote.Unbind(),
      std::move(controller_info), loader_factory_);

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      wrapped_loader_factory = provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, wrapped_loader_factory);

  // Clear our context and ensure that we don't crash when later trying to use
  // the factory.
  provider_context.reset();

  network::ResourceRequest request;
  request.url = GURL("https://www.example.com/random.js");
  request.destination = network::mojom::RequestDestination::kEmpty;
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient loader_client;
  wrapped_loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, request, loader_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

}  // namespace service_worker_provider_context_unittest
}  // namespace content
