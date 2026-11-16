// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import <optional>
#import <string>

#import "base/base64.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/escape.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "google_apis/google_api_keys.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/http_status_code.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Base64-encoded dummy OHTTP key configs from the RFC 9458 Appendix A example,
// needed to pass OhttpKeyService's key config parsing.
constexpr char kEncodedTestOhttpKey[] =
    "AQAgMeHwWnQBAhFSIOmvkY9zhnSuyV9U224E63Baro55gVUACAABAAEAAQAD";
constexpr char kRotatedEncodedTestOhttpKey[] =
    "AgAgMeHwWnQBAhFSIOmvkY9zhnSuyV9U224E63Baro55gVUACAABAAEAAQAD";

GURL GetTestKeyFetchUrl() {
  GURL key_fetch_url(
      safe_browsing::kHashPrefixRealTimeLookupsKeyFetchUrl.Get());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    key_fetch_url = key_fetch_url.Resolve(
        "?key=" + base::EscapeQueryParamValue(api_key, /*use_plus=*/true));
  }
  return key_fetch_url;
}

}  // namespace

class OhttpKeyServiceFactoryTest : public PlatformTest {
 protected:
  OhttpKeyServiceFactoryTest() = default;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

class OhttpKeyServiceFactoryIncognitoLookupTest
    : public OhttpKeyServiceFactoryTest,
      public testing::WithParamInterface<bool> {
 protected:
  OhttpKeyServiceFactoryIncognitoLookupTest()
      : fake_safe_browsing_service_(
            base::MakeRefCounted<FakeSafeBrowsingService>()) {
    scoped_feature_list_.InitWithFeatureState(
        safe_browsing::kHashPrefixRealTimeLookupsOnIncognitoAndGuest,
        GetParam());
    TestingApplicationContext::GetGlobal()->SetSafeBrowsingService(
        fake_safe_browsing_service_.get());
  }

  ~OhttpKeyServiceFactoryIncognitoLookupTest() override {
    TestingApplicationContext::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

  safe_browsing::hash_realtime_utils::GoogleChromeBrandingPretenderForTesting
      apply_branding_;
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<FakeSafeBrowsingService> fake_safe_browsing_service_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OhttpKeyServiceFactoryIncognitoLookupTest,
                         testing::Bool());

// Checks that OhttpKeyServiceFactory returns a non-null instance for a regular
// profile, and returns a separate non-null instance for an off-the-record
// profile iff kHashPrefixRealTimeLookupsOnIncognitoAndGuest is enabled.
TEST_P(OhttpKeyServiceFactoryIncognitoLookupTest, GetForProfile) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(OhttpKeyServiceFactory::GetInstance(),
                            OhttpKeyServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  safe_browsing::OhttpKeyService* regular_service =
      OhttpKeyServiceFactory::GetForProfile(profile.get());
  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(regular_service);

  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              OhttpKeyServiceFactory::GetInstance(),
              OhttpKeyServiceFactory::GetDefaultFactory()}});
  safe_browsing::OhttpKeyService* incognito_service =
      OhttpKeyServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(GetParam(), incognito_service != nullptr);
  EXPECT_NE(regular_service, incognito_service);
}

