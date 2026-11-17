// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "url/gurl.h"
#include "v8/include/v8-local-handle.h"

namespace content {

namespace {

class FakeWebServiceWorkerContextProxy
    : public blink::WebServiceWorkerContextProxy {
 public:
  FakeWebServiceWorkerContextProxy() = default;
  ~FakeWebServiceWorkerContextProxy() override = default;

  void SetOnCompleteCallback(base::OnceClosure callback) {
    on_complete_callback_ = std::move(callback);
  }
  void SetOnErrorCallback(base::OnceClosure callback) {
    on_error_callback_ = std::move(callback);
  }

  int complete_count() const { return complete_count_; }
  int error_count() const { return error_count_; }

  // blink::WebServiceWorkerContextProxy:
  void BindServiceWorker(
      blink::CrossVariantMojoReceiver<blink::mojom::ServiceWorkerInterfaceBase>
          receiver) override {}
  void BindControllerServiceWorker(
      blink::CrossVariantMojoReceiver<
          blink::mojom::ControllerServiceWorkerInterfaceBase> receiver)
      override {}
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse> response,
      mojo::ScopedDataPipeConsumerHandle data_pipe) override {}
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError> error) override {
    ++error_count_;
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
  }
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override {
    ++complete_count_;
    if (on_complete_callback_) {
      std::move(on_complete_callback_).Run();
    }
  }
  bool IsWindowInteractionAllowed() override { return false; }
  void PauseEvaluation() override {}
  void ResumeEvaluation() override {}
  void DeferPrepareForEvaluation() override {}
  void RunDeferredPrepareForEvaluation() override {}
  blink::mojom::ServiceWorkerFetchHandlerType FetchHandlerType() override {
    return blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
  }
  bool HasHidEventHandlers() override { return false; }
  bool HasUsbEventHandlers() override { return false; }
  void GetRemoteAssociatedInterface(
      const blink::WebString& name,
      mojo::ScopedInterfaceEndpointHandle handle) override {}
  blink::AssociatedInterfaceRegistry& GetAssociatedInterfaceRegistry()
      override {
    return associated_interface_registry_;
  }

 private:
  blink::AssociatedInterfaceRegistry associated_interface_registry_;
  base::OnceClosure on_complete_callback_;
  base::OnceClosure on_error_callback_;
  int complete_count_ = 0;
  int error_count_ = 0;
};

}  // namespace

