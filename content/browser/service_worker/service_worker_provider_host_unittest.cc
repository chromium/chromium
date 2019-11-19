// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/url_schemes.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

const char kServiceWorkerScheme[] = "i-can-use-service-worker";

class ServiceWorkerTestContentClient : public TestContentClient {
 public:
  void AddAdditionalSchemes(Schemes* schemes) override {
    schemes->service_worker_schemes.push_back(kServiceWorkerScheme);
  }
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  struct AllowServiceWorkerCallLog {
    AllowServiceWorkerCallLog(
        const GURL& scope,
        const GURL& site_for_cookies,
        const base::Optional<url::Origin>& top_frame_origin,
        const GURL& script_url)
        : scope(scope),
          site_for_cookies(site_for_cookies),
          top_frame_origin(top_frame_origin),
          script_url(script_url) {}
    const GURL scope;
    const GURL site_for_cookies;
    const base::Optional<url::Origin> top_frame_origin;
    const GURL script_url;
  };

  ServiceWorkerTestContentBrowserClient() {}

  bool AllowServiceWorkerOnIO(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::ResourceContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    logs_.emplace_back(scope, site_for_cookies, top_frame_origin, script_url);
    return false;
  }

  bool AllowServiceWorkerOnUI(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    logs_.emplace_back(scope, site_for_cookies, top_frame_origin, script_url);
    return false;
  }

  const std::vector<AllowServiceWorkerCallLog>& logs() const { return logs_; }

