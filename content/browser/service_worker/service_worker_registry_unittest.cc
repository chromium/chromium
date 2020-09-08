// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registry.h"

#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "net/disk_cache/disk_cache.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace content {

namespace {

void FindCallback(base::OnceClosure quit_closure,
                  base::Optional<blink::ServiceWorkerStatusCode>* result,
                  scoped_refptr<ServiceWorkerRegistration>* found,
                  blink::ServiceWorkerStatusCode status,
                  scoped_refptr<ServiceWorkerRegistration> registration) {
  *result = status;
  *found = std::move(registration);
  std::move(quit_closure).Run();
}

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

}  // namespace

class ServiceWorkerRegistryTest : public testing::Test {
 public:
  ServiceWorkerRegistryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    CHECK(user_data_directory_.CreateUniqueTempDir());
    user_data_directory_path_ = user_data_directory_.GetPath();
    special_storage_policy_ =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    InitializeTestHelper();
  }

  void TearDown() override {
    helper_.reset();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }

  storage::MockSpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  void InitializeTestHelper() {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(
        user_data_directory_path_, special_storage_policy_.get());
  }

  void SimulateRestart() {
    // Need to reset |helper_| then wait for scheduled tasks to be finished
    // because |helper_| has TestBrowserContext and the dtor schedules storage
    // cleanup tasks.
    helper_.reset();
    base::RunLoop().RunUntilIdle();
    InitializeTestHelper();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForClientUrl(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForClientUrl(
        document_url, base::BindOnce(&FindCallback, loop.QuitClosure(), &result,
                                     registration));
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->StoreRegistration(
        registration.get(), version.get(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode GetAllRegistrationsInfos(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->GetAllRegistrationsInfos(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            const std::vector<ServiceWorkerRegistrationInfo>& infos) {
          result = status;
          *registrations = infos;
          loop.Quit();
        }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

 private:
  // |user_data_directory_| must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
};

TEST_F(ServiceWorkerRegistryTest, FindRegistration_LongestScopeMatch) {
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/".
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope1, kScript1,
                                                /*resource_id=*/1);

  // Registration for "/scope/foo".
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope2, kScript2,
                                                /*resource_id=*/2);

  // Registration for "/scope/foobar".
  const GURL kScope3("http://www.example.com/scope/foobar");
  const GURL kScript3("http://www.example.com/script3.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope3, kScript3,
                                                /*resource_id=*/3);

  // Notify storage of them being installed.
  registry()->NotifyInstallingRegistration(live_registration1.get());
  registry()->NotifyInstallingRegistration(live_registration2.get());
  registry()->NotifyInstallingRegistration(live_registration3.get());

  // Find a registration among installing ones.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
  found_registration = nullptr;

  // Store registrations.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration1,
                              live_registration1->waiting_version()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration2,
                              live_registration2->waiting_version()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration3,
                              live_registration3->waiting_version()));

  // Notify storage of installations no longer happening.
  registry()->NotifyDoneInstallingRegistration(
      live_registration1.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration2.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration3.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);

  // Find a registration among installed ones.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
}

// Tests that fields of ServiceWorkerRegistrationInfo are filled correctly.
TEST_F(ServiceWorkerRegistryTest, RegistrationInfoFields) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScript("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);

  // Set some fields to check ServiceWorkerStorage serializes/deserializes
  // these fields correctly.
  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader("header");

  registry()->NotifyInstallingRegistration(registration.get());
  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(GetAllRegistrationsInfos(&all_registrations),
            blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(all_registrations.size(), 1UL);

  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(info.scope, registration->scope());
  EXPECT_EQ(info.update_via_cache, registration->update_via_cache());
  EXPECT_EQ(info.registration_id, registration->id());
  EXPECT_EQ(info.navigation_preload_enabled,
            registration->navigation_preload_state().enabled);
  EXPECT_EQ(info.navigation_preload_header_length,
            registration->navigation_preload_state().header.size());
}

// Tests loading a registration with an enabled navigation preload state, as
// well as a custom header value.
TEST_F(ServiceWorkerRegistryTest, EnabledNavigationPreloadState) {
  const GURL kScope("https://valid.example.com/scope");
  const GURL kScript("https://valid.example.com/script.js");
  const std::string kHeaderValue("custom header value");

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration->SetActiveVersion(version);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader(kHeaderValue);

  ASSERT_EQ(StoreRegistration(registration, version),
            blink::ServiceWorkerStatusCode::kOk);

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(FindRegistrationForClientUrl(kScope, &found_registration),
            blink::ServiceWorkerStatusCode::kOk);
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_TRUE(registration_state.enabled);
  EXPECT_EQ(registration_state.header, kHeaderValue);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_TRUE(state.enabled);
  EXPECT_EQ(state.header, kHeaderValue);
}

// Tests that storage policy changes are observed.
TEST_F(ServiceWorkerRegistryTest, StoragePolicyChange) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScriptUrl("http://www.example.com/script.js");
  const auto kOrigin(url::Origin::Create(kScope));

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScriptUrl,
                                                /*resource_id=*/1);

  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(registry()->ShouldPurgeOnShutdown(kOrigin));

  {
    // Update storage policy to mark the origin should be purged on shutdown.
    special_storage_policy()->AddSessionOnly(kOrigin.GetURL());
    special_storage_policy()->NotifyPolicyChanged();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(registry()->ShouldPurgeOnShutdown(kOrigin));
}

