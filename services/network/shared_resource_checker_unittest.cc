// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_resource_checker.h"

#include <string>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Allow cookies by default with an option to block all cookies for testing.
class MockCookieSettings : public content_settings::CookieSettingsBase {
 public:
  MockCookieSettings() {}
  ~MockCookieSettings() override = default;
  void set_block_all(bool block_all) { block_all_ = block_all; }

 private:
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info) const override {
    return block_all_ ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;
  }
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override {
    return false;
  }
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const override {
    return !block_all_;
  }
  bool ShouldBlockThirdPartyCookies(
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides) const override {
    return true;
  }
  bool MitigationsEnabledFor3pcd() const override { return false; }
  bool IsThirdPartyCookiesAllowedScheme(
      const std::string& scheme) const override {
    return true;
  }
  bool block_all_ = false;
};

static ResourceRequest CreateResourceRequest(const char* url) {
  ResourceRequest request;
  request.url = GURL(url);
  request.destination = mojom::RequestDestination::kScript;
  request.site_for_cookies = net::SiteForCookies::FromUrl(request.url);
  return request;
}

static constexpr char kTestUrlPatterns[] =
    "https://www.example.test/exact\n"
    "https://www.example.test/wildcard/end/*\n"
    "https://www.example.test/wildcard/*/middle\n"
    "https://www2.example.test/exact\n";

static const char* kPatternMatches[] = {
    "https://www.example.test/exact",
    "https://www.example.test/wildcard/end/match",
    "https://www.example.test/wildcard/match/middle",
    "https://www2.example.test/exact"};

static const char* kPatternMatchFails[] = {
    "https://www.example.test/exact?hello=world",
    "https://www.example.test/wildcard/end/query?hello=world",
    "https://www.example.test/exact2",
    "https://www.example.test/wildcard/end",
    "https://www.example.test/wildcard/not/middl",
    "https://www2.example.test/exac",
    "https://www3.example.test/exact",
    "http://www.example.test/exact",
    "file://www.example.test/exact"};

static const char* kUrlVariants[] = {
    "https://www.example.test/wildcard/end/match1",
    "https://www.example.test/wildcard/end/match2",
    "https://www.example.test/wildcard/end/match3",
    "https://www.example.test/wildcard/end/match4",
};

class SharedResourceCheckerTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 public:
  SharedResourceCheckerTest() {
    enabled_ = GetParam();
    if (enabled_) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kCacheSharingForPervasiveScripts,
          {{"url_patterns", kTestUrlPatterns}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kCacheSharingForPervasiveScripts);
    }
    shared_resource_checker_ =
        std::make_unique<SharedResourceChecker>(cookie_settings_);
  }
  void BlockAllCookies() { cookie_settings_.set_block_all(true); }
  bool enabled() const { return enabled_; }
  const std::unique_ptr<SharedResourceChecker>& shared_resource_checker()
      const {
    return shared_resource_checker_;
  }

 private:
  MockCookieSettings cookie_settings_;
  std::unique_ptr<SharedResourceChecker> shared_resource_checker_;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool enabled_ = false;
};

constexpr mojom::RequestDestination kAllDestinations[] = {
    mojom::RequestDestination::kEmpty,
    mojom::RequestDestination::kAudio,
    mojom::RequestDestination::kAudioWorklet,
    mojom::RequestDestination::kDocument,
    mojom::RequestDestination::kEmbed,
    mojom::RequestDestination::kFont,
    mojom::RequestDestination::kFrame,
    mojom::RequestDestination::kIframe,
    mojom::RequestDestination::kImage,
    mojom::RequestDestination::kManifest,
    mojom::RequestDestination::kObject,
    mojom::RequestDestination::kPaintWorklet,
    mojom::RequestDestination::kReport,
    mojom::RequestDestination::kScript,
    mojom::RequestDestination::kServiceWorker,
    mojom::RequestDestination::kSharedWorker,
    mojom::RequestDestination::kStyle,
    mojom::RequestDestination::kTrack,
    mojom::RequestDestination::kVideo,
    mojom::RequestDestination::kWebBundle,
    mojom::RequestDestination::kWorker,
    mojom::RequestDestination::kXslt,
    mojom::RequestDestination::kFencedframe,
    mojom::RequestDestination::kWebIdentity,
    mojom::RequestDestination::kDictionary,
    mojom::RequestDestination::kSpeculationRules,
    mojom::RequestDestination::kJson,
    mojom::RequestDestination::kSharedStorageWorklet,
};