 private:
  std::vector<AllowServiceWorkerCallLog> logs_;
};

}  // namespace

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {
    SetContentClient(&test_content_client_);
  }
  ~ServiceWorkerProviderHostTest() override {}

  void SetUp() override {
    old_content_browser_client_ =
        SetBrowserClientForTesting(&test_content_browser_client_);
    ResetSchemesAndOriginsWhitelist();
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &ServiceWorkerProviderHostTest::OnMojoError, base::Unretained(this)));

    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    context_ = helper_->context();
    script_url_ = GURL("https://www.example.com/service_worker.js");

    blink::mojom::ServiceWorkerRegistrationOptions options1;
    options1.scope = GURL("https://www.example.com/");
    registration1_ =
        new ServiceWorkerRegistration(options1, 1L, context_->AsWeakPtr());

    blink::mojom::ServiceWorkerRegistrationOptions options2;
    options2.scope = GURL("https://www.example.com/example");
    registration2_ =
        new ServiceWorkerRegistration(options2, 2L, context_->AsWeakPtr());

    blink::mojom::ServiceWorkerRegistrationOptions options3;
    options3.scope = GURL("https://other.example.com/");
    registration3_ =
        new ServiceWorkerRegistration(options3, 3L, context_->AsWeakPtr());
  }

  void TearDown() override {
    registration1_ = nullptr;
    registration2_ = nullptr;
    registration3_ = nullptr;
    helper_.reset();
    SetBrowserClientForTesting(old_content_browser_client_);
    // Reset cached security schemes so we don't affect other tests.
    ResetSchemesAndOriginsWhitelist();
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  ServiceWorkerRemoteProviderEndpoint PrepareServiceWorkerProviderHost(
      const GURL& document_url) {
    ServiceWorkerRemoteProviderEndpoint remote_endpoint;
    GURL site_for_cookies = document_url;
    url::Origin top_frame_origin = url::Origin::Create(document_url);
    CreateProviderHostInternal(document_url, site_for_cookies, top_frame_origin,
                               &remote_endpoint);
    return remote_endpoint;
  }

  ServiceWorkerRemoteProviderEndpoint
  PrepareServiceWorkerProviderHostWithSiteForCookies(
      const GURL& document_url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) {
    ServiceWorkerRemoteProviderEndpoint remote_endpoint;
    CreateProviderHostInternal(document_url, site_for_cookies, top_frame_origin,
                               &remote_endpoint);
    return remote_endpoint;
  }

  ServiceWorkerProviderHost* CreateProviderHost(const GURL& document_url) {
    GURL site_for_cookies = document_url;
    url::Origin top_frame_origin = url::Origin::Create(document_url);
    remote_endpoints_.emplace_back();
    return CreateProviderHostInternal(document_url, site_for_cookies,
                                      top_frame_origin,
                                      &remote_endpoints_.back());
  }

  ServiceWorkerProviderHost* CreateProviderHostWithInsecureParentFrame(
      const GURL& document_url) {
    remote_endpoints_.emplace_back();
    base::WeakPtr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), false /* is_parent_frame_secure */,
        helper_->context()->AsWeakPtr(), &remote_endpoints_.back());
    ServiceWorkerProviderHost* host_raw = host.get();
    host->UpdateUrls(document_url, document_url,
                     url::Origin::Create(document_url));
    return host_raw;
  }

  void FinishNavigation(ServiceWorkerProviderHost* host) {
    // In production code, the loader/request handler does this.
    const GURL url("https://www.example.com/page");
    host->UpdateUrls(url, url, url::Origin::Create(url));

    // In production code this is called from NavigationRequest in the browser
    // process right before navigation commit.
    host->OnBeginNavigationCommit(
        helper_->mock_render_process_id(), 1 /* route_id */,
        network::mojom::CrossOriginEmbedderPolicy::kNone);
  }

  blink::mojom::ServiceWorkerErrorType Register(
      blink::mojom::ServiceWorkerContainerHost* container_host,
      GURL scope,
      GURL worker_url) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    auto options = blink::mojom::ServiceWorkerRegistrationOptions::New();
    options->scope = scope;
    container_host->Register(
        worker_url, std::move(options),
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindOnce([](blink::mojom::ServiceWorkerErrorType* out_error,
                          blink::mojom::ServiceWorkerErrorType error,
                          const base::Optional<std::string>& error_msg,
                          blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                              registration) { *out_error = error; },
                       &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  blink::mojom::ServiceWorkerErrorType GetRegistration(
      blink::mojom::ServiceWorkerContainerHost* container_host,
      GURL document_url,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* out_info =
          nullptr) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    container_host->GetRegistration(
        document_url,
        base::BindOnce(
            [](blink::mojom::ServiceWorkerErrorType* out_error,
               blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* out_info,
               blink::mojom::ServiceWorkerErrorType error,
               const base::Optional<std::string>& error_msg,
               blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                   registration) {
              *out_error = error;
              if (out_info)
                *out_info = std::move(registration);
            },
            &error, out_info));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  blink::mojom::ServiceWorkerErrorType GetRegistrations(
      blink::mojom::ServiceWorkerContainerHost* container_host) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    container_host->GetRegistrations(base::BindOnce(
        [](blink::mojom::ServiceWorkerErrorType* out_error,
           blink::mojom::ServiceWorkerErrorType error,
           const base::Optional<std::string>& error_msg,
           base::Optional<std::vector<
               blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>> infos) {
          *out_error = error;
        },
        &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  bool CanFindClientProviderHost(ServiceWorkerProviderHost* host) {
    for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
             context_->GetClientProviderHostIterator(
                 host->url().GetOrigin(), false /* include_reserved_clients */);
         !it->IsAtEnd(); it->Advance()) {
      if (host == it->GetProviderHost())
        return true;
    }
    return false;
  }

  void ExpectUpdateIsScheduled(ServiceWorkerVersion* version) {
    EXPECT_TRUE(version->is_update_scheduled_);
    EXPECT_TRUE(version->update_timer_.IsRunning());
  }

  void ExpectUpdateIsNotScheduled(ServiceWorkerVersion* version) {
    EXPECT_FALSE(version->is_update_scheduled_);
    EXPECT_FALSE(version->update_timer_.IsRunning());
  }

  bool HasVersionToUpdate(ServiceWorkerProviderHost* host) {
    return !host->versions_to_update_.empty();
  }

  void TestReservedClientsAreNotExposed(
      blink::mojom::ServiceWorkerProviderType provider_type,
      const GURL url);
  void TestClientPhaseTransition(
      blink::mojom::ServiceWorkerProviderType provider_type,
      const GURL url);

  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  ServiceWorkerContextCore* context_;
  scoped_refptr<ServiceWorkerRegistration> registration1_;
  scoped_refptr<ServiceWorkerRegistration> registration2_;
  scoped_refptr<ServiceWorkerRegistration> registration3_;
  GURL script_url_;
  ServiceWorkerTestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
  ContentBrowserClient* old_content_browser_client_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
  std::vector<std::string> bad_messages_;

 private:
  ServiceWorkerProviderHost* CreateProviderHostInternal(
      const GURL& document_url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      ServiceWorkerRemoteProviderEndpoint* remote_endpoint) {
    base::WeakPtr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        helper_->context()->AsWeakPtr(), remote_endpoint);
    host->UpdateUrls(document_url, site_for_cookies, top_frame_origin);
    return host.get();
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

// Run tests with PlzDedicatedWorker.
// TODO(https://crbug.com/906991): Merge this test fixture into
// ServiceWorkerProviderHostTest once PlzDedicatedWorker is enabled by default.
class ServiceWorkerProviderHostTestWithPlzDedicatedWorker
    : public ServiceWorkerProviderHostTest {
 public:
  ServiceWorkerProviderHostTestWithPlzDedicatedWorker() {
    // ServiceWorkerProviderHost for dedicated workers is available only when
    // PlzDedicatedWorker is enabled.
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPlzDedicatedWorker);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ServiceWorkerProviderHostTest, MatchRegistration) {
  ServiceWorkerProviderHost* provider_host1 =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));

  // Match registration should return the longest matching one.
  ASSERT_EQ(registration2_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, provider_host1->MatchRegistration());

  // Should return nullptr after removing all matching registrations.
  provider_host1->RemoveMatchingRegistration(registration1_.get());
  ASSERT_EQ(nullptr, provider_host1->MatchRegistration());

  // SetDocumentUrl sets all of matching registrations
  provider_host1->UpdateUrls(
      GURL("https://www.example.com/example1"),
      GURL("https://www.example.com/example1"),
      url::Origin::Create(GURL("https://www.example.com/example1")));
  ASSERT_EQ(registration2_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, provider_host1->MatchRegistration());

  // SetDocumentUrl with another origin also updates matching registrations
  provider_host1->UpdateUrls(
      GURL("https://other.example.com/example"),
      GURL("https://other.example.com/example"),
      url::Origin::Create(GURL("https://other.example.com/example")));
  ASSERT_EQ(registration3_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration3_.get());
  ASSERT_EQ(nullptr, provider_host1->MatchRegistration());
}

TEST_F(ServiceWorkerProviderHostTest, ContextSecurity) {
  ServiceWorkerProviderHost* provider_host_secure_parent =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));
  ServiceWorkerProviderHost* provider_host_insecure_parent =
      CreateProviderHostWithInsecureParentFrame(
          GURL("https://www.example.com/example1.html"));

  // Insecure document URL.
  provider_host_secure_parent->UpdateUrls(
      GURL("http://host"), GURL("http://host"),
      url::Origin::Create(GURL("http://host")));
  EXPECT_FALSE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Insecure parent frame.
  provider_host_insecure_parent->UpdateUrls(
      GURL("https://host"), GURL("https://host"),
      url::Origin::Create(GURL("https://host")));
  EXPECT_FALSE(
      provider_host_insecure_parent->IsContextSecureForServiceWorker());

  // Secure URL and parent frame.
  provider_host_secure_parent->UpdateUrls(
      GURL("https://host"), GURL("https://host"),
      url::Origin::Create(GURL("https://host")));
  EXPECT_TRUE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Exceptional service worker scheme.
  GURL url(std::string(kServiceWorkerScheme) + "://host");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(url.is_valid());
  EXPECT_FALSE(IsOriginSecure(url));
  EXPECT_TRUE(OriginCanAccessServiceWorkers(url));
  provider_host_secure_parent->UpdateUrls(url, url, origin);
  EXPECT_TRUE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Exceptional service worker scheme with insecure parent frame.
  provider_host_insecure_parent->UpdateUrls(url, url, origin);
  EXPECT_FALSE(
      provider_host_insecure_parent->IsContextSecureForServiceWorker());
}