class ServiceWorkerRegistryOriginTrialsTest : public ServiceWorkerRegistryTest {
 public:
  ServiceWorkerRegistryOriginTrialsTest() {
    blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
        [](blink::OriginTrialPolicy* policy) { return policy; },
        base::Unretained(&origin_trial_policy_)));
  }

  ~ServiceWorkerRegistryOriginTrialsTest() override {
    blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

 private:
  class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
   public:
    TestOriginTrialPolicy() {
      public_keys_.emplace_back(
          base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                            base::size(kTestPublicKey)));
    }

    bool IsOriginTrialsSupported() const override { return true; }

    std::vector<base::StringPiece> GetPublicKeys() const override {
      return public_keys_;
    }

    bool IsOriginSecure(const GURL& url) const override {
      return blink::network_utils::IsOriginSecure(url);
    }

   private:
    std::vector<base::StringPiece> public_keys_;
  };

  TestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(ServiceWorkerRegistryOriginTrialsTest, FromMainScript) {
  const GURL kScope("https://valid.example.com/scope");
  const GURL kScript("https://valid.example.com/script.js");
  const int64_t kRegistrationId = 1;
  const int64_t kVersionId = 1;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(options, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration.get(), kScript, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());

  network::mojom::URLResponseHead response_head;
  response_head.ssl_info = net::SSLInfo();
  response_head.ssl_info->cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  EXPECT_TRUE(response_head.ssl_info->is_valid());
  // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
  response_head.ssl_info->connection_status = 0x300039;

  const std::string kHTTPHeaderLine("HTTP/1.1 200 OK\n\n");
  const std::string kOriginTrial("Origin-Trial");
  // Token for Feature1 which expires 2033-05-18.
  // generate_token.py valid.example.com Feature1 --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature1Token(
      "AtiUXksymWhTv5ipBE7853JytiYb0RMj3wtEBjqu3PeufQPwV1oEaNjHt4R/oEBfcK0UiWlA"
      "P2b9BE2/eThqcAYAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
  // Token for Feature2 which expires 2033-05-18.
  // generate_token.py valid.example.com Feature2 --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature2Token1(
      "ApmHVC6Dpez0KQNBy13o6cGuoB5AgzOLN0keQMyAN5mjebCwR0MA8/IyjKQIlyom2RuJVg/u"
      "LmnqEpldfewkbA8AAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
  // Token for Feature2 which expires 2036-07-18.
  // generate_token.py valid.example.com Feature2 --expire-timestamp=2100000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature2Token2(
      "AmV2SSxrYstE2zSwZToy7brAbIJakd146apC/6+VDflLmc5yDfJlHGILe5+ZynlcliG7clOR"
      "fHhXCzS5Lh1v4AAAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMiIsICJleHBpcnkiOiAyMTAwMDAwMDAwfQ==");
  // Token for Feature3 which expired 2001-09-09.
  // generate_token.py valid.example.com Feature3 --expire-timestamp=1000000000
  const std::string kFeature3ExpiredToken(
      "AtSAc03z4qvid34W4MHMxyRFUJKlubZ+P5cs5yg6EiBWcagVbnm5uBgJMJN34pag7D5RywGV"
      "ol2RFf+4Sdm1hQ4AAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMyIsICJleHBpcnkiOiAxMDAwMDAwMDAwfQ==");
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head.headers->AddHeader(kOriginTrial, kFeature1Token);
  response_head.headers->AddHeader(kOriginTrial, kFeature2Token1);
  response_head.headers->AddHeader(kOriginTrial, kFeature2Token2);
  response_head.headers->AddHeader(kOriginTrial, kFeature3ExpiredToken);
  version->SetMainScriptResponse(
      std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
          response_head));
  ASSERT_TRUE(version->origin_trial_tokens());
  const blink::TrialTokenValidator::FeatureToTokensMap& tokens =
      *version->origin_trial_tokens();
  ASSERT_EQ(2UL, tokens.size());
  ASSERT_EQ(1UL, tokens.at("Feature1").size());
  EXPECT_EQ(kFeature1Token, tokens.at("Feature1")[0]);
  ASSERT_EQ(2UL, tokens.at("Feature2").size());
  EXPECT_EQ(kFeature2Token1, tokens.at("Feature2")[0]);
  EXPECT_EQ(kFeature2Token2, tokens.at("Feature2")[1]);

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(
      storage::mojom::ServiceWorkerResourceRecord::New(1, kScript, 100));
  version->script_cache_map()->SetResources(records);
  version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetActiveVersion(version);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(registration, version));
  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, &found_registration));
  ASSERT_TRUE(found_registration->active_version());
  const blink::TrialTokenValidator::FeatureToTokensMap& found_tokens =
      *found_registration->active_version()->origin_trial_tokens();
  ASSERT_EQ(2UL, found_tokens.size());
  ASSERT_EQ(1UL, found_tokens.at("Feature1").size());
  EXPECT_EQ(kFeature1Token, found_tokens.at("Feature1")[0]);
  ASSERT_EQ(2UL, found_tokens.at("Feature2").size());
  EXPECT_EQ(kFeature2Token1, found_tokens.at("Feature2")[0]);
  EXPECT_EQ(kFeature2Token2, found_tokens.at("Feature2")[1]);
}

}  // namespace content
