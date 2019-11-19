// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/cookie_store/cookie_store_context.h"
#include "content/browser/cookie_store/cookie_store_manager.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Synchronous proxies to a wrapped CookieStore service's methods.
class CookieStoreSync {
 public:
  using Subscriptions = std::vector<blink::mojom::CookieChangeSubscriptionPtr>;

  // The caller must ensure that the CookieStore service outlives this.
  explicit CookieStoreSync(blink::mojom::CookieStore* cookie_store_service)
      : cookie_store_service_(cookie_store_service) {}
  ~CookieStoreSync() = default;

  bool AppendSubscriptions(int64_t service_worker_registration_id,
                           Subscriptions subscriptions) {
    bool success;
    base::RunLoop run_loop;
    cookie_store_service_->AppendSubscriptions(
        service_worker_registration_id, std::move(subscriptions),
        base::BindLambdaForTesting([&](bool service_success) {
          success = service_success;
          run_loop.Quit();
        }));
    run_loop.Run();
    return success;
  }

  base::Optional<Subscriptions> GetSubscriptions(
      int64_t service_worker_registration_id) {
    base::Optional<Subscriptions> result;
    base::RunLoop run_loop;
    cookie_store_service_->GetSubscriptions(
        service_worker_registration_id,
        base::BindLambdaForTesting(
            [&](Subscriptions service_result, bool service_success) {
              if (service_success)
                result = std::move(service_result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

 private:
  blink::mojom::CookieStore* cookie_store_service_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreSync);
};

const char kExampleScope[] = "https://example.com/a";
const char kExampleWorkerScript[] = "https://example.com/a/script.js";
const char kGoogleScope[] = "https://google.com/a";
const char kGoogleWorkerScript[] = "https://google.com/a/script.js";
const char kLegacyScope[] = "https://legacy.com/a";
const char kLegacyWorkerScript[] = "https://legacy.com/a/script.js";

// Mocks a service worker that uses the cookieStore API.
class CookieStoreWorkerTestHelper : public EmbeddedWorkerTestHelper {
 public:
  using EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper;

  explicit CookieStoreWorkerTestHelper(
      const base::FilePath& user_data_directory)
      : EmbeddedWorkerTestHelper(user_data_directory) {}
  ~CookieStoreWorkerTestHelper() override = default;

  class EmbeddedWorkerInstanceClient : public FakeEmbeddedWorkerInstanceClient {
   public:
    explicit EmbeddedWorkerInstanceClient(
        CookieStoreWorkerTestHelper* worker_helper)
        : FakeEmbeddedWorkerInstanceClient(worker_helper),
          worker_helper_(worker_helper) {}
    ~EmbeddedWorkerInstanceClient() override = default;

    // Collects the worker's registration ID for OnInstallEvent().
    void StartWorker(
        blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
      ServiceWorkerVersion* service_worker_version =
          worker_helper_->context()->GetLiveVersion(
              params->service_worker_version_id);
      DCHECK(service_worker_version);
      worker_helper_->service_worker_registration_id_ =
          service_worker_version->registration_id();

      FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(params));
    }

   private:
    CookieStoreWorkerTestHelper* const worker_helper_;

    DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClient);
  };

  class ServiceWorker : public FakeServiceWorker {
   public:
    explicit ServiceWorker(CookieStoreWorkerTestHelper* worker_helper)
        : FakeServiceWorker(worker_helper), worker_helper_(worker_helper) {}
    ~ServiceWorker() override = default;

    // Cookie change subscriptions can only be created in this event handler.
    void DispatchInstallEvent(DispatchInstallEventCallback callback) override {
      for (auto& subscriptions :
           worker_helper_->install_subscription_batches_) {
        worker_helper_->cookie_store_service_->AppendSubscriptions(
            worker_helper_->service_worker_registration_id_,
            std::move(subscriptions),
            base::BindOnce(
                [](bool expect_success, bool success) {
                  EXPECT_EQ(expect_success, success)
                      << "AppendSubscriptions wrong result";
                },
                worker_helper_->expect_subscription_success_));
      }
      worker_helper_->install_subscription_batches_.clear();

      FakeServiceWorker::DispatchInstallEvent(std::move(callback));
    }

    // Used to implement WaitForActivateEvent().
    void DispatchActivateEvent(
        DispatchActivateEventCallback callback) override {
      if (worker_helper_->quit_on_activate_) {
        worker_helper_->quit_on_activate_->Quit();
        worker_helper_->quit_on_activate_ = nullptr;
      }

      FakeServiceWorker::DispatchActivateEvent(std::move(callback));
    }

    void DispatchCookieChangeEvent(
        const net::CookieChangeInfo& change,
        DispatchCookieChangeEventCallback callback) override {
      worker_helper_->changes_.emplace_back(change);
      std::move(callback).Run(
          blink::mojom::ServiceWorkerEventStatus::COMPLETED);
    }

   private:
    CookieStoreWorkerTestHelper* const worker_helper_;

    DISALLOW_COPY_AND_ASSIGN(ServiceWorker);
  };

  std::unique_ptr<FakeEmbeddedWorkerInstanceClient> CreateInstanceClient()
      override {
    return std::make_unique<EmbeddedWorkerInstanceClient>(this);
  }

  std::unique_ptr<FakeServiceWorker> CreateServiceWorker() override {
    return std::make_unique<ServiceWorker>(this);
  }

  // Sets the cookie change subscriptions requested in the next install event.
  void SetOnInstallSubscriptions(
      std::vector<CookieStoreSync::Subscriptions> subscription_batches,
      blink::mojom::CookieStore* cookie_store_service,
      bool expect_subscription_success = true) {
    install_subscription_batches_ = std::move(subscription_batches);
    cookie_store_service_ = cookie_store_service;
    expect_subscription_success_ = expect_subscription_success;
  }

  // Spins inside a run loop until a service worker activate event is received.
  void WaitForActivateEvent() {
    base::RunLoop run_loop;
    quit_on_activate_ = &run_loop;
    run_loop.Run();
  }

  // The data in the CookieChangeEvents received by the worker.
  std::vector<net::CookieChangeInfo>& changes() { return changes_; }

 private:
  // Used to add cookie change subscriptions during OnInstallEvent().
  blink::mojom::CookieStore* cookie_store_service_ = nullptr;
  std::vector<CookieStoreSync::Subscriptions> install_subscription_batches_;
  bool expect_subscription_success_ = true;
  int64_t service_worker_registration_id_;

  // Set by WaitForActivateEvent(), used in OnActivateEvent().
  base::RunLoop* quit_on_activate_ = nullptr;

  // Collects the changes reported to OnCookieChangeEvent().
  std::vector<net::CookieChangeInfo> changes_;
};

}  // namespace

// This class cannot be in an anonymous namespace because it needs to be a
// friend of StoragePartitionImpl, to access its constructor.
class CookieStoreManagerTest
    : public testing::Test,
      public testing::WithParamInterface<bool /* reset_context */> {
 public:
  CookieStoreManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {
    // Enable SameSiteByDefaultCookies because the default CookieAccessSemantics
    // setting is based on the state of this feature, and we want a consistent
    // expected value in the tests for domains without a custom setting.
    feature_list_.InitAndEnableFeature(
        net::features::kSameSiteByDefaultCookies);
  }

  void SetUp() override {
    // Use an on-disk service worker storage to test saving and loading.
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());

    ResetServiceWorkerContext();
  }