TEST_F(ServiceWorkerProviderHostTest, UpdateUrls_SameOriginRedirect) {
  const GURL url1("https://origin1.example.com/page1.html");
  const GURL url2("https://origin1.example.com/page2.html");

  ServiceWorkerProviderHost* host = CreateProviderHost(url1);
  const std::string uuid1 = host->client_uuid();
  EXPECT_EQ(url1, host->url());
  EXPECT_EQ(url1, host->site_for_cookies());

  host->UpdateUrls(url2, url2, url::Origin::Create(url2));
  EXPECT_EQ(url2, host->url());
  EXPECT_EQ(url2, host->site_for_cookies());
  EXPECT_EQ(uuid1, host->client_uuid());

  EXPECT_EQ(host, context_->GetProviderHostByClientID(host->client_uuid()));
}

TEST_F(ServiceWorkerProviderHostTest, UpdateUrls_CrossOriginRedirect) {
  const GURL url1("https://origin1.example.com/page1.html");
  const GURL url2("https://origin2.example.com/page2.html");

  ServiceWorkerProviderHost* host = CreateProviderHost(url1);
  const std::string uuid1 = host->client_uuid();
  EXPECT_EQ(url1, host->url());
  EXPECT_EQ(url1, host->site_for_cookies());

  host->UpdateUrls(url2, url2, url::Origin::Create(url2));
  EXPECT_EQ(url2, host->url());
  EXPECT_EQ(url2, host->site_for_cookies());
  EXPECT_NE(uuid1, host->client_uuid());

  EXPECT_FALSE(context_->GetProviderHostByClientID(uuid1));
  EXPECT_EQ(host, context_->GetProviderHostByClientID(host->client_uuid()));
}

