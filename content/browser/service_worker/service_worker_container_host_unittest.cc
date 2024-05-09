// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"
#include "url/url_util.h"

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
        const net::SiteForCookies& site_for_cookies,
        const std::optional<url::Origin>& top_frame_origin,
        const GURL& script_url)
        : scope(scope),
          site_for_cookies(site_for_cookies),
          top_frame_origin(top_frame_origin),
          script_url(script_url) {}
    const GURL scope;
    const net::SiteForCookies site_for_cookies;
    const std::optional<url::Origin> top_frame_origin;
    const GURL script_url;
  };

  ServiceWorkerTestContentBrowserClient() {}

  AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override {
    logs_.emplace_back(scope, site_for_cookies, top_frame_origin, script_url);
    return AllowServiceWorkerResult::No();
  }

  const std::vector<AllowServiceWorkerCallLog>& logs() const { return logs_; }

 private:
  std::vector<AllowServiceWorkerCallLog> logs_;
};

}  // namespace

class ServiceWorkerContainerHostTest : public testing::Test {
 public:
  ServiceWorkerContainerHostTest(const ServiceWorkerContainerHostTest&) =
      delete;
  ServiceWorkerContainerHostTest& operator=(
      const ServiceWorkerContainerHostTest&) = delete;

 protected:
  ServiceWorkerContainerHostTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {
    SetContentClient(&test_content_client_);
    ReRegisterContentSchemesForTests();
  }
  ~ServiceWorkerContainerHostTest() override {}