  void TearDown() override {
    // Let the service worker context cleanly shut down, so its storage can be
    // safely opened again if the test will continue.
    if (worker_test_helper_)
      worker_test_helper_->ShutdownContext();

    task_environment_.RunUntilIdle();

    // Smart pointers are reset manually in destruction order because this is
    // called by ResetServiceWorkerContext().
    example_service_.reset();
    google_service_.reset();
    legacy_service_.reset();
    example_service_remote_.reset();
    google_service_remote_.reset();
    legacy_service_remote_.reset();
    cookie_manager_.reset();
    cookie_store_context_ = nullptr;
    storage_partition_impl_.reset();
    worker_test_helper_.reset();
  }

  void ResetServiceWorkerContext() {
    if (cookie_store_context_)
      TearDown();

    worker_test_helper_ = std::make_unique<CookieStoreWorkerTestHelper>(
        user_data_directory_.GetPath());
    cookie_store_context_ = base::MakeRefCounted<CookieStoreContext>();
    cookie_store_context_->Initialize(worker_test_helper_->context_wrapper(),
                                      base::BindOnce([](bool success) {
                                        CHECK(success) << "Initialize failed";
                                      }));
    storage_partition_impl_ = StoragePartitionImpl::Create(
        worker_test_helper_->browser_context(), true /* in_memory */,
        base::FilePath() /* relative_partition_path */,
        std::string() /* partition_domain */);
    storage_partition_impl_->Initialize();
    ::network::mojom::NetworkContext* network_context =
        storage_partition_impl_->GetNetworkContext();
    cookie_store_context_->ListenToCookieChanges(
        network_context, base::BindOnce([](bool success) {
          CHECK(success) << "ListenToCookieChanges failed";
        }));
    network_context->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());