class MockServiceWorkerRegistration : public ServiceWorkerRegistration {
 public:
  MockServiceWorkerRegistration(
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      int64_t registration_id,
      base::WeakPtr<ServiceWorkerContextCore> context)
      : ServiceWorkerRegistration(options, registration_id, context) {}

  void AddListener(ServiceWorkerRegistration::Listener* listener) override {
    listeners_.insert(listener);
  }

  void RemoveListener(ServiceWorkerRegistration::Listener* listener) override {
    listeners_.erase(listener);
  }

  const std::set<ServiceWorkerRegistration::Listener*>& listeners() {
    return listeners_;
  }

 protected:
  ~MockServiceWorkerRegistration() override {}

 private:
  std::set<ServiceWorkerRegistration::Listener*> listeners_;
};

TEST_F(ServiceWorkerProviderHostTest, RemoveProvider) {
  // Create a provider host connected with the renderer process.
  ServiceWorkerProviderHost* provider_host =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));
  int provider_id = provider_host->provider_id();
  EXPECT_TRUE(context_->GetProviderHost(provider_id));

  // Disconnect the mojo pipe from the renderer side.
  ASSERT_TRUE(remote_endpoints_.back().host_remote()->is_bound());
  remote_endpoints_.back().host_remote()->reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->GetProviderHost(provider_id));
}