  void SetUp() override {
    old_content_browser_client_ =
        SetBrowserClientForTesting(&test_content_browser_client_);
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &ServiceWorkerContainerHostTest::OnMojoError, base::Unretained(this)));

    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    context_ = helper_->context()->AsWeakPtr();
    script_url_ = GURL("https://www.example.com/service_worker.js");

    blink::mojom::ServiceWorkerRegistrationOptions options1;
    options1.scope = GURL("https://www.example.com/");
    const blink::StorageKey key1 = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(options1.scope));
    registration1_ = ServiceWorkerRegistration::Create(
        options1, key1, 1L, context_,
        blink::mojom::AncestorFrameType::kNormalFrame);

    blink::mojom::ServiceWorkerRegistrationOptions options2;
    options2.scope = GURL("https://www.example.com/example");
    const blink::StorageKey key2 = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(options2.scope));
    registration2_ = ServiceWorkerRegistration::Create(
        options2, key2, 2L, context_,
        blink::mojom::AncestorFrameType::kNormalFrame);

    blink::mojom::ServiceWorkerRegistrationOptions options3;
    options3.scope = GURL("https://other.example.com/");
    const blink::StorageKey key3 = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(options3.scope));
    registration3_ = ServiceWorkerRegistration::Create(
        options3, key3, 3L, context_,
        blink::mojom::AncestorFrameType::kNormalFrame);
  }

  void TearDown() override {
    registration1_ = nullptr;
    registration2_ = nullptr;
    registration3_ = nullptr;
    helper_.reset();
    SetBrowserClientForTesting(old_content_browser_client_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  ServiceWorkerRemoteContainerEndpoint PrepareServiceWorkerClient(
      const GURL& document_url) {
    ServiceWorkerRemoteContainerEndpoint remote_endpoint;
    url::Origin top_frame_origin = url::Origin::Create(document_url);
    CreateServiceWorkerClientInternal(document_url, top_frame_origin,
                                      &remote_endpoint);
    return remote_endpoint;
  }

  ServiceWorkerRemoteContainerEndpoint
  PrepareServiceWorkerClientWithSiteForCookies(
      const GURL& document_url,
      const std::optional<url::Origin>& top_frame_origin) {
    ServiceWorkerRemoteContainerEndpoint remote_endpoint;
    CreateServiceWorkerClientInternal(document_url, top_frame_origin,
                                      &remote_endpoint);
    return remote_endpoint;
  }

  base::WeakPtr<ServiceWorkerClient> CreateServiceWorkerClient(
      const GURL& document_url) {
    url::Origin top_frame_origin = url::Origin::Create(document_url);
    remote_endpoints_.emplace_back();
    return CreateServiceWorkerClientInternal(document_url, top_frame_origin,
                                             &remote_endpoints_.back());
  }

  base::WeakPtr<ServiceWorkerClient>
  CreateServiceWorkerClientWithInsecureParentFrame(const GURL& document_url) {
    remote_endpoints_.emplace_back();
    base::WeakPtr<ServiceWorkerClient> service_worker_client =
        CreateServiceWorkerClientForWindow(
            GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                                    /*mock frame_routing_id=*/1),
            /*is_parent_frame_secure=*/false, helper_->context()->AsWeakPtr(),
            &remote_endpoints_.back());
    service_worker_client->UpdateUrls(
        document_url, url::Origin::Create(document_url),
        blink::StorageKey::CreateFirstParty(url::Origin::Create(document_url)));
    return service_worker_client;
  }

  void FinishNavigation(ServiceWorkerClient* service_worker_client) {
    // In production code, the loader/request handler does this.
    const GURL url("https://www.example.com/page");
    service_worker_client->UpdateUrls(
        url, url::Origin::Create(url),
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

    // Establish a dummy connection to allow sending messages without errors.
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        reporter;
    auto dummy = reporter.InitWithNewPipeAndPassReceiver();

    // In production code this is called from NavigationRequest in the browser
    // process right before navigation commit.
    service_worker_client->CommitResponse(
        GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                                1 /* route_id */),
        PolicyContainerPolicies(), std::move(reporter),
        ukm::UkmRecorder::GetNewSourceID());
    // After the navigation commit, it is ready to call the container's methods.
    service_worker_client->SetContainerReady();
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
                          const std::optional<std::string>& error_msg,
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
               const std::optional<std::string>& error_msg,
               blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                   registration) {
              *out_error = error;
              if (out_info) {
                *out_info = std::move(registration);
              }
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
           const std::optional<std::string>& error_msg,
           std::optional<std::vector<
               blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>> infos) {
          *out_error = error;
        },
        &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  bool CanFindServiceWorkerClient(ServiceWorkerClient* service_worker_client) {
    if (context_) {
      for (auto it = context_->GetServiceWorkerClients(
               service_worker_client->key(),
               false /* include_reserved_clients */,
               false /* include_back_forward_cached_clients */);
           !it.IsAtEnd(); ++it) {
        if (service_worker_client == &*it) {
          return true;
        }
      }
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

  bool HasVersionToUpdate(ServiceWorkerClient* service_worker_client) {
    return !service_worker_client->versions_to_update_.empty();
  }

  blink::StorageKey GetCorrectStorageKeyForWebSecurityState(
      ServiceWorkerClient* service_worker_client,
      const GURL& url) const {
    return service_worker_security_utils::
        GetCorrectStorageKeyForWebSecurityState(service_worker_client->key(),
                                                url);
  }

  void TestReservedClientsAreNotExposed(ServiceWorkerClientInfo client_info,
                                        const GURL& url);
  void TestClientPhaseTransition(ServiceWorkerClientInfo client_info,
                                 const GURL& url);

  void TestBackForwardCachedClientsAreNotExposed(const GURL& url);

  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<ServiceWorkerRegistration> registration1_;
  scoped_refptr<ServiceWorkerRegistration> registration2_;
  scoped_refptr<ServiceWorkerRegistration> registration3_;
  GURL script_url_;
  ServiceWorkerTestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_ = nullptr;
  std::vector<ServiceWorkerRemoteContainerEndpoint> remote_endpoints_;
  std::vector<std::string> bad_messages_;

 private:
  base::WeakPtr<ServiceWorkerClient> CreateServiceWorkerClientInternal(
      const GURL& document_url,
      const std::optional<url::Origin>& top_frame_origin,
      ServiceWorkerRemoteContainerEndpoint* remote_endpoint) {
    base::WeakPtr<ServiceWorkerClient> service_worker_client =
        CreateServiceWorkerClientForWindow(
            GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                                    /*mock frame_routing_id=*/1),
            /*is_parent_frame_secure=*/true, helper_->context()->AsWeakPtr(),
            remote_endpoint);
    service_worker_client->UpdateUrls(
        document_url, top_frame_origin,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(document_url)));
    return service_worker_client;
  }

  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Run tests with PlzDedicatedWorker.
// TODO(crbug.com/40093136): Merge this test fixture into
// ServiceWorkerContainerHostTest once PlzDedicatedWorker is enabled by default.
class ServiceWorkerContainerHostTestWithPlzDedicatedWorker
    : public ServiceWorkerContainerHostTest {
 public:
  ServiceWorkerContainerHostTestWithPlzDedicatedWorker() {
    // ServiceWorkerClient for dedicated workers is available only when
    // PlzDedicatedWorker is enabled.
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPlzDedicatedWorker);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ServiceWorkerContainerHostTest, MatchRegistration) {
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example1.html"));

  // Match registration should return the longest matching one.
  ASSERT_EQ(registration2_, service_worker_client->MatchRegistration());
  service_worker_client->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, service_worker_client->MatchRegistration());

  // Should return nullptr after removing all matching registrations.
  service_worker_client->RemoveMatchingRegistration(registration1_.get());
  ASSERT_EQ(nullptr, service_worker_client->MatchRegistration());

  // UpdateUrls sets all of matching registrations
  service_worker_client->UpdateUrls(
      GURL("https://www.example.com/example1"),
      url::Origin::Create(GURL("https://www.example.com/example1")),
      blink::StorageKey::CreateFromStringForTesting(
          "https://www.example.com/example1"));
  ASSERT_EQ(registration2_, service_worker_client->MatchRegistration());
  service_worker_client->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, service_worker_client->MatchRegistration());

  // UpdateUrls with another origin also updates matching registrations
  service_worker_client->UpdateUrls(
      GURL("https://other.example.com/example"),
      url::Origin::Create(GURL("https://other.example.com/example")),
      blink::StorageKey::CreateFromStringForTesting(
          "https://other.example.com/example1"));
  ASSERT_EQ(registration3_, service_worker_client->MatchRegistration());
  service_worker_client->RemoveMatchingRegistration(registration3_.get());
  ASSERT_EQ(nullptr, service_worker_client->MatchRegistration());
}