    cookie_store_context_->CreateServiceForTesting(
        url::Origin::Create(GURL(kExampleScope)),
        example_service_remote_.BindNewPipeAndPassReceiver());
    example_service_ =
        std::make_unique<CookieStoreSync>(example_service_remote_.get());

    cookie_store_context_->CreateServiceForTesting(
        url::Origin::Create(GURL(kGoogleScope)),
        google_service_remote_.BindNewPipeAndPassReceiver());
    google_service_ =
        std::make_unique<CookieStoreSync>(google_service_remote_.get());

    cookie_store_context_->CreateServiceForTesting(
        url::Origin::Create(GURL(kLegacyScope)),
        legacy_service_remote_.BindNewPipeAndPassReceiver());
    legacy_service_ =
        std::make_unique<CookieStoreSync>(legacy_service_remote_.get());

    // Set Legacy cookie access setting for legacy.com to test
    // CookieAccessSemantics.
    std::vector<ContentSettingPatternSource> legacy_settings;
    legacy_settings.emplace_back(
        ContentSettingsPattern::FromString("[*.]legacy.com"),
        ContentSettingsPattern::FromString("*"),
        base::Value(ContentSetting::CONTENT_SETTING_ALLOW), std::string(),
        false /* incognito */);
    cookie_manager_->SetContentSettingsForLegacyCookieAccess(
        std::move(legacy_settings));
    cookie_manager_.FlushForTesting();
  }

  int64_t RegisterServiceWorker(const char* scope, const char* script_url) {
    bool success = false;
    int64_t registration_id;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL(scope);
    base::RunLoop run_loop;
    worker_test_helper_->context()->RegisterServiceWorker(
        GURL(script_url), options,
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status,
                                       const std::string& status_message,
                                       int64_t service_worker_registration_id) {
          success = (status == blink::ServiceWorkerStatusCode::kOk);
          registration_id = service_worker_registration_id;
          EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
              << blink::ServiceWorkerStatusToString(status);
          run_loop.Quit();
        }));
    run_loop.Run();
    if (!success)
      return kInvalidRegistrationId;

    worker_test_helper_->WaitForActivateEvent();
    return registration_id;
  }

  // Synchronous helper for CookieManager::SetCanonicalCookie.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie) {
    base::RunLoop run_loop;
    bool success = false;
    cookie_manager_->SetCanonicalCookie(
        cookie, "https", net::CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting(
            [&](net::CanonicalCookie::CookieInclusionStatus service_status) {
              success = service_status.IsInclude();
              run_loop.Quit();
            }));
    run_loop.Run();
    return success;
  }

  // Simplified helper for SetCanonicalCookie.
  //
  // Creates a CanonicalCookie that is not secure, not http-only,
  // and not restricted to first parties. Returns false if creation fails.
  bool SetSessionCookie(const char* name,
                        const char* value,
                        const char* domain,
                        const char* path) {
    return SetCanonicalCookie(net::CanonicalCookie(
        name, value, domain, path, base::Time(), base::Time(), base::Time(),
        /* secure = */ true,
        /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
        net::COOKIE_PRIORITY_DEFAULT));
  }

  bool reset_context_during_test() const { return GetParam(); }

  static constexpr const int64_t kInvalidRegistrationId = -1;

 protected:
  BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir user_data_directory_;
  std::unique_ptr<CookieStoreWorkerTestHelper> worker_test_helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  scoped_refptr<CookieStoreContext> cookie_store_context_;
  mojo::Remote<::network::mojom::CookieManager> cookie_manager_;

  mojo::Remote<blink::mojom::CookieStore> example_service_remote_,
      google_service_remote_, legacy_service_remote_;
  std::unique_ptr<CookieStoreSync> example_service_, google_service_,
      legacy_service_;
};