class MockServiceWorkerContainer : public blink::mojom::ServiceWorkerContainer {
 public:
  explicit MockServiceWorkerContainer(
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainer>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  ~MockServiceWorkerContainer() override = default;

  void SetController(
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      bool should_notify_controllerchange) override {
    was_set_controller_called_ = true;
  }
  void PostMessageToClient(blink::mojom::ServiceWorkerObjectInfoPtr controller,
                           blink::TransferableMessage message) override {}
  void CountFeature(blink::mojom::WebFeature feature) override {}

  bool was_set_controller_called() const { return was_set_controller_called_; }

 private:
  bool was_set_controller_called_ = false;
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainer> receiver_;
};

TEST_F(ServiceWorkerProviderHostTest, Controller) {
  // Create a host.
  std::unique_ptr<ServiceWorkerProviderHostAndInfo> host_and_info =
      CreateProviderHostAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                         /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerProviderHost> host =
      std::move(host_and_info->host);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindForWindow(std::move(host_and_info->info));
  auto container = std::make_unique<MockServiceWorkerContainer>(
      std::move(*remote_endpoints_.back().client_receiver()));

  // Create an active version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version);

  // Finish the navigation.
  FinishNavigation(host.get());
  host->SetControllerRegistration(registration1_,
                                  false /* notify_controllerchange */);
  remote_endpoints_.back().host_remote()->get()->OnExecutionReady();
  base::RunLoop().RunUntilIdle();

  // The page should be controlled since there was an active version at the
  // time navigation started. The SetController IPC should have been sent.
  EXPECT_TRUE(host->controller());
  EXPECT_TRUE(container->was_set_controller_called());
  EXPECT_EQ(registration1_.get(), host->MatchRegistration());
}

TEST_F(ServiceWorkerProviderHostTest, UncontrolledWithMatchingRegistration) {
  // Create a host.
  std::unique_ptr<ServiceWorkerProviderHostAndInfo> host_and_info =
      CreateProviderHostAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                         /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerProviderHost> host =
      std::move(host_and_info->host);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindForWindow(std::move(host_and_info->info));
  auto container = std::make_unique<MockServiceWorkerContainer>(
      std::move(*remote_endpoints_.back().client_receiver()));

  // Create an installing version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  registration1_->SetInstallingVersion(version);

  // Finish the navigation.
  FinishNavigation(host.get());
  // Promote the worker to active while navigation is still happening.
  registration1_->SetActiveVersion(version);
  base::RunLoop().RunUntilIdle();

  // The page should not be controlled since there was no active version at the
  // time navigation started. Furthermore, no SetController IPC should have been
  // sent.
  EXPECT_FALSE(host->controller());
  EXPECT_FALSE(container->was_set_controller_called());
  // However, the host should know the registration is its best match, for
  // .ready and claim().
  EXPECT_EQ(registration1_.get(), host->MatchRegistration());
}

TEST_F(ServiceWorkerProviderHostTest,
       Register_ContentSettingsDisallowsServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHostWithSiteForCookies(
          GURL("https://www.example.com/foo"),
          GURL("https://www.example.com/top"),
          url::Origin::Create(GURL("https://www.example.com")));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("https://www.example.com/scope"),
                     GURL("https://www.example.com/bar")));
  ASSERT_EQ(1ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/scope"),
            test_browser_client.logs()[0].scope);
  EXPECT_EQ(GURL("https://www.example.com/top"),
            test_browser_client.logs()[0].site_for_cookies);
  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            test_browser_client.logs()[0].top_frame_origin);
  EXPECT_EQ(GURL("https://www.example.com/bar"),
            test_browser_client.logs()[0].script_url);

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            GetRegistration(remote_endpoint.host_remote()->get(),
                            GURL("https://www.example.com/")));
  ASSERT_EQ(2ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/foo"),
            test_browser_client.logs()[1].scope);
  EXPECT_EQ(GURL("https://www.example.com/top"),
            test_browser_client.logs()[1].site_for_cookies);
  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            test_browser_client.logs()[1].top_frame_origin);
  EXPECT_EQ(GURL(), test_browser_client.logs()[1].script_url);

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            GetRegistrations(remote_endpoint.host_remote()->get()));
  ASSERT_EQ(3ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/foo"),
            test_browser_client.logs()[2].scope);
  EXPECT_EQ(GURL("https://www.example.com/top"),
            test_browser_client.logs()[2].site_for_cookies);
  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            *test_browser_client.logs()[2].top_frame_origin);
  EXPECT_EQ(GURL(), test_browser_client.logs()[2].script_url);

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerProviderHostTest, AllowsServiceWorker) {
  // Create an active version.
  scoped_refptr<ServiceWorkerVersion> version =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration1_.get(), GURL("https://www.example.com/sw.js"),
          blink::mojom::ScriptType::kClassic, 1 /* version_id */,
          helper_->context()->AsWeakPtr());
  registration1_->SetActiveVersion(version);

  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerProviderHost> host =
      CreateProviderHostForServiceWorkerContext(
          helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
          version.get(), helper_->context()->AsWeakPtr(), &remote_endpoint);

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  EXPECT_FALSE(
      host->AllowServiceWorker(GURL("https://www.example.com/scope"), GURL()));

  ASSERT_EQ(1ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/scope"),
            test_browser_client.logs()[0].scope);
  EXPECT_EQ(GURL("https://www.example.com/sw.js"),
            test_browser_client.logs()[0].site_for_cookies);
  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            test_browser_client.logs()[0].top_frame_origin);
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerProviderHostTest, Register_HTTPS) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("https://www.example.com/"),
                     GURL("https://www.example.com/bar")));
}

TEST_F(ServiceWorkerProviderHostTest, Register_NonSecureTransportLocalhost) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("http://127.0.0.3:81/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("http://127.0.0.3:81/bar"),
                     GURL("http://127.0.0.3:81/baz")));
}