TEST_F(ServiceWorkerContainerHostTest, ContextSecurity) {
  base::WeakPtr<ServiceWorkerClient> service_worker_client_secure_parent =
      CreateServiceWorkerClient(GURL("https://www.example.com/example1.html"));
  base::WeakPtr<ServiceWorkerClient> service_worker_client_insecure_parent =
      CreateServiceWorkerClientWithInsecureParentFrame(
          GURL("https://www.example.com/example1.html"));

  // Insecure document URL.
  service_worker_client_secure_parent->UpdateUrls(
      GURL("http://host"), url::Origin::Create(GURL("http://host")),
      blink::StorageKey::CreateFromStringForTesting("http://host"));
  EXPECT_FALSE(service_worker_client_secure_parent
                   ->IsEligibleForServiceWorkerController());

  // Insecure parent frame.
  service_worker_client_insecure_parent->UpdateUrls(
      GURL("https://host"), url::Origin::Create(GURL("https://host")),
      blink::StorageKey::CreateFromStringForTesting("https://host"));
  EXPECT_FALSE(service_worker_client_insecure_parent
                   ->IsEligibleForServiceWorkerController());

  // Secure URL and parent frame.
  service_worker_client_secure_parent->UpdateUrls(
      GURL("https://host"), url::Origin::Create(GURL("https://host")),
      blink::StorageKey::CreateFromStringForTesting("https://host"));
  EXPECT_TRUE(service_worker_client_secure_parent
                  ->IsEligibleForServiceWorkerController());

  // Exceptional service worker scheme.
  GURL url(std::string(kServiceWorkerScheme) + "://host");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(url.is_valid());
  EXPECT_FALSE(network::IsUrlPotentiallyTrustworthy(url));
  EXPECT_TRUE(OriginCanAccessServiceWorkers(url));
  service_worker_client_secure_parent->UpdateUrls(
      url, origin, blink::StorageKey::CreateFirstParty(origin));
  EXPECT_TRUE(service_worker_client_secure_parent
                  ->IsEligibleForServiceWorkerController());

  // Exceptional service worker scheme with insecure parent frame.
  service_worker_client_insecure_parent->UpdateUrls(
      url, origin, blink::StorageKey::CreateFirstParty(origin));
  EXPECT_FALSE(service_worker_client_insecure_parent
                   ->IsEligibleForServiceWorkerController());
}

TEST_F(ServiceWorkerContainerHostTest, UpdateUrls_SameOriginRedirect) {
  const GURL url1("https://origin1.example.com/page1.html");
  const GURL url2("https://origin1.example.com/page2.html");

  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(url1);
  const std::string uuid1 = service_worker_client->client_uuid();
  EXPECT_EQ(url1, service_worker_client->url());
  EXPECT_TRUE(service_worker_security_utils::site_for_cookies(
                  service_worker_client->key())
                  .IsEquivalent(net::SiteForCookies::FromUrl(url1)));

  service_worker_client->UpdateUrls(
      url2, url::Origin::Create(url2),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url2)));
  EXPECT_EQ(url2, service_worker_client->url());
  EXPECT_TRUE(service_worker_security_utils::site_for_cookies(
                  service_worker_client->key())
                  .IsEquivalent(net::SiteForCookies::FromUrl(url2)));
  EXPECT_EQ(uuid1, service_worker_client->client_uuid());

  ASSERT_TRUE(context_);
  EXPECT_EQ(service_worker_client.get(),
            context_->GetServiceWorkerClientByClientID(
                service_worker_client->client_uuid()));
}

TEST_F(ServiceWorkerContainerHostTest, UpdateUrls_CrossOriginRedirect) {
  const GURL url1("https://origin1.example.com/page1.html");
  const GURL url2("https://origin2.example.com/page2.html");

  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(url1);
  const std::string uuid1 = service_worker_client->client_uuid();
  EXPECT_EQ(url1, service_worker_client->url());
  EXPECT_TRUE(service_worker_security_utils::site_for_cookies(
                  service_worker_client->key())
                  .IsEquivalent(net::SiteForCookies::FromUrl(url1)));

  service_worker_client->UpdateUrls(
      url2, url::Origin::Create(url2),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url2)));
  EXPECT_EQ(url2, service_worker_client->url());
  EXPECT_TRUE(service_worker_security_utils::site_for_cookies(
                  service_worker_client->key())
                  .IsEquivalent(net::SiteForCookies::FromUrl(url2)));
  EXPECT_NE(uuid1, service_worker_client->client_uuid());

  ASSERT_TRUE(context_);
  EXPECT_FALSE(context_->GetServiceWorkerClientByClientID(uuid1));
  EXPECT_EQ(service_worker_client.get(),
            context_->GetServiceWorkerClientByClientID(
                service_worker_client->client_uuid()));
}

TEST_F(ServiceWorkerContainerHostTest, UpdateUrls_CorrectStorageKey) {
  const GURL url1("https://origin1.example.com/page1.html");
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url1));
  const GURL url2("https://origin2.example.com/page2.html");
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url2));

  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(url1);
  EXPECT_EQ(key1, service_worker_client->key());

  service_worker_client->UpdateUrls(url2, url::Origin::Create(url2), key2);
  EXPECT_EQ(key2, service_worker_client->key());
}

TEST_F(ServiceWorkerContainerHostTest, ForServiceWorker_CorrectStorageKey) {
  const GURL url3("https://origin3.example.com/sw.js");
  const blink::StorageKey key3 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url3));

  auto container_host_for_service_worker =
      std::make_unique<ServiceWorkerContainerHostForServiceWorker>(
          helper_->context()->AsWeakPtr(), /*service_worker_host=*/nullptr,
          url3, key3);
  EXPECT_EQ(key3, container_host_for_service_worker->key());
}

TEST_F(ServiceWorkerContainerHostTest,
       ForServiceWorkerWithTopLevelSite_CorrectStorageKey) {
  const GURL url4("https://origin3.example.com/sw.js");
  const GURL url4_top_level_site("https://other.com/");
  const blink::StorageKey key4 = blink::StorageKey::Create(
      url::Origin::Create(url4), net::SchemefulSite(url4_top_level_site),
      blink::mojom::AncestorChainBit::kCrossSite, true);

  auto container_host_for_service_worker =
      std::make_unique<ServiceWorkerContainerHostForServiceWorker>(
          helper_->context()->AsWeakPtr(), /*service_worker_host=*/nullptr,
          url4, key4);
  EXPECT_EQ(key4, container_host_for_service_worker->key());
}

