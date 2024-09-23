// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_content_browser_client.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {
namespace service_worker_controllee_request_handler_unittest {

namespace {

class DeleteAndStartOverWaiter : public ServiceWorkerContextCoreObserver {
 public:
  explicit DeleteAndStartOverWaiter(
      ServiceWorkerContextWrapper& service_worker_context_wrapper)
      : service_worker_context_wrapper_(service_worker_context_wrapper) {
    service_worker_context_wrapper_->AddObserver(this);
  }
  void OnDeleteAndStartOver() override { run_loop_.Quit(); }
  void Wait() {
    run_loop_.Run();
    service_worker_context_wrapper_->RemoveObserver(this);
  }

 private:
  raw_ref<ServiceWorkerContextWrapper> service_worker_context_wrapper_;
  base::RunLoop run_loop_;
};

}  // namespace

class ServiceWorkerControlleeRequestHandlerTest : public testing::Test {
 public:
  class ServiceWorkerRequestTestResources {
   public:
    ServiceWorkerRequestTestResources(
        ServiceWorkerControlleeRequestHandlerTest* test,
        const GURL& url,
        network::mojom::RequestDestination destination,
        network::mojom::RequestMode request_mode =
            network::mojom::RequestMode::kNoCors)
        : destination_(destination),
          request_(test->url_request_context_->CreateRequest(
              url,
              net::DEFAULT_PRIORITY,
              &test->url_request_delegate_,
              TRAFFIC_ANNOTATION_FOR_TESTS)),
          handler_(std::make_unique<ServiceWorkerControlleeRequestHandler>(
              test->context()->AsWeakPtr(),
              test->service_worker_client_,
              destination,
              /*skip_service_worker=*/false,
              FrameTreeNodeId(),
              base::DoNothing())) {}

    void MaybeCreateLoader() {
      network::ResourceRequest resource_request;
      resource_request.url = request_->url();
      resource_request.destination = destination_;
      resource_request.headers = request()->extra_request_headers();
      DCHECK(!loader_loop_.AnyQuitCalled());
      handler_->MaybeCreateLoader(
          resource_request,
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(resource_request.url)),
          nullptr,
          base::BindOnce(
              [](base::OnceClosure closure,
                 std::optional<NavigationLoaderInterceptor::Result>
                     interceptor_result) { std::move(closure).Run(); },
              loader_loop_.QuitClosure()),
          base::DoNothing());
    }

    void WaitLoader() { loader_loop_.Run(); }

    ServiceWorkerMainResourceLoader* loader() { return handler_->loader(); }

    void SetHandler(
        std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler) {
      handler_ = std::move(handler);
    }

    void ResetHandler() { handler_.reset(nullptr); }

    net::URLRequest* request() const { return request_.get(); }

   private:
    const network::mojom::RequestDestination destination_;
    std::unique_ptr<net::URLRequest> request_;
    std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler_;
    base::RunLoop loader_loop_;
  };

  // A fake instance client for toggling whether a fetch event handler exists.
  class FetchHandlerInstanceClient : public FakeEmbeddedWorkerInstanceClient {
   public:
    explicit FetchHandlerInstanceClient(EmbeddedWorkerTestHelper* helper)
        : FakeEmbeddedWorkerInstanceClient(helper) {}
    ~FetchHandlerInstanceClient() override = default;

    void set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType fetch_handler_type) {
      fetch_handler_type_ = fetch_handler_type;
    }

   protected:
    void EvaluateScript() override {
      host()->OnScriptEvaluationStart();
      host()->OnStarted(
          blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
          fetch_handler_type_, /*has_hid_event_handlers=*/false,
          /*has_usb_event_handlers=*/false, helper()->GetNextThreadId(),
          blink::mojom::EmbeddedWorkerStartTiming::New());
    }

   private:
    ServiceWorkerVersion::FetchHandlerType fetch_handler_type_ =
        ServiceWorkerVersion::FetchHandlerType::kNoHandler;
  };

  ServiceWorkerControlleeRequestHandlerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        url_request_context_(
            net::CreateTestURLRequestContextBuilder()->Build()) {}

  void SetUp() override { SetUpWithHelper(/*is_parent_frame_secure=*/true); }

  void SetUpWithHelper(bool is_parent_frame_secure) {
    SetUpWithHelperAndFetchHandlerType(
        is_parent_frame_secure,
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  }

  void SetUpWithHelperAndFetchHandlerType(
      bool is_parent_frame_secure,
      ServiceWorkerVersion::FetchHandlerType fetch_handler_type) {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    auto* fetch_handler_worker =
        helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
            helper_.get());
    fetch_handler_worker->set_fetch_handler_type(fetch_handler_type);

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = ServiceWorkerRegistration::Create(
        options,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_)), 1L,
        context()->AsWeakPtr(), blink::mojom::AncestorFrameType::kNormalFrame);
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, blink::mojom::ScriptType::kClassic,
        1L, mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
        context()->AsWeakPtr());
    version_->set_policy_container_host(
        base::MakeRefCounted<PolicyContainerHost>(PolicyContainerPolicies()));

    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    records.push_back(WriteToDiskCacheSync(
        context()->GetStorageControl(), version_->script_url(),
        {} /* headers */, "I'm a body", "I'm a meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptResponse(
        EmbeddedWorkerTestHelper::CreateMainScriptResponse());

    // An empty host.
    ScopedServiceWorkerClient service_worker_client =
        helper_->context()
            ->service_worker_client_owner()
            .CreateServiceWorkerClientForWindow(is_parent_frame_secure,
                                                FrameTreeNodeId(1));
    service_worker_client_ = service_worker_client.AsWeakPtr();
    service_worker_clients_.push_back(std::move(service_worker_client));
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  void CloseRemotes() { service_worker_clients_.clear(); }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerClient> service_worker_client_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  net::TestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
  std::vector<ScopedServiceWorkerClient> service_worker_clients_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() = default;
  AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override {
    return AllowServiceWorkerResult::No();
  }
};