TEST_F(ServiceWorkerProviderHostTest, Register_InvalidScopeShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(), GURL(""),
           GURL("https://www.example.com/bar/hoge.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, Register_InvalidScriptShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/bar/"), GURL(""));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, Register_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("http://www.example.com/"), GURL("http://www.example.com/bar"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, Register_CrossOriginShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  // Script has a different host
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/"),
           GURL("https://foo.example.com/bar"));
  EXPECT_EQ(1u, bad_messages_.size());

  // Scope has a different host
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://foo.example.com/"),
           GURL("https://www.example.com/bar"));
  EXPECT_EQ(2u, bad_messages_.size());

  // Script has a different port
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/"),
           GURL("https://www.example.com:8080/bar"));
  EXPECT_EQ(3u, bad_messages_.size());

  // Scope has a different transport
  Register(remote_endpoint.host_remote()->get(), GURL("wss://www.example.com/"),
           GURL("https://www.example.com/bar"));
  EXPECT_EQ(4u, bad_messages_.size());

  // Script and scope have a different host but match each other
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://foo.example.com/"),
           GURL("https://foo.example.com/bar"));
  EXPECT_EQ(5u, bad_messages_.size());

  // Script and scope URLs are invalid
  Register(remote_endpoint.host_remote()->get(), GURL(), GURL("h@ttps://@"));
  EXPECT_EQ(6u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, Register_BadCharactersShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/%2f"),
           GURL("https://www.example.com/"));
  EXPECT_EQ(1u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/%2F"),
           GURL("https://www.example.com/"));
  EXPECT_EQ(2u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/%2f"));
  EXPECT_EQ(3u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/%5c"),
           GURL("https://www.example.com/"));
  EXPECT_EQ(4u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/%5c"));
  EXPECT_EQ(5u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/%5C"));
  EXPECT_EQ(6u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, Register_FileSystemDocumentShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(
          GURL("filesystem:https://www.example.com/temporary/a"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("filesystem:https://www.example.com/temporary/"),
           GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/temporary/"),
           GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("filesystem:https://www.example.com/temporary/"),
           GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest,
       Register_FileSystemScriptOrScopeShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(
          GURL("https://www.example.com/temporary/"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("filesystem:https://www.example.com/temporary/"),
           GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/temporary/"),
           GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2u, bad_messages_.size());

  Register(remote_endpoint.host_remote()->get(),
           GURL("filesystem:https://www.example.com/temporary/"),
           GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, EarlyContextDeletion) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  // Because ServiceWorkerContextCore owns ServiceWorkerProviderHost, our
  // ServiceWorkerProviderHost instance has destroyed.
  EXPECT_FALSE(remote_endpoint.host_remote()->is_connected());
}

TEST_F(ServiceWorkerProviderHostTest, GetRegistration_Success) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  const GURL kScope("https://www.example.com/");
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            Register(remote_endpoint.host_remote()->get(), kScope,
                     GURL("https://www.example.com/sw.js")));
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  EXPECT_EQ(
      blink::mojom::ServiceWorkerErrorType::kNone,
      GetRegistration(remote_endpoint.host_remote()->get(), kScope, &info));
  ASSERT_TRUE(info);
  EXPECT_EQ(kScope, info->scope);
}

TEST_F(ServiceWorkerProviderHostTest,
       GetRegistration_NotFoundShouldReturnNull) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            GetRegistration(remote_endpoint.host_remote()->get(),
                            GURL("https://www.example.com/"), &info));
  EXPECT_FALSE(info);
}

TEST_F(ServiceWorkerProviderHostTest, GetRegistration_CrossOriginShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(),
                  GURL("https://foo.example.com/"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, GetRegistration_InvalidScopeShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(), GURL(""));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest,
       GetRegistration_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(),
                  GURL("http://www.example.com/"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerProviderHostTest, GetRegistrations_SecureOrigin) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("https://www.example.com/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            GetRegistrations(remote_endpoint.host_remote()->get()));
}

TEST_F(ServiceWorkerProviderHostTest,
       GetRegistrations_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareServiceWorkerProviderHost(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistrations(remote_endpoint.host_remote()->get());
  EXPECT_EQ(1u, bad_messages_.size());
}

// Test that a "reserved" (i.e., not execution ready) client is not included
// when iterating over client provider hosts. If it were, it'd be undesirably
// exposed via the Clients API.
void ServiceWorkerProviderHostTest::TestReservedClientsAreNotExposed(
    blink::mojom::ServiceWorkerProviderType provider_type,
    const GURL url) {
  {
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver;
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    provider_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    host_receiver =
        provider_info->host_remote.InitWithNewEndpointAndPassReceiver();
    base::WeakPtr<ServiceWorkerProviderHost> host =
        ServiceWorkerProviderHost::CreateForWebWorker(
            context_->AsWeakPtr(), helper_->mock_render_process_id(),
            provider_type, std::move(host_receiver), std::move(client_remote));
    host->UpdateUrls(url, url, url::Origin::Create(url));
    EXPECT_FALSE(CanFindClientProviderHost(host.get()));
    host->CompleteWebWorkerPreparation(
        network::mojom::CrossOriginEmbedderPolicy::kNone);
    EXPECT_TRUE(CanFindClientProviderHost(host.get()));
  }

  {
    std::unique_ptr<ServiceWorkerProviderHostAndInfo> host_and_info =
        CreateProviderHostAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                           /*are_ancestors_secure=*/true);
    base::WeakPtr<ServiceWorkerProviderHost> host =
        std::move(host_and_info->host);
    ServiceWorkerRemoteProviderEndpoint remote_endpoint;
    remote_endpoint.BindForWindow(std::move(host_and_info->info));

    FinishNavigation(host.get());
    EXPECT_FALSE(CanFindClientProviderHost(host.get()));

    base::RunLoop run_loop;
    host->AddExecutionReadyCallback(run_loop.QuitClosure());
    remote_endpoint.host_remote()->get()->OnExecutionReady();
    run_loop.Run();
    EXPECT_TRUE(CanFindClientProviderHost(host.get()));
  }
}

TEST_F(ServiceWorkerProviderHostTestWithPlzDedicatedWorker,
       ReservedClientsAreNotExposedToClientsApiForDedicatedWorker) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  TestReservedClientsAreNotExposed(
      blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker,
      GURL("https://www.example.com/dedicated_worker.js"));
}

TEST_F(ServiceWorkerProviderHostTest,
       ReservedClientsAreNotExposedToClientsApiForSharedWorker) {
  TestReservedClientsAreNotExposed(
      blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
      GURL("https://www.example.com/shared_worker.js"));
}

// Tests the client phase transitions for a navigation.
TEST_F(ServiceWorkerProviderHostTest, ClientPhaseForWindow) {
  std::unique_ptr<ServiceWorkerProviderHostAndInfo> host_and_info =
      CreateProviderHostAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                         /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerProviderHost> host =
      std::move(host_and_info->host);
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  remote_endpoint.BindForWindow(std::move(host_and_info->info));
  EXPECT_FALSE(host->is_response_committed());
  EXPECT_FALSE(host->is_execution_ready());

  FinishNavigation(host.get());
  EXPECT_TRUE(host->is_response_committed());
  EXPECT_FALSE(host->is_execution_ready());

  base::RunLoop run_loop;
  host->AddExecutionReadyCallback(run_loop.QuitClosure());
  remote_endpoint.host_remote()->get()->OnExecutionReady();
  run_loop.Run();
  EXPECT_TRUE(host->is_response_committed());
  EXPECT_TRUE(host->is_execution_ready());
}

// Tests the client phase transitions for workers.
void ServiceWorkerProviderHostTest::TestClientPhaseTransition(
    blink::mojom::ServiceWorkerProviderType provider_type,
    const GURL url) {
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForClient::New();
  provider_info->client_receiver =
      client_remote.InitWithNewEndpointAndPassReceiver();
  host_receiver =
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver();
  base::WeakPtr<ServiceWorkerProviderHost> host =
      ServiceWorkerProviderHost::CreateForWebWorker(
          helper_->context()->AsWeakPtr(), helper_->mock_render_process_id(),
          provider_type, std::move(host_receiver), std::move(client_remote));
  EXPECT_FALSE(host->is_response_committed());
  EXPECT_FALSE(host->is_execution_ready());

  host->UpdateUrls(url, url, url::Origin::Create(url));
  host->CompleteWebWorkerPreparation(
      network::mojom::CrossOriginEmbedderPolicy::kNone);

  EXPECT_TRUE(host->is_response_committed());
  EXPECT_TRUE(host->is_execution_ready());
}

TEST_F(ServiceWorkerProviderHostTestWithPlzDedicatedWorker,
       ClientPhaseForDedicatedWorker) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  TestClientPhaseTransition(
      blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker,
      GURL("https://www.example.com/dedicated_worker.js"));
}

TEST_F(ServiceWorkerProviderHostTest, ClientPhaseForSharedWorker) {
  TestClientPhaseTransition(
      blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
      GURL("https://www.example.com/shared_worker.js"));
}

// Tests that the service worker involved with a navigation (via
// AddServiceWorkerToUpdate) is updated when the host for the navigation is
// destroyed.
TEST_F(ServiceWorkerProviderHostTest, UpdateServiceWorkerOnDestruction) {
  // Make a window.
  ServiceWorkerProviderHost* host =
      CreateProviderHost(GURL("https://www.example.com/example.html"));

  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  auto version2 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration2_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 2 /* version_id */,
      helper_->context()->AsWeakPtr());
  version2->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version2->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration2_->SetActiveVersion(version1);

  host->AddServiceWorkerToUpdate(version1);
  host->AddServiceWorkerToUpdate(version2);
  ExpectUpdateIsNotScheduled(version1.get());
  ExpectUpdateIsNotScheduled(version2.get());

  // Destroy the provider host.
  ASSERT_TRUE(remote_endpoints_.back().host_remote()->is_bound());
  remote_endpoints_.back().host_remote()->reset();
  base::RunLoop().RunUntilIdle();

  // The provider host's destructor should have scheduled the update.
  ExpectUpdateIsScheduled(version1.get());
  ExpectUpdateIsScheduled(version2.get());
}