TEST_F(ServiceWorkerContainerHostTest,
       GetCorrectStorageKeyForWebSecurityState) {
  // Without disable-web-security this function should return always return the
  // container host's key.
  const GURL url1("https://origin1.example.com/");
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url1));
  const GURL url2("https://origin2.example.com/");
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url2));
  const GURL url3("https://origin3.example.com/");
  const blink::StorageKey key3 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url3));

  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(url1);

  EXPECT_EQ(service_worker_client->key(), key1);

  EXPECT_EQ(service_worker_client->key(),
            GetCorrectStorageKeyForWebSecurityState(service_worker_client.get(),
                                                    url1));
  EXPECT_EQ(service_worker_client->key(),
            GetCorrectStorageKeyForWebSecurityState(service_worker_client.get(),
                                                    url2));
  EXPECT_EQ(service_worker_client->key(),
            GetCorrectStorageKeyForWebSecurityState(service_worker_client.get(),
                                                    url3));

  // With disable-web-security we should get a new key for the cross-origin
  // urls.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisableWebSecurity);

  EXPECT_EQ(service_worker_client->key(),
            GetCorrectStorageKeyForWebSecurityState(service_worker_client.get(),
                                                    url1));
  EXPECT_EQ(key2, GetCorrectStorageKeyForWebSecurityState(
                      service_worker_client.get(), url2));
  EXPECT_EQ(key3, GetCorrectStorageKeyForWebSecurityState(
                      service_worker_client.get(), url3));
}

TEST_F(ServiceWorkerContainerHostTest, RemoveProvider) {
  // Create a container host connected with the renderer process.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example1.html"));
  EXPECT_TRUE(service_worker_client);

  // Disconnect the mojo pipe from the renderer side.
  ASSERT_TRUE(remote_endpoints_.back().host_remote()->is_bound());
  remote_endpoints_.back().host_remote()->reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service_worker_client);
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

TEST_F(ServiceWorkerContainerHostTest, Controller) {
  // Create a host.
  std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
      CreateServiceWorkerClientAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                                /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      std::move(client_and_info->service_worker_client);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindForWindow(std::move(client_and_info->info));
  auto container = std::make_unique<MockServiceWorkerContainer>(
      std::move(*remote_endpoints_.back().client_receiver()));

  // Create an active version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version);

  // Finish the navigation.
  FinishNavigation(service_worker_client.get());
  service_worker_client->SetControllerRegistration(
      registration1_, false /* notify_controllerchange */);
  remote_endpoints_.back().host_remote()->get()->OnExecutionReady();
  base::RunLoop().RunUntilIdle();

  // The page should be controlled since there was an active version at the
  // time navigation started. The SetController IPC should have been sent.
  EXPECT_TRUE(service_worker_client->controller());
  EXPECT_TRUE(container->was_set_controller_called());
  EXPECT_EQ(registration1_.get(), service_worker_client->MatchRegistration());
}

TEST_F(ServiceWorkerContainerHostTest, UncontrolledWithMatchingRegistration) {
  // Create a host.
  std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
      CreateServiceWorkerClientAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                                /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      std::move(client_and_info->service_worker_client);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindForWindow(std::move(client_and_info->info));
  auto container = std::make_unique<MockServiceWorkerContainer>(
      std::move(*remote_endpoints_.back().client_receiver()));

  // Create an installing version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  registration1_->SetInstallingVersion(version);

  // Finish the navigation.
  FinishNavigation(service_worker_client.get());
  // Promote the worker to active while navigation is still happening.
  registration1_->SetActiveVersion(version);
  base::RunLoop().RunUntilIdle();

  // The page should not be controlled since there was no active version at the
  // time navigation started. Furthermore, no SetController IPC should have been
  // sent.
  EXPECT_FALSE(service_worker_client->controller());
  EXPECT_FALSE(container->was_set_controller_called());
  // However, the host should know the registration is its best match, for
  // .ready and claim().
  EXPECT_EQ(registration1_.get(), service_worker_client->MatchRegistration());
}

TEST_F(ServiceWorkerContainerHostTest,
       Register_ContentSettingsDisallowsServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClientWithSiteForCookies(
          GURL("https://www.example.com/foo"),
          url::Origin::Create(GURL("https://www.example.com")));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("https://www.example.com/scope"),
                     GURL("https://www.example.com/bar")));
  ASSERT_EQ(1ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/scope"),
            test_browser_client.logs()[0].scope);
  EXPECT_TRUE(test_browser_client.logs()[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("https://www.example.com/top"))));
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
  EXPECT_TRUE(test_browser_client.logs()[1].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("https://www.example.com/top"))));

  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            test_browser_client.logs()[1].top_frame_origin);
  EXPECT_EQ(GURL(), test_browser_client.logs()[1].script_url);

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            GetRegistrations(remote_endpoint.host_remote()->get()));
  ASSERT_EQ(3ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/foo"),
            test_browser_client.logs()[2].scope);
  EXPECT_TRUE(test_browser_client.logs()[2].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("https://www.example.com/top"))));
  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            *test_browser_client.logs()[2].top_frame_origin);
  EXPECT_EQ(GURL(), test_browser_client.logs()[2].script_url);

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerContainerHostTest, AllowServiceWorker) {
  // Create an active version.
  scoped_refptr<ServiceWorkerVersion> version =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration1_.get(), GURL("https://www.example.com/sw.js"),
          blink::mojom::ScriptType::kClassic, 1 /* version_id */,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          helper_->context()->AsWeakPtr());
  registration1_->SetActiveVersion(version);

  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerHost> worker_host = CreateServiceWorkerHost(
      helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
      *version, helper_->context()->AsWeakPtr(), &remote_endpoint);
  ServiceWorkerContainerHost* container_host = worker_host->container_host();

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  EXPECT_FALSE(container_host->AllowServiceWorker(
      GURL("https://www.example.com/scope"), GURL()));

  ASSERT_EQ(1ul, test_browser_client.logs().size());
  EXPECT_EQ(GURL("https://www.example.com/scope"),
            test_browser_client.logs()[0].scope);
  EXPECT_TRUE(test_browser_client.logs()[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("https://www.example.com/sw.js"))));
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            test_browser_client.logs()[0].top_frame_origin);
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerContainerHostTest, Register_HTTPS) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("https://www.example.com/"),
                     GURL("https://www.example.com/bar")));
}