const int64_t CookieStoreManagerTest::kInvalidRegistrationId;

namespace {

// Useful for sorting a vector of cookie change subscriptions.
bool CookieChangeSubscriptionLessThan(
    const blink::mojom::CookieChangeSubscriptionPtr& lhs,
    const blink::mojom::CookieChangeSubscriptionPtr& rhs) {
  return std::tie(lhs->name, lhs->match_type, lhs->url) <
         std::tie(rhs->name, rhs->match_type, rhs->url);
}

TEST_P(CookieStoreManagerTest, NoSubscriptions) {
  worker_test_helper_->SetOnInstallSubscriptions(
      std::vector<CookieStoreSync::Subscriptions>(),
      example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(0u, all_subscriptions_opt.value().size());
}

TEST_P(CookieStoreManagerTest, EmptySubscriptions) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();
  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(0u, all_subscriptions_opt.value().size());
}

TEST_P(CookieStoreManagerTest, OneSubscription) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie_name_prefix";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  CookieStoreSync::Subscriptions all_subscriptions =
      std::move(all_subscriptions_opt).value();
  EXPECT_EQ(1u, all_subscriptions.size());
  EXPECT_EQ("cookie_name_prefix", all_subscriptions[0]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::STARTS_WITH,
            all_subscriptions[0]->match_type);
  EXPECT_EQ(GURL(kExampleScope), all_subscriptions[0]->url);
}

TEST_P(CookieStoreManagerTest, WrongDomainSubscription) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kGoogleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get(),
                                                 false /* expecting failure */);
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  ASSERT_TRUE(
      SetSessionCookie("cookie-name", "cookie-value", "google.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(0u, worker_test_helper_->changes().size());
}

TEST_P(CookieStoreManagerTest, AppendSubscriptionsAfterEmptyInstall) {
  worker_test_helper_->SetOnInstallSubscriptions(
      std::vector<CookieStoreSync::Subscriptions>(),
      example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  CookieStoreSync::Subscriptions subscriptions;
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie_name_prefix";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  EXPECT_FALSE(example_service_->AppendSubscriptions(registration_id,
                                                     std::move(subscriptions)));

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(0u, all_subscriptions_opt.value().size());
}

TEST_P(CookieStoreManagerTest, AppendSubscriptionsAfterInstall) {
  {
    std::vector<CookieStoreSync::Subscriptions> batches;
    batches.emplace_back();

    CookieStoreSync::Subscriptions& subscriptions = batches.back();
    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "cookie_name_prefix";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::STARTS_WITH;
    subscriptions.back()->url = GURL(kExampleScope);

    worker_test_helper_->SetOnInstallSubscriptions(
        std::move(batches), example_service_remote_.get());
  }
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  {
    CookieStoreSync::Subscriptions subscriptions;
    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "cookie_name";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::EQUALS;
    subscriptions.back()->url = GURL(kExampleScope);

    EXPECT_FALSE(example_service_->AppendSubscriptions(
        registration_id, std::move(subscriptions)));
  }

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  CookieStoreSync::Subscriptions all_subscriptions =
      std::move(all_subscriptions_opt).value();
  EXPECT_EQ(1u, all_subscriptions.size());
  EXPECT_EQ("cookie_name_prefix", all_subscriptions[0]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::STARTS_WITH,
            all_subscriptions[0]->match_type);
  EXPECT_EQ(GURL(kExampleScope), all_subscriptions[0]->url);
}