TEST_F(ServiceWorkerControlleeRequestHandlerTest, Basic) {
  // Prepare a valid version and registration.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  {
    base::RunLoop loop;
    context()->registry()->StoreRegistration(
        registration_.get(), version_.get(),
        base::BindOnce(
            [](base::OnceClosure closure,
               blink::ServiceWorkerStatusCode status) {
              ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
              std::move(closure).Run();
            },
            loop.QuitClosure()));
    loop.Run();
  }

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  test_resources.WaitLoader();

  EXPECT_TRUE(test_resources.loader());
  EXPECT_TRUE(version_->HasControllee());

  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DoesNotExist) {
  // No version and registration exists in the scope.

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  test_resources.WaitLoader();

  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, Error) {
  // Disabling the storage makes looking up the registration return an error.
  context()->registry()->DisableStorageForTesting(base::DoNothing());

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  test_resources.WaitLoader();

  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DisallowServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  // Store an activated worker.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  test_resources.WaitLoader();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, InsecureContext) {
  // Reset the provider host as insecure.
  SetUpWithHelper(/*is_parent_frame_secure=*/false);

  // Store an activated worker.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  test_resources.WaitLoader();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, ActivateWaitingVersion) {
  // Store a registration that is installed but not activated yet.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration_->SetWaitingVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  test_resources.WaitLoader();

  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version_->status());
  EXPECT_TRUE(test_resources.loader());
  EXPECT_TRUE(version_->HasControllee());

  test_resources.ResetHandler();
}

// Test that an installing registration is associated with a provider host.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, InstallingRegistration) {
  // Create an installing registration.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  registration_->SetInstallingVersion(version_);
  context()->registry()->NotifyInstallingRegistration(registration_.get());

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();

  test_resources.WaitLoader();

  // The handler should have fallen back to network and destroyed the job. The
  // provider host should not be controlled. However it should add the
  // registration as a matching registration so it can be used for .ready and
  // claim().
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(service_worker_client_->controller());
  EXPECT_EQ(registration_.get(), service_worker_client_->MatchRegistration());
}

// Test to not regress crbug/414118.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, DeletedContainerHost) {
  // Store a registration so the call to FindRegistrationForDocument will read
  // from the database.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();
  version_ = nullptr;
  registration_ = nullptr;

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the database lookup.
  CloseRemotes();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service_worker_client_);
  EXPECT_FALSE(test_resources.loader());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, SkipServiceWorker) {
  // Store an activated worker.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Create an interceptor that skips service workers.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.SetHandler(
      std::make_unique<ServiceWorkerControlleeRequestHandler>(
          context()->AsWeakPtr(), service_worker_client_,
          network::mojom::RequestDestination::kDocument,
          /*skip_service_worker=*/true, FrameTreeNodeId(), base::DoNothing()));

  // Conduct a main resource load.
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  test_resources.WaitLoader();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  // The host should still have the correct URL.
  EXPECT_EQ(GURL("https://host/scope/doc"), service_worker_client_->url());
}

// Tests interception after the context core has been destroyed and the provider
// host is transferred to a new context.
// TODO(crbug.com/41409843): Remove this test when transferring contexts is
// removed.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, NullContext) {
  // Store an activated worker.
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Create an interceptor.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  test_resources.SetHandler(
      std::make_unique<ServiceWorkerControlleeRequestHandler>(
          context()->AsWeakPtr(), service_worker_client_,
          network::mojom::RequestDestination::kDocument,
          /*skip_service_worker=*/false, FrameTreeNodeId(), base::DoNothing()));

  // Destroy the context and make a new one.
  DeleteAndStartOverWaiter delete_and_start_over_waiter(
      *helper_->context_wrapper());
  helper_->context_wrapper()->DeleteAndStartOver();
  delete_and_start_over_waiter.Wait();

  // Conduct a main resource load. The loader won't be created because the
  // interceptor's context is now null.
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  // Since the interceptor's context is now null, `test_resources.WaitLoader()`
  // won't finish.  Use RunUntilIdle() instead.
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  // The host should still have the correct URL.
  EXPECT_EQ(GURL("https://host/scope/doc"), service_worker_client_->url());
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithOfflineHeader) {
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();
  version_.reset();
  registration_.reset();

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  // Sets an offline header to indicate force loading offline page.
  test_resources.request()->SetExtraRequestHeaderByName(
      "X-Chrome-offline", "reason=download", true);
  test_resources.MaybeCreateLoader();
  test_resources.WaitLoader();
  EXPECT_FALSE(test_resources.loader());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithNoOfflineHeader) {
  version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->registry()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();
  version_.reset();
  registration_.reset();

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kDocument);
  // Empty offline header value should not cause fallback.
  test_resources.request()->SetExtraRequestHeaderByName("X-Chrome-offline", "",
                                                        true);
  test_resources.MaybeCreateLoader();
  test_resources.WaitLoader();
  EXPECT_TRUE(test_resources.loader());
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGE)

}  // namespace service_worker_controllee_request_handler_unittest
}  // namespace content