// Make sure that all request destinations except for Script fail.
TEST_P(SharedResourceCheckerTest, DestinationIsScript) {
  ResourceRequest request = CreateResourceRequest(kPatternMatches[0]);
  std::optional<url::Origin> origin = url::Origin::Create(request.url);
  for (const mojom::RequestDestination& destination : kAllDestinations) {
    request.destination = destination;
    if (enabled() &&
        request.destination == mojom::RequestDestination::kScript) {
      EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request, origin,
                                                              std::nullopt));
    } else {
      EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request, origin,
                                                               std::nullopt));
    }
  }
}

TEST_P(SharedResourceCheckerTest, SuccessfulPatterns) {
  bool expect = enabled();
  for (const char* url : kPatternMatches) {
    ResourceRequest request = CreateResourceRequest(url);
    std::optional<url::Origin> origin = url::Origin::Create(request.url);
    EXPECT_EQ(expect, shared_resource_checker()->IsSharedResource(
                          request, origin, std::nullopt));
  }
}

TEST_P(SharedResourceCheckerTest, FailPatterns) {
  for (const char* url : kPatternMatchFails) {
    ResourceRequest request = CreateResourceRequest(url);
    std::optional<url::Origin> origin = url::Origin::Create(request.url);
    EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request, origin,
                                                             std::nullopt));
  }
}

TEST_P(SharedResourceCheckerTest, LoadFlags) {
  const int kBlockedFlags[] = {net::LOAD_VALIDATE_CACHE, net::LOAD_BYPASS_CACHE,
                               net::LOAD_SKIP_CACHE_VALIDATION,
                               net::LOAD_ONLY_FROM_CACHE,
                               net::LOAD_DISABLE_CACHE};
  if (!enabled()) {
    return;
  }
  // Set an innoculous flag to make sure the check check the bit flag and not
  // equality.
  ResourceRequest request = CreateResourceRequest(kPatternMatches[0]);
  std::optional<url::Origin> origin = url::Origin::Create(request.url);
  request.load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY;
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request, origin,
                                                          std::nullopt));
  for (auto flag : kBlockedFlags) {
    request.load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY | flag;
    EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request, origin,
                                                             std::nullopt));
  }
}

TEST_P(SharedResourceCheckerTest, CookiesDisabled) {
  if (!enabled()) {
    return;
  }
  ResourceRequest request = CreateResourceRequest(kPatternMatches[0]);
  std::optional<url::Origin> origin = url::Origin::Create(request.url);
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request, origin,
                                                          std::nullopt));
  auto cookie_partition_key = net::CookiePartitionKey::FromURLForTesting(
      request.url, net::CookiePartitionKey::AncestorChainBit::kCrossSite);
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(
      request, origin, cookie_partition_key));

  // Should not be shared for a nonced partition key.
  cookie_partition_key = net::CookiePartitionKey::FromURLForTesting(
      request.url, net::CookiePartitionKey::AncestorChainBit::kCrossSite,
      base::UnguessableToken::Create());
  EXPECT_FALSE(shared_resource_checker()->IsSharedResource(
      request, origin, cookie_partition_key));

  // Should not be shared when cookies are blocked.
  BlockAllCookies();
  EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request, origin,
                                                           std::nullopt));
}

TEST_P(SharedResourceCheckerTest, PatternLimits) {
  if (!enabled()) {
    return;
  }
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  ResourceRequest request1 = CreateResourceRequest(kUrlVariants[0]);
  ResourceRequest request2 = CreateResourceRequest(kUrlVariants[1]);
  ResourceRequest request3 = CreateResourceRequest(kUrlVariants[2]);
  ResourceRequest request4 = CreateResourceRequest(kUrlVariants[3]);
  std::optional<url::Origin> origin = url::Origin::Create(request1.url);

  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request1, origin,
                                                          std::nullopt));
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request2, origin,
                                                          std::nullopt));

  // Only 2 URLs per pattern should match (in a given hour window).
  EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request3, origin,
                                                           std::nullopt));

  // Freshen the timestamp on the first request.
  task_environment.AdvanceClock(base::Hours(1));
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request1, origin,
                                                          std::nullopt));
  task_environment.AdvanceClock(base::Hours(1));

  // request3 should now be able to take request 2's spot (but request 1 is
  // still just inside the 1-hour window).
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request3, origin,
                                                          std::nullopt));
  EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request2, origin,
                                                           std::nullopt));
  EXPECT_FALSE(shared_resource_checker()->IsSharedResource(request4, origin,
                                                           std::nullopt));
  EXPECT_TRUE(shared_resource_checker()->IsSharedResource(request1, origin,
                                                          std::nullopt));
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         SharedResourceCheckerTest,
                         testing::Bool());

}  // namespace network