TEST_P(CookieStoreManagerTest, AppendSubscriptionsFromWrongOrigin) {
  worker_test_helper_->SetOnInstallSubscriptions(
      std::vector<CookieStoreSync::Subscriptions>(),
      example_service_remote_.get());
  int64_t example_registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(example_registration_id, kInvalidRegistrationId);

  CookieStoreSync::Subscriptions subscriptions;
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie_name_prefix";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  EXPECT_FALSE(google_service_->AppendSubscriptions(example_registration_id,
                                                    std::move(subscriptions)));

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(example_registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(0u, all_subscriptions_opt.value().size());
}

TEST_P(CookieStoreManagerTest, AppendSubscriptionsInvalidRegistrationId) {
  worker_test_helper_->SetOnInstallSubscriptions(
      std::vector<CookieStoreSync::Subscriptions>(),
      example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  CookieStoreSync::Subscriptions subscriptions;
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie_name_prefix";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  EXPECT_FALSE(example_service_->AppendSubscriptions(registration_id + 100,
                                                     std::move(subscriptions)));

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(0u, all_subscriptions_opt.value().size());
}

TEST_P(CookieStoreManagerTest, MultiWorkerSubscriptions) {
  {
    std::vector<CookieStoreSync::Subscriptions> batches;
    batches.emplace_back();

    CookieStoreSync::Subscriptions& subscriptions = batches.back();
    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "cookie_name_prefix";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::STARTS_WITH;
    subscriptions.back()->url = GURL(kExampleScope);

    worker_test_helper_->SetOnInstallSubscriptions(
        std::move(batches), example_service_remote_.get());
  }
  int64_t example_registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(example_registration_id, kInvalidRegistrationId);

  {
    std::vector<CookieStoreSync::Subscriptions> batches;
    batches.emplace_back();

    CookieStoreSync::Subscriptions& subscriptions = batches.back();
    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "cookie_name";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::EQUALS;
    subscriptions.back()->url = GURL(kGoogleScope);

    worker_test_helper_->SetOnInstallSubscriptions(
        std::move(batches), google_service_remote_.get());
  }
  int64_t google_registration_id =
      RegisterServiceWorker(kGoogleScope, kGoogleWorkerScript);
  ASSERT_NE(google_registration_id, kInvalidRegistrationId);
  EXPECT_NE(example_registration_id, google_registration_id);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> example_subscriptions_opt =
      example_service_->GetSubscriptions(example_registration_id);
  ASSERT_TRUE(example_subscriptions_opt.has_value());
  CookieStoreSync::Subscriptions example_subscriptions =
      std::move(example_subscriptions_opt).value();
  EXPECT_EQ(1u, example_subscriptions.size());
  EXPECT_EQ("cookie_name_prefix", example_subscriptions[0]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::STARTS_WITH,
            example_subscriptions[0]->match_type);
  EXPECT_EQ(GURL(kExampleScope), example_subscriptions[0]->url);

  base::Optional<CookieStoreSync::Subscriptions> google_subscriptions_opt =
      google_service_->GetSubscriptions(google_registration_id);
  ASSERT_TRUE(google_subscriptions_opt.has_value());
  CookieStoreSync::Subscriptions google_subscriptions =
      std::move(google_subscriptions_opt).value();
  EXPECT_EQ(1u, google_subscriptions.size());
  EXPECT_EQ("cookie_name", google_subscriptions[0]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::EQUALS,
            google_subscriptions[0]->match_type);
  EXPECT_EQ(GURL(kGoogleScope), google_subscriptions[0]->url);
}