// Verifies that on iOS, the Incognito OhttpKeyService instance reuses the
// regular profile's cached OHTTP key via OverlayUserPrefStore without issuing a
// network request, and keeps any Incognito key fetches or 422 rotations in
// memory without modifying the regular profile's prefs.
TEST_P(OhttpKeyServiceFactoryIncognitoLookupTest,
       IncognitoReusesRegularProfilePrefAndKeepsFetchesInMemory) {
  std::string initial_ohttp_key;
  ASSERT_TRUE(base::Base64Decode(kEncodedTestOhttpKey, &initial_ohttp_key));

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(OhttpKeyServiceFactory::GetInstance(),
                            OhttpKeyServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  safe_browsing::OhttpKeyService* regular_service =
      OhttpKeyServiceFactory::GetForProfile(profile.get());
  ASSERT_NE(nullptr, regular_service);

  // Populate the regular profile's OHTTP key and prefs end-to-end via a network
  // fetch before creating the off-the-record profile.
  network::TestURLLoaderFactory* test_url_loader_factory =
      fake_safe_browsing_service_->GetTestURLLoaderFactory();
  test_url_loader_factory->AddResponse(GetTestKeyFetchUrl().spec(),
                                       initial_ohttp_key);
  base::test::TestFuture<std::optional<std::string>> regular_init_future;
  regular_service->GetOhttpKey(regular_init_future.GetCallback());
  EXPECT_EQ(regular_init_future.Get(), initial_ohttp_key);
  test_url_loader_factory->ClearResponses();

  PrefService* regular_prefs = profile->GetPrefs();
  ASSERT_EQ(regular_prefs->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
  base::Time initial_expiration = regular_prefs->GetTime(
      prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime);
  size_t requests_before_incognito = test_url_loader_factory->total_requests();

  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              OhttpKeyServiceFactory::GetInstance(),
              OhttpKeyServiceFactory::GetDefaultFactory()}});
  safe_browsing::OhttpKeyService* incognito_service =
      OhttpKeyServiceFactory::GetForProfile(otr_profile);
  if (!GetParam()) {
    EXPECT_EQ(nullptr, incognito_service);
    return;
  }

  ASSERT_NE(nullptr, incognito_service);
  EXPECT_NE(regular_service, incognito_service);

  // Flush the posted MaybeStartOrRescheduleAsyncFetch task from OhttpKeyService
  // construction and verify that creating the off-the-record profile populated
  // its key from the regular profile's prefs via OverlayUserPrefStore without
  // issuing a network request.
  base::test::TestFuture<void> flush_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  ASSERT_TRUE(flush_future.Wait());
  EXPECT_EQ(test_url_loader_factory->total_requests(),
            requests_before_incognito);

  base::test::TestFuture<std::optional<std::string>> initial_key_future;
  incognito_service->GetOhttpKey(initial_key_future.GetCallback());
  EXPECT_EQ(initial_key_future.Get(), initial_ohttp_key);
  EXPECT_EQ(test_url_loader_factory->total_requests(),
            requests_before_incognito);

  // Simulate a 422 key-related HTTP error during an Incognito lookup, which
  // clears the Incognito service's key and requires fetching a new key.
  incognito_service->NotifyLookupResponse(initial_ohttp_key,
                                          net::HTTP_UNPROCESSABLE_CONTENT);
  EXPECT_FALSE(incognito_service->get_ohttp_key_for_testing().has_value());
  EXPECT_TRUE(regular_service->get_ohttp_key_for_testing().has_value());

  std::string rotated_ohttp_key;
  ASSERT_TRUE(
      base::Base64Decode(kRotatedEncodedTestOhttpKey, &rotated_ohttp_key));
  base::test::TestFuture<std::optional<std::string>> rotated_key_future;
  incognito_service->GetOhttpKey(rotated_key_future.GetCallback());
  test_url_loader_factory->AddResponse(GetTestKeyFetchUrl().spec(),
                                       rotated_ohttp_key);
  EXPECT_EQ(rotated_key_future.Get(), rotated_ohttp_key);
  EXPECT_EQ(test_url_loader_factory->total_requests(),
            requests_before_incognito + 1u);

  // Subsequent Incognito lookups continue using the newly fetched key from
  // memory without issuing another network request.
  base::test::TestFuture<std::optional<std::string>> cached_rotated_key_future;
  incognito_service->GetOhttpKey(cached_rotated_key_future.GetCallback());
  EXPECT_EQ(cached_rotated_key_future.Get(), rotated_ohttp_key);
  EXPECT_EQ(test_url_loader_factory->total_requests(),
            requests_before_incognito + 1u);

  // The rotated key is stored only in Incognito's in-memory overlay pref store;
  // the regular profile's prefs and service remain untouched.
  EXPECT_EQ(otr_profile->GetPrefs()->GetString(
                prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kRotatedEncodedTestOhttpKey);
  EXPECT_EQ(regular_prefs->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey),
            kEncodedTestOhttpKey);
  EXPECT_EQ(regular_prefs->GetTime(
                prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime),
            initial_expiration);
  base::test::TestFuture<std::optional<std::string>> regular_key_future;
  regular_service->GetOhttpKey(regular_key_future.GetCallback());
  EXPECT_EQ(regular_key_future.Get(), initial_ohttp_key);
}

// Checks that by default OhttpKeyServiceFactory returns null for both regular
// and off-the-record profiles for testing profiles if no testing factory is
// installed.
TEST_F(OhttpKeyServiceFactoryTest, GetForProfile_NoTestingFactory) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(
      OhttpKeyServiceFactory::GetForProfile(profile->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForProfile(profile.get()));
}