// Tests that the service worker involved with a navigation is updated when the
// host receives a HintToUpdateServiceWorker message.
TEST_F(ServiceWorkerProviderHostTest, HintToUpdateServiceWorker) {
  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  // Make a window.
  ServiceWorkerProviderHost* host =
      CreateProviderHost(GURL("https://www.example.com/example.html"));

  // Mark the service worker as needing update. Update should not be scheduled
  // yet.
  host->AddServiceWorkerToUpdate(version1);
  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_TRUE(HasVersionToUpdate(host));

  // Send the hint from the renderer. Update should be scheduled.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>*
      host_remote = remote_endpoints_.back().host_remote();
  (*host_remote)->HintToUpdateServiceWorker();
  base::RunLoop().RunUntilIdle();
  ExpectUpdateIsScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(host));
}

// Tests that the host receives a HintToUpdateServiceWorker message but
// there was no service worker at main resource request time. This
// can happen due to claim().
TEST_F(ServiceWorkerProviderHostTest,
       HintToUpdateServiceWorkerButNoVersionToUpdate) {
  // Make a window.
  ServiceWorkerProviderHost* host =
      CreateProviderHost(GURL("https://www.example.com/example.html"));

  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  // Pretend the registration gets associated after the main
  // resource request, so AddServiceWorkerToUpdate() is not called.

  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(host));

  // Send the hint from the renderer. Update should not be scheduled, since
  // AddServiceWorkerToUpdate() was not called.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>*
      host_remote = remote_endpoints_.back().host_remote();
  (*host_remote)->HintToUpdateServiceWorker();
  base::RunLoop().RunUntilIdle();
  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(host));
}