TEST_P(CookieStoreManagerTest, MultipleSubscriptions) {
  std::vector<CookieStoreSync::Subscriptions> batches;

  {
    batches.emplace_back();
    CookieStoreSync::Subscriptions& subscriptions = batches.back();

    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "name1";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::STARTS_WITH;
    subscriptions.back()->url = GURL("https://example.com/a/1");
    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "name2";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::EQUALS;
    subscriptions.back()->url = GURL("https://example.com/a/2");
  }

  batches.emplace_back();

  {
    batches.emplace_back();
    CookieStoreSync::Subscriptions& subscriptions = batches.back();

    subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
    subscriptions.back()->name = "name3";
    subscriptions.back()->match_type =
        ::network::mojom::CookieMatchType::STARTS_WITH;
    subscriptions.back()->url = GURL("https://example.com/a/3");
  }

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  CookieStoreSync::Subscriptions all_subscriptions =
      std::move(all_subscriptions_opt).value();

  std::sort(all_subscriptions.begin(), all_subscriptions.end(),
            CookieChangeSubscriptionLessThan);

  EXPECT_EQ(3u, all_subscriptions.size());
  EXPECT_EQ("name1", all_subscriptions[0]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::STARTS_WITH,
            all_subscriptions[0]->match_type);
  EXPECT_EQ(GURL("https://example.com/a/1"), all_subscriptions[0]->url);
  EXPECT_EQ("name2", all_subscriptions[1]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::EQUALS,
            all_subscriptions[1]->match_type);
  EXPECT_EQ(GURL("https://example.com/a/2"), all_subscriptions[1]->url);
  EXPECT_EQ("name3", all_subscriptions[2]->name);
  EXPECT_EQ(::network::mojom::CookieMatchType::STARTS_WITH,
            all_subscriptions[2]->match_type);
  EXPECT_EQ(GURL("https://example.com/a/3"), all_subscriptions[2]->url);
}

TEST_P(CookieStoreManagerTest, OneCookieChange) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name", "cookie-value", "example.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

// Same as above except this tests that the LEGACY access semantics for
// legacy.com cookies is correctly reflected in the change info.
TEST_P(CookieStoreManagerTest, OneCookieChangeLegacy) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kLegacyScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 legacy_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kLegacyScope, kLegacyWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      legacy_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name", "cookie-value", "legacy.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