TEST_F(ServiceWorkerContainerHostTest, Register_NonSecureTransportLocalhost) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("http://127.0.0.3:81/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            Register(remote_endpoint.host_remote()->get(),
                     GURL("http://127.0.0.3:81/bar"),
                     GURL("http://127.0.0.3:81/baz")));
}

TEST_F(ServiceWorkerContainerHostTest, Register_InvalidScopeShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(), GURL(""),
           GURL("https://www.example.com/bar/hoge.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest, Register_InvalidScriptShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("https://www.example.com/bar/"), GURL(""));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest, Register_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("http://www.example.com/"), GURL("http://www.example.com/bar"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest, Register_CrossOriginShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

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

TEST_F(ServiceWorkerContainerHostTest, Register_BadCharactersShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com"));

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

TEST_F(ServiceWorkerContainerHostTest, Register_FileSystemDocumentShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(
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

TEST_F(ServiceWorkerContainerHostTest,
       Register_FileSystemScriptOrScopeShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/temporary/"));

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

class WebUIUntrustedServiceWorkerContainerHostTest
    : public ServiceWorkerContainerHostTest,
      public testing::WithParamInterface<bool> {
 public:
  WebUIUntrustedServiceWorkerContainerHostTest() {
    if (GetParam()) {
      features_.InitAndEnableFeature(
          features::kEnableServiceWorkersForChromeUntrusted);
    } else {
      features_.InitAndDisableFeature(
          features::kEnableServiceWorkersForChromeUntrusted);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that chrome:// webuis can't register service workers even if the
// chrome-untrusted:// SW flag is on.
TEST_P(WebUIUntrustedServiceWorkerContainerHostTest,
       Register_RegistrationShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("chrome://testwebui/"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(), GURL("chrome://testwebui/"),
           GURL("chrome://testwebui/sw.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_P(WebUIUntrustedServiceWorkerContainerHostTest,
       Register_UntrustedRegistrationShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("chrome-untrusted://testwebui/"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("chrome-untrusted://testwebui/"),
           GURL("chrome-untrusted://testwebui/sw.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIUntrustedServiceWorkerContainerHostTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           if (info.param) {
                             return "ServiceWorkersForChromeUntrustedEnabled";
                           }
                           return "ServiceWorkersForChromeUntrustedDisabled";
                         });

class WebUIServiceWorkerContainerHostTest
    : public ServiceWorkerContainerHostTest,
      public testing::WithParamInterface<bool> {
 public:
  WebUIServiceWorkerContainerHostTest() {
    if (GetParam()) {
      features_.InitAndEnableFeature(
          features::kEnableServiceWorkersForChromeScheme);
    } else {
      features_.InitAndDisableFeature(
          features::kEnableServiceWorkersForChromeScheme);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(WebUIServiceWorkerContainerHostTest, Register_RegistrationShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("chrome://testwebui/"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(), GURL("chrome://testwebui/"),
           GURL("chrome://testwebui/sw.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

// Test that chrome-untrusted:// service workers are disallowed with the
// chrome:// flag turned on.
TEST_P(WebUIServiceWorkerContainerHostTest,
       Register_UntrustedRegistrationShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("chrome-untrusted://testwebui/"));

  ASSERT_TRUE(bad_messages_.empty());
  Register(remote_endpoint.host_remote()->get(),
           GURL("chrome-untrusted://testwebui/"),
           GURL("chrome-untrusted://testwebui/sw.js"));
  EXPECT_EQ(1u, bad_messages_.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIServiceWorkerContainerHostTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           if (info.param) {
                             return "ServiceWorkersForChromeEnabled";
                           }
                           return "ServiceWorkersForChromeDisabled";
                         });

TEST_F(ServiceWorkerContainerHostTest, EarlyContextDeletion) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  // Because ServiceWorkerContextCore owns ServiceWorkerClient, our
  // ServiceWorkerClient instance has destroyed.
  EXPECT_FALSE(remote_endpoint.host_remote()->is_connected());
}

TEST_F(ServiceWorkerContainerHostTest, GetRegistration_Success) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

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

TEST_F(ServiceWorkerContainerHostTest,
       GetRegistration_NotFoundShouldReturnNull) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            GetRegistration(remote_endpoint.host_remote()->get(),
                            GURL("https://www.example.com/"), &info));
  EXPECT_FALSE(info);
}

TEST_F(ServiceWorkerContainerHostTest, GetRegistration_CrossOriginShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(),
                  GURL("https://foo.example.com/"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest, GetRegistration_InvalidScopeShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(), GURL(""));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest,
       GetRegistration_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistration(remote_endpoint.host_remote()->get(),
                  GURL("http://www.example.com/"));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerContainerHostTest, GetRegistrations_SecureOrigin) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("https://www.example.com/foo"));

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            GetRegistrations(remote_endpoint.host_remote()->get()));
}

TEST_F(ServiceWorkerContainerHostTest,
       GetRegistrations_NonSecureOriginShouldFail) {
  ServiceWorkerRemoteContainerEndpoint remote_endpoint =
      PrepareServiceWorkerClient(GURL("http://www.example.com/foo"));

  ASSERT_TRUE(bad_messages_.empty());
  GetRegistrations(remote_endpoint.host_remote()->get());
  EXPECT_EQ(1u, bad_messages_.size());
}

// Test that a "reserved" (i.e., not execution ready) client is not included
// when iterating over client container hosts. If it were, it'd be undesirably
// exposed via the Clients API.
void ServiceWorkerContainerHostTest::TestReservedClientsAreNotExposed(
    ServiceWorkerClientInfo client_info,
    const GURL& url) {
  {
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver;
    auto container_info =
        blink::mojom::ServiceWorkerContainerInfoForClient::New();
    container_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    host_receiver =
        container_info->host_remote.InitWithNewEndpointAndPassReceiver();

    ASSERT_TRUE(context_);
    base::WeakPtr<ServiceWorkerClient> service_worker_client =
        context_->CreateServiceWorkerClientForWorker(
            std::move(host_receiver), helper_->mock_render_process_id(),
            std::move(client_remote), client_info);
    service_worker_client->UpdateUrls(
        url, url::Origin::Create(url),
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
    EXPECT_FALSE(CanFindServiceWorkerClient(service_worker_client.get()));
    service_worker_client->CommitResponse(
        /*rfh_id=*/std::nullopt, PolicyContainerPolicies(),
        /*coep_reporter=*/{}, ukm::UkmRecorder::GetNewSourceID());
    service_worker_client->SetExecutionReady();
    EXPECT_TRUE(CanFindServiceWorkerClient(service_worker_client.get()));
  }

  {
    std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
        CreateServiceWorkerClientAndInfoForWindow(
            helper_->context()->AsWeakPtr(),
            /*are_ancestors_secure=*/true);
    base::WeakPtr<ServiceWorkerClient> service_worker_client =
        std::move(client_and_info->service_worker_client);
    ServiceWorkerRemoteContainerEndpoint remote_endpoint;
    remote_endpoint.BindForWindow(std::move(client_and_info->info));

    FinishNavigation(service_worker_client.get());
    EXPECT_FALSE(CanFindServiceWorkerClient(service_worker_client.get()));

    base::RunLoop run_loop;
    service_worker_client->AddExecutionReadyCallback(run_loop.QuitClosure());
    remote_endpoint.host_remote()->get()->OnExecutionReady();
    run_loop.Run();
    EXPECT_TRUE(CanFindServiceWorkerClient(service_worker_client.get()));
  }
}

TEST_F(ServiceWorkerContainerHostTestWithPlzDedicatedWorker,
       ReservedClientsAreNotExposedToClientsApiForDedicatedWorker) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  TestReservedClientsAreNotExposed(
      ServiceWorkerClientInfo(blink::DedicatedWorkerToken()),
      GURL("https://www.example.com/dedicated_worker.js"));
}

TEST_F(ServiceWorkerContainerHostTest,
       ReservedClientsAreNotExposedToClientsApiForSharedWorker) {
  TestReservedClientsAreNotExposed(
      ServiceWorkerClientInfo(blink::SharedWorkerToken()),
      GURL("https://www.example.com/shared_worker.js"));
}

// Tests the client phase transitions for a navigation.
TEST_F(ServiceWorkerContainerHostTest, ClientPhaseForWindow) {
  std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
      CreateServiceWorkerClientAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                                /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      std::move(client_and_info->service_worker_client);
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  remote_endpoint.BindForWindow(std::move(client_and_info->info));
  EXPECT_FALSE(service_worker_client->is_response_committed());
  EXPECT_FALSE(service_worker_client->is_execution_ready());

  FinishNavigation(service_worker_client.get());
  EXPECT_TRUE(service_worker_client->is_response_committed());
  EXPECT_FALSE(service_worker_client->is_execution_ready());

  base::RunLoop run_loop;
  service_worker_client->AddExecutionReadyCallback(run_loop.QuitClosure());
  remote_endpoint.host_remote()->get()->OnExecutionReady();
  run_loop.Run();
  EXPECT_TRUE(service_worker_client->is_response_committed());
  EXPECT_TRUE(service_worker_client->is_execution_ready());
}

// Tests the client phase transitions for workers.
void ServiceWorkerContainerHostTest::TestClientPhaseTransition(
    ServiceWorkerClientInfo client_info,
    const GURL& url) {
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  auto container_info =
      blink::mojom::ServiceWorkerContainerInfoForClient::New();
  container_info->client_receiver =
      client_remote.InitWithNewEndpointAndPassReceiver();
  host_receiver =
      container_info->host_remote.InitWithNewEndpointAndPassReceiver();

  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      helper_->context()->CreateServiceWorkerClientForWorker(
          std::move(host_receiver), helper_->mock_render_process_id(),
          std::move(client_remote), client_info);
  EXPECT_FALSE(service_worker_client->is_response_committed());
  EXPECT_FALSE(service_worker_client->is_execution_ready());

  service_worker_client->UpdateUrls(
      url, url::Origin::Create(url),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
  service_worker_client->CommitResponse(
      /*rfh_id=*/std::nullopt, PolicyContainerPolicies(), /*coep_reporter=*/{},
      ukm::UkmRecorder::GetNewSourceID());
  service_worker_client->SetExecutionReady();

  EXPECT_TRUE(service_worker_client->is_response_committed());
  EXPECT_TRUE(service_worker_client->is_execution_ready());
}

TEST_F(ServiceWorkerContainerHostTestWithPlzDedicatedWorker,
       ClientPhaseForDedicatedWorker) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  TestClientPhaseTransition(
      ServiceWorkerClientInfo(blink::DedicatedWorkerToken()),
      GURL("https://www.example.com/dedicated_worker.js"));
}

TEST_F(ServiceWorkerContainerHostTest, ClientPhaseForSharedWorker) {
  TestClientPhaseTransition(ServiceWorkerClientInfo(blink::SharedWorkerToken()),
                            GURL("https://www.example.com/shared_worker.js"));
}

// Run tests with BackForwardCache.
class ServiceWorkerContainerHostTestWithBackForwardCache
    : public ServiceWorkerContainerHostTest {
 public:
  ServiceWorkerContainerHostTestWithBackForwardCache() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that a client in BackForwardCache is not included
// when iterating over client container hosts. If it were, it'd be undesirably
// exposed via the Clients API.
void ServiceWorkerContainerHostTest::TestBackForwardCachedClientsAreNotExposed(
    const GURL& url) {
  std::unique_ptr<ServiceWorkerHost> worker_host;
  {
    // Create an active version.
    scoped_refptr<ServiceWorkerVersion> version =
        base::MakeRefCounted<ServiceWorkerVersion>(
            registration1_.get(), url, blink::mojom::ScriptType::kClassic,
            1 /* version_id */,
            mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
            helper_->context()->AsWeakPtr());
    registration1_->SetActiveVersion(version);

    ServiceWorkerRemoteContainerEndpoint remote_endpoint;
    worker_host = CreateServiceWorkerHost(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        *version, helper_->context()->AsWeakPtr(), &remote_endpoint);
    ASSERT_TRUE(worker_host);
  }
  {
    std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
        CreateServiceWorkerClientAndInfoForWindow(
            helper_->context()->AsWeakPtr(),
            /*are_ancestors_secure=*/true);
    base::WeakPtr<ServiceWorkerClient> service_worker_client =
        std::move(client_and_info->service_worker_client);
    ServiceWorkerRemoteContainerEndpoint remote_endpoint;
    remote_endpoint.BindForWindow(std::move(client_and_info->info));

    FinishNavigation(service_worker_client.get());
    EXPECT_FALSE(CanFindServiceWorkerClient(service_worker_client.get()));

    base::RunLoop run_loop;
    service_worker_client->AddExecutionReadyCallback(run_loop.QuitClosure());
    remote_endpoint.host_remote()->get()->OnExecutionReady();
    run_loop.Run();
    EXPECT_TRUE(CanFindServiceWorkerClient(service_worker_client.get()));
    service_worker_client->EnterBackForwardCacheForTesting();
    EXPECT_FALSE(CanFindServiceWorkerClient(service_worker_client.get()));
    service_worker_client->LeaveBackForwardCacheForTesting();
    EXPECT_TRUE(CanFindServiceWorkerClient(service_worker_client.get()));
  }
}

TEST_F(ServiceWorkerContainerHostTestWithBackForwardCache,
       SkipBackForwardCachedServiceWorker) {
  ASSERT_TRUE(IsBackForwardCacheEnabled());

  TestBackForwardCachedClientsAreNotExposed(
      GURL("https://www.example.com/sw.js"));
}

class TestServiceWorkerContextCoreObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  explicit TestServiceWorkerContextCoreObserver(
      ServiceWorkerContextWrapper* wrapper) {
    observation_.Observe(wrapper);
  }

  void OnControlleeAdded(int64_t version_id,
                         const std::string& uuid,
                         const ServiceWorkerClientInfo& info) override {
    ++on_controllee_added_count_;
  }
  void OnControlleeRemoved(int64_t version_id,
                           const std::string& uuid) override {
    ++on_controllee_removed_count_;
  }
  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& uuid,
      GlobalRenderFrameHostId render_frame_host_id) override {
    ++on_controllee_navigation_committed_count_;
  }

  int on_controllee_added_count() const { return on_controllee_added_count_; }
  int on_controllee_removed_count() const {
    return on_controllee_removed_count_;
  }
  int on_controllee_navigation_committed_count() const {
    return on_controllee_navigation_committed_count_;
  }

 private:
  int on_controllee_added_count_ = 0;
  int on_controllee_removed_count_ = 0;
  int on_controllee_navigation_committed_count_ = 0;

  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};
};

TEST_F(ServiceWorkerContainerHostTestWithBackForwardCache, ControlleeEvents) {
  TestServiceWorkerContextCoreObserver observer(helper_->context_wrapper());

  // Create a host.
  std::unique_ptr<ServiceWorkerClientAndInfo> client_and_info =
      CreateServiceWorkerClientAndInfoForWindow(helper_->context()->AsWeakPtr(),
                                                /*are_ancestors_secure=*/true);
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      std::move(client_and_info->service_worker_client);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindForWindow(std::move(client_and_info->info));
  auto container = std::make_unique<MockServiceWorkerContainer>(
      std::move(*remote_endpoints_.back().client_receiver()));

  // Create an active version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version);

  // Finish the navigation.
  FinishNavigation(service_worker_client.get());
  service_worker_client->SetControllerRegistration(
      registration1_, false /* notify_controllerchange */);
  remote_endpoints_.back().host_remote()->get()->OnExecutionReady();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(observer.on_controllee_added_count(), 1);
  EXPECT_EQ(observer.on_controllee_navigation_committed_count(), 0);
  EXPECT_EQ(observer.on_controllee_removed_count(), 0);

  // The navigation commit ending should send the
  // OnControlleeNavigationCommitted() notification.
  service_worker_client->OnEndNavigationCommit();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(observer.on_controllee_added_count(), 1);
  EXPECT_EQ(observer.on_controllee_navigation_committed_count(), 1);
  EXPECT_EQ(observer.on_controllee_removed_count(), 0);

  version->MoveControlleeToBackForwardCacheMap(
      service_worker_client->client_uuid());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(observer.on_controllee_added_count(), 1);
  EXPECT_EQ(observer.on_controllee_navigation_committed_count(), 1);
  EXPECT_EQ(observer.on_controllee_removed_count(), 1);

  version->RestoreControlleeFromBackForwardCacheMap(
      service_worker_client->client_uuid());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(observer.on_controllee_added_count(), 2);
  EXPECT_EQ(observer.on_controllee_navigation_committed_count(), 2);
  EXPECT_EQ(observer.on_controllee_removed_count(), 1);
}

// Tests that the service worker involved with a navigation (via
// AddServiceWorkerToUpdate) is updated when the host for the navigation is
// destroyed.
TEST_F(ServiceWorkerContainerHostTest, UpdateServiceWorkerOnDestruction) {
  // Make a window.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example.html"));

  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  auto version2 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration2_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 2 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version2->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version2->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration2_->SetActiveVersion(version1);

  service_worker_client->AddServiceWorkerToUpdate(version1);
  service_worker_client->AddServiceWorkerToUpdate(version2);
  ExpectUpdateIsNotScheduled(version1.get());
  ExpectUpdateIsNotScheduled(version2.get());

  // Destroy the container host.
  ASSERT_TRUE(remote_endpoints_.back().host_remote()->is_bound());
  remote_endpoints_.back().host_remote()->reset();
  base::RunLoop().RunUntilIdle();

  // The container host's destructor should have scheduled the update.
  ExpectUpdateIsScheduled(version1.get());
  ExpectUpdateIsScheduled(version2.get());
}

// Tests that the service worker involved with a navigation is updated when the
// host receives a HintToUpdateServiceWorker message.
TEST_F(ServiceWorkerContainerHostTest, HintToUpdateServiceWorker) {
  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  // Make a window.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example.html"));

  // Mark the service worker as needing update. Update should not be scheduled
  // yet.
  service_worker_client->AddServiceWorkerToUpdate(version1);
  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_TRUE(HasVersionToUpdate(service_worker_client.get()));

  // Send the hint from the renderer. Update should be scheduled.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>*
      host_remote = remote_endpoints_.back().host_remote();
  (*host_remote)->HintToUpdateServiceWorker();
  base::RunLoop().RunUntilIdle();
  ExpectUpdateIsScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(service_worker_client.get()));
}

// Tests that the host receives a HintToUpdateServiceWorker message but
// there was no service worker at main resource request time. This
// can happen due to claim().
TEST_F(ServiceWorkerContainerHostTest,
       HintToUpdateServiceWorkerButNoVersionToUpdate) {
  // Make a window.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example.html"));

  // Make an active version.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  // Pretend the registration gets associated after the main
  // resource request, so AddServiceWorkerToUpdate() is not called.

  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(service_worker_client.get()));

  // Send the hint from the renderer. Update should not be scheduled, since
  // AddServiceWorkerToUpdate() was not called.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>*
      host_remote = remote_endpoints_.back().host_remote();
  (*host_remote)->HintToUpdateServiceWorker();
  base::RunLoop().RunUntilIdle();
  ExpectUpdateIsNotScheduled(version1.get());
  EXPECT_FALSE(HasVersionToUpdate(service_worker_client.get()));
}

TEST_F(ServiceWorkerContainerHostTest, HintToUpdateServiceWorkerMultiple) {
  // Make active versions.
  auto version1 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 1 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version1->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration1_->SetActiveVersion(version1);

  auto version2 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration2_.get(), GURL("https://www.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 2 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version2->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version2->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration2_->SetActiveVersion(version1);

  auto version3 = base::MakeRefCounted<ServiceWorkerVersion>(
      registration3_.get(), GURL("https://other.example.com/sw.js"),
      blink::mojom::ScriptType::kClassic, 3 /* version_id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version3->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version3->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration3_->SetActiveVersion(version1);

  // Make a window.
  base::WeakPtr<ServiceWorkerClient> service_worker_client =
      CreateServiceWorkerClient(GURL("https://www.example.com/example.html"));

  // Mark the service worker as needing update. Update should not be scheduled
  // yet.
  service_worker_client->AddServiceWorkerToUpdate(version1);
  service_worker_client->AddServiceWorkerToUpdate(version2);
  service_worker_client->AddServiceWorkerToUpdate(version3);
  ExpectUpdateIsNotScheduled(version1.get());
  ExpectUpdateIsNotScheduled(version2.get());
  ExpectUpdateIsNotScheduled(version3.get());
  EXPECT_TRUE(HasVersionToUpdate(service_worker_client.get()));

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
  EXPECT_FALSE(HasVersionToUpdate(service_worker_client.get()));

  // Pretend the other page also finished for version3.
  version3->DecrementPendingUpdateHintCount();
  ExpectUpdateIsScheduled(version3.get());
}

}  // namespace content