class ServiceWorkerContextClientTest : public testing::Test {
 protected:
  void SetUp() override {
    old_renderer_client_ = SetRendererClientForTesting(&renderer_client_);
    worker_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

    owner_ = base::WrapUnique(new EmbeddedWorkerInstanceClientImpl(
        instance_client_remote_.BindNewPipeAndPassReceiver(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        /*cors_exempt_header_list=*/{}));

    auto subresource_loaders =
        std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    default_loader_factory_receiver_ =
        subresource_loaders->pending_default_factory()
            .InitWithNewPipeAndPassReceiver();

    auto start_timing = blink::mojom::EmbeddedWorkerStartTiming::New();
    const GURL kScope("https://example.com/");
    const GURL kScript("https://example.com/sw.js");
    mojo::PendingAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>
        instance_host_remote;
    instance_host_receiver_ =
        instance_host_remote.InitWithNewEndpointAndPassReceiver();

    context_client_ = std::make_unique<ServiceWorkerContextClient>(
        /*service_worker_version_id=*/1, kScope, kScript,
        /*is_starting_installed_worker=*/false, blink::RendererPreferences(),
        service_worker_remote_.BindNewPipeAndPassReceiver(),
        controller_remote_.BindNewPipeAndPassReceiver(),
        std::move(instance_host_remote),
        interface_provider_remote_.BindNewPipeAndPassReceiver(),
        blink::mojom::ServiceWorkerProviderInfoForStartWorker::New(),
        owner_.get(), std::move(start_timing),
        /*preference_watcher_receiver=*/mojo::NullReceiver(),
        std::move(subresource_loaders),
        /*subresource_loader_updater=*/mojo::NullReceiver(),
        /*script_url_to_skip_throttling=*/GURL(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        /*service_worker_route_id=*/1,
        /*cors_exempt_header_list=*/std::vector<std::string>(),
        blink::StorageKey(), blink::ServiceWorkerToken());
  }

  void TearDown() override {
    context_client_.reset();
    owner_.reset();
    SetRendererClientForTesting(old_renderer_client_);
  }

  void RunOnWorkerThread(base::OnceClosure closure) {
    base::RunLoop run_loop;
    worker_task_runner_->PostTask(FROM_HERE, std::move(closure));
    worker_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  bool HasContext() const { return context_client_->context_ != nullptr; }

  base::test::TaskEnvironment task_environment_;
  ContentRendererClient renderer_client_;
  raw_ptr<ContentRendererClient> old_renderer_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  mojo::Remote<blink::mojom::EmbeddedWorkerInstanceClient>
      instance_client_remote_;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_loader_factory_receiver_;
  mojo::Remote<blink::mojom::ServiceWorker> service_worker_remote_;
  mojo::Remote<blink::mojom::ControllerServiceWorker> controller_remote_;
  mojo::PendingAssociatedReceiver<blink::mojom::EmbeddedWorkerInstanceHost>
      instance_host_receiver_;
  mojo::Remote<service_manager::mojom::InterfaceProvider>
      interface_provider_remote_;

  std::unique_ptr<EmbeddedWorkerInstanceClientImpl> owner_;
  std::unique_ptr<ServiceWorkerContextClient> context_client_;
  FakeWebServiceWorkerContextProxy proxy_;
};

TEST_F(ServiceWorkerContextClientTest,
       OnNavigationPreloadCompleteDestroysWorkerContext) {
  RunOnWorkerThread(base::BindLambdaForTesting([&]() {
    context_client_->WorkerContextStarted(&proxy_, worker_task_runner_);
    ASSERT_TRUE(HasContext());

    mojo::Remote<network::mojom::URLLoaderClient> preload_loader_client;
    context_client_->SetupNavigationPreload(
        /*fetch_event_id=*/1, blink::WebURL(GURL("https://example.com/")),
        preload_loader_client.BindNewPipeAndPassReceiver());

    proxy_.SetOnCompleteCallback(base::BindLambdaForTesting([&]() {
      context_client_->WillDestroyWorkerContext(v8::Local<v8::Context>());
    }));

    context_client_->OnNavigationPreloadComplete(
        /*fetch_event_id=*/1, base::TimeTicks::Now(),
        /*encoded_data_length=*/0, /*encoded_body_length=*/0,
        /*decoded_body_length=*/0);

    EXPECT_EQ(1, proxy_.complete_count());
    EXPECT_FALSE(HasContext());
  }));
}

TEST_F(ServiceWorkerContextClientTest,
       OnNavigationPreloadErrorDestroysWorkerContext) {
  RunOnWorkerThread(base::BindLambdaForTesting([&]() {
    context_client_->WorkerContextStarted(&proxy_, worker_task_runner_);
    ASSERT_TRUE(HasContext());

    mojo::Remote<network::mojom::URLLoaderClient> preload_loader_client;
    context_client_->SetupNavigationPreload(
        /*fetch_event_id=*/1, blink::WebURL(GURL("https://example.com/")),
        preload_loader_client.BindNewPipeAndPassReceiver());

    proxy_.SetOnErrorCallback(base::BindLambdaForTesting([&]() {
      context_client_->WillDestroyWorkerContext(v8::Local<v8::Context>());
    }));

    context_client_->OnNavigationPreloadError(
        /*fetch_event_id=*/1,
        std::make_unique<blink::WebServiceWorkerError>(
            blink::mojom::ServiceWorkerErrorType::kNetwork,
            blink::WebString::FromAscii("error"),
            blink::WebServiceWorkerError::Mode::kNone));

    EXPECT_EQ(1, proxy_.error_count());
    EXPECT_FALSE(HasContext());
  }));
}

}  // namespace content