TEST_P(CookieStoreManagerTest, CookieChangeNameStartsWith) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie-name-2";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name-1", "cookie-value-1", "example.com", "/"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-2", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-2", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(SetSessionCookie("cookie-name-22", "cookie-value-22",
                               "example.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-22", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-22",
            worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

// Same as above except this tests that the LEGACY access semantics for
// legacy.com cookies is correctly reflected in the change info.
TEST_P(CookieStoreManagerTest, CookieChangeNameStartsWithLegacy) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie-name-2";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kLegacyScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 legacy_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kLegacyScope, kLegacyWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      legacy_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name-1", "cookie-value-1", "legacy.com", "/"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-2", "cookie-value-2", "legacy.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-2", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-2", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-22", "cookie-value-22", "legacy.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-22", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-22",
            worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

TEST_P(CookieStoreManagerTest, CookieChangeUrl) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name-1", "cookie-value-1", "google.com", "/"));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com",
                               "/a/subpath"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-3", "cookie-value-3", "example.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-3", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-3", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-4", "cookie-value-4", "example.com", "/a"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-4", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-4", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/a", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

// Same as above except this tests that the LEGACY access semantics for
// legacy.com cookies is correctly reflected in the change info.
TEST_P(CookieStoreManagerTest, CookieChangeUrlLegacy) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kLegacyScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 legacy_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kLegacyScope, kLegacyWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      legacy_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(
      SetSessionCookie("cookie-name-1", "cookie-value-1", "google.com", "/"));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(SetSessionCookie("cookie-name-2", "cookie-value-2", "legacy.com",
                               "/a/subpath"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-3", "cookie-value-3", "legacy.com", "/"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-3", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-3", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(
      SetSessionCookie("cookie-name-4", "cookie-value-4", "legacy.com", "/a"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-4", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-4", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/a", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

TEST_P(CookieStoreManagerTest, HttpOnlyCookieChange) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(SetCanonicalCookie(net::CanonicalCookie(
      "cookie-name-1", "cookie-value-1", "example.com", "/", base::Time(),
      base::Time(), base::Time(),
      /* secure = */ true,
      /* httponly = */ true, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(SetCanonicalCookie(net::CanonicalCookie(
      "cookie-name-2", "cookie-value-2", "example.com", "/", base::Time(),
      base::Time(), base::Time(),
      /* secure = */ true,
      /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-2", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-2", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("example.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // example.com does not have a custom access semantics setting, so it defaults
  // to NONLEGACY, because the FeatureList has SameSiteByDefaultCookies enabled.
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

// Same as above except this tests that the LEGACY access semantics for
// legacy.com cookies is correctly reflected in the change info.
TEST_P(CookieStoreManagerTest, HttpOnlyCookieChangeLegacy) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kLegacyScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 legacy_service_remote_.get());
  int64_t registration_id =
      RegisterServiceWorker(kLegacyScope, kLegacyWorkerScript);
  ASSERT_NE(registration_id, kInvalidRegistrationId);

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      legacy_service_->GetSubscriptions(registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  ASSERT_EQ(1u, all_subscriptions_opt.value().size());

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  ASSERT_TRUE(SetCanonicalCookie(net::CanonicalCookie(
      "cookie-name-1", "cookie-value-1", "legacy.com", "/", base::Time(),
      base::Time(), base::Time(),
      /* secure = */ false,
      /* httponly = */ true, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, worker_test_helper_->changes().size());

  worker_test_helper_->changes().clear();
  ASSERT_TRUE(SetCanonicalCookie(net::CanonicalCookie(
      "cookie-name-2", "cookie-value-2", "legacy.com", "/", base::Time(),
      base::Time(), base::Time(),
      /* secure = */ false,
      /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, worker_test_helper_->changes().size());
  EXPECT_EQ("cookie-name-2", worker_test_helper_->changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value-2", worker_test_helper_->changes()[0].cookie.Value());
  EXPECT_EQ("legacy.com", worker_test_helper_->changes()[0].cookie.Domain());
  EXPECT_EQ("/", worker_test_helper_->changes()[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            worker_test_helper_->changes()[0].cause);
  // legacy.com has a custom Legacy setting.
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            worker_test_helper_->changes()[0].access_semantics);
}

TEST_P(CookieStoreManagerTest, GetSubscriptionsFromWrongOrigin) {
  std::vector<CookieStoreSync::Subscriptions> batches;
  batches.emplace_back();

  CookieStoreSync::Subscriptions& subscriptions = batches.back();
  subscriptions.emplace_back(blink::mojom::CookieChangeSubscription::New());
  subscriptions.back()->name = "cookie_name_prefix";
  subscriptions.back()->match_type =
      ::network::mojom::CookieMatchType::STARTS_WITH;
  subscriptions.back()->url = GURL(kExampleScope);

  worker_test_helper_->SetOnInstallSubscriptions(std::move(batches),
                                                 example_service_remote_.get());
  int64_t example_registration_id =
      RegisterServiceWorker(kExampleScope, kExampleWorkerScript);
  ASSERT_NE(example_registration_id, kInvalidRegistrationId);

  if (reset_context_during_test())
    ResetServiceWorkerContext();

  base::Optional<CookieStoreSync::Subscriptions> all_subscriptions_opt =
      example_service_->GetSubscriptions(example_registration_id);
  ASSERT_TRUE(all_subscriptions_opt.has_value());
  EXPECT_EQ(1u, all_subscriptions_opt.value().size());

  base::Optional<CookieStoreSync::Subscriptions> wrong_subscriptions_opt =
      google_service_->GetSubscriptions(example_registration_id);
  EXPECT_FALSE(wrong_subscriptions_opt.has_value());
}

INSTANTIATE_TEST_SUITE_P(,
                         CookieStoreManagerTest,
                         testing::Bool() /* reset_storage_during_test */);

}  // namespace

}  // namespace content