TEST_F(ServiceWorkerProviderHostTest, HintToUpdateServiceWorkerMultiple) {
  // Make active versions.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  auto version2 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration2_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 2 /* version_id */,
      helper_->context()->AsWeakPtr());
  version2->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version2->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration2_->SetActiveVersion(version1);

  auto version3 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration3_.get(), GURL("https://other.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 3 /* version_id */,
      helper_->context()->AsWeakPtr());
  version3->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version3->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration3_->SetActiveVersion(version1);

  // Make a window.
  ServiceWorkerProviderHost* host =
      CreateProviderHost(GURL("https://www.example.com/example.html"));

  // Mark the service worker as needing update. Update should not be scheduled
  // yet.
  host->AddServiceWorkerToUpdate(version1);
  host->AddServiceWorkerToUpdate(version2);
  host->AddServiceWorkerToUpdate(version3);
  ExpectUpdateIsNotScheduled(version1.get());
  ExpectUpdateIsNotScheduled(version2.get());
  ExpectUpdateIsNotScheduled(version3.get());
  EXPECT_TRUE(HasVersionToUpdate(host));

  // Pretend another page also used version3.
  version3->IncrementPendingUpdateHintCount();

  // Send the hint from the renderer. Update should be scheduled except for
  // |version3| as it's being used by another page.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>*
      host_remote = remote_endpoints_.back().host_remote();
  (*host_remote)->HintToUpdateServiceWorker();
  base::RunLoop().RunUntilIdle();
  ExpectUpdateIsScheduled(version1.get());
  ExpectUpdateIsScheduled(version2.get());
  ExpectUpdateIsNotScheduled(version3.get());
  EXPECT_FALSE(HasVersionToUpdate(host));

  // Pretend the other page also finished for version3.
  version3->DecrementPendingUpdateHintCount();
  ExpectUpdateIsScheduled(version3.get());
}

}  // namespace content
