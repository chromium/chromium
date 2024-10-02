// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "content/browser/origin_trials/critical_origin_trials_throttle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::ResourceType;

const char kExampleURL[] = "https://example.com/";
const char kHttpHeaderPreamble[] = "HTTP/1.1 200 OK\n";
const char kOriginTrialHeader[] = "Origin-Trial: ";
const char kCriticalOriginTrialHeader[] = "Critical-Origin-Trial: ";
const char kCRLF[] = "\n";
const char kHttpHeaderTerminator[] = "\n\n";

const char kPersistentTrialName[] = "FrobulatePersistent";
const char kFakePersistentTrialName[] = "FakePersistent";

// Generated with
// tools/origin_trials/generate_token.py https://example.com FrobulatePersistent
// --expire-timestamp=2000000000
const char kPersistentTrialToken[] =
    "AzZfd1vKZ0SSGRGk/"
    "8nIszQSlHYjbuYVE3jwaNZG3X4t11zRhzPWWJwTZ+JJDS3JJsyEZcpz+y20pAP6/"
    "6upOQ4AAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI"
    "6ICJGcm9idWxhdGVQZXJzaXN0ZW50IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

class MockOriginTrialsDelegate
    : public content::OriginTrialsControllerDelegate {
 public:
  ~MockOriginTrialsDelegate() override = default;

  base::flat_map<url::Origin, base::flat_set<std::string>> persisted_trials_;

  int get_persisted_trials_count_ = 0;

  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      base::Time current_time) override {
    get_persisted_trials_count_++;
    const auto& it = persisted_trials_.find(origin);
    if (it != persisted_trials_.end()) {
      return it->second;
    } else {
      return {};
    }
  }

  bool IsFeaturePersistedForOrigin(const url::Origin& origin,
                                   const url::Origin& top_level_origin,
                                   blink::mojom::OriginTrialFeature feature,
                                   const base::Time current_time) override {
    std::string trial_name = "";
    switch (feature) {
      case blink::mojom::OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentFeature:
        trial_name = kPersistentTrialName;
        break;
      default:
        break;
    }
    const auto& it = persisted_trials_.find(origin);
    return it != persisted_trials_.end() && it->second.contains(trial_name);
  }

  void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {
    DCHECK(false) << "Critical Origin Trial Throttle should not override full "
                     "set of tokens, only append.";
  }

  void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      const base::span<const url::Origin> script_origins,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {}
  void ClearPersistedTokens() override { persisted_trials_.clear(); }

  void AddPersistedTrialForTest(std::string_view url,
                                std::string_view trial_name) {
    url::Origin key = url::Origin::Create(GURL(url));
    persisted_trials_[key].emplace(trial_name);
  }
};

class CriticalOriginTrialsThrottleTest : public ::testing::Test {
 public:
  CriticalOriginTrialsThrottleTest()
      : origin_trials_delegate_(),
        throttle_(origin_trials_delegate_,
                  url::Origin::Create(GURL(kExampleURL)),
                  /*source_id=*/std::nullopt) {}

  ~CriticalOriginTrialsThrottleTest() override = default;

  std::string CreateHeaderLines(std::string_view prefix,
                                const base::span<std::string> values) {
    std::string line;
    for (const std::string& value : values)
      line += base::StrCat({prefix, value, kCRLF});

    return line;
  }

  void StartRequest(std::string_view url, ResourceType resource_type) {
    network::ResourceRequest request;
    request.url = GURL(url);
    request.resource_type = static_cast<int>(resource_type);
    bool defer = false;
    throttle_.WillStartRequest(&request, &defer);
  }

  blink::URLLoaderThrottle::RestartWithURLReset BeforeWillProcess(
      std::string_view url,
      const base::span<std::string> origin_trial_tokens = {},
      const base::span<std::string> critical_origin_trials = {}) {
    network::mojom::URLResponseHead response_head;
    response_head.headers = net::HttpResponseHeaders::TryToCreate(base::StrCat(
        {kHttpHeaderPreamble,
         CreateHeaderLines(kOriginTrialHeader, origin_trial_tokens),
         CreateHeaderLines(kCriticalOriginTrialHeader, critical_origin_trials),
         kHttpHeaderTerminator}));
    blink::URLLoaderThrottle::RestartWithURLReset restart_with_url_reset(false);
    throttle_.BeforeWillProcessResponse(GURL(url), response_head,
                                        &restart_with_url_reset);
    return restart_with_url_reset;
  }

 protected:
  blink::ScopedTestOriginTrialPolicy trial_policy_;
  MockOriginTrialsDelegate origin_trials_delegate_;
  CriticalOriginTrialsThrottle throttle_;
};

TEST_F(CriticalOriginTrialsThrottleTest,
       StartRequestShouldStoreExistingPersistedTrials) {
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  EXPECT_EQ(1, origin_trials_delegate_.get_persisted_trials_count_);
}

TEST_F(CriticalOriginTrialsThrottleTest,
       CriticalShouldRestartIfNotAlreadyPresent) {
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  std::vector<std::string> critical_trials = {kPersistentTrialName};
  EXPECT_TRUE(BeforeWillProcess(kExampleURL, tokens, critical_trials));
}

TEST_F(CriticalOriginTrialsThrottleTest,
       CriticalShouldRestartIfNotAlreadyPresentForSubframe) {
  StartRequest(kExampleURL, ResourceType::kSubFrame);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  std::vector<std::string> critical_trials = {kPersistentTrialName};
  EXPECT_TRUE(BeforeWillProcess(kExampleURL, tokens, critical_trials));
}

TEST_F(CriticalOriginTrialsThrottleTest,
       CriticalShouldNotRestartIfNotAlreadyPresentForImageResource) {
  StartRequest(kExampleURL, ResourceType::kImage);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  std::vector<std::string> critical_trials = {kPersistentTrialName};
  EXPECT_FALSE(BeforeWillProcess(kExampleURL, tokens, critical_trials));
}

TEST_F(CriticalOriginTrialsThrottleTest, NoHeadersShouldNotRestartRequest) {
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  EXPECT_FALSE(BeforeWillProcess(kExampleURL));
}

TEST_F(CriticalOriginTrialsThrottleTest,
       NoCriticalHeadersShouldNotRestartRequest) {
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  EXPECT_FALSE(BeforeWillProcess(kExampleURL, tokens));
}

TEST_F(CriticalOriginTrialsThrottleTest,
       CriticalForNonRequestedTrialShouldNotRestart) {
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  std::vector<std::string> critical_trials = {kFakePersistentTrialName};
  EXPECT_FALSE(BeforeWillProcess(kExampleURL, tokens, critical_trials));
}

TEST_F(CriticalOriginTrialsThrottleTest,
       CriticalShouldNotRestartIfPreviouslySet) {
  origin_trials_delegate_.AddPersistedTrialForTest(kExampleURL,
                                                   kPersistentTrialName);
  StartRequest(kExampleURL, ResourceType::kMainFrame);
  std::vector<std::string> tokens = {kPersistentTrialToken};
  std::vector<std::string> critical_trials = {kPersistentTrialName};
  EXPECT_FALSE(BeforeWillProcess(kExampleURL, tokens, critical_trials));
}

}  // namespace

}  // namespace content
