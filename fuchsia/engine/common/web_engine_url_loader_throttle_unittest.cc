// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "fuchsia/engine/common/cors_exempt_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace {

constexpr char kMixedCaseCorsExemptHeader[] = "CoRs-ExEmPt";
constexpr char kUpperCaseCorsExemptHeader[] = "CORS-EXEMPT";
constexpr char kMixedCaseCorsExemptHeader2[] = "Another-CoRs-ExEmPt-2";
constexpr char kUpperCaseCorsExemptHeader2[] = "ANOTHER-CORS-EXEMPT-2";
constexpr char kRequiresCorsHeader[] = "requires-cors";

class TestCachedRulesProvider
    : public WebEngineURLLoaderThrottle::CachedRulesProvider {
 public:
  TestCachedRulesProvider() = default;
  ~TestCachedRulesProvider() override = default;

  void SetCachedRules(
      scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
          cached_rules) {
    cached_rules_ = cached_rules;
  }

  // WebEngineURLLoaderThrottle::CachedRulesProvider implementation.
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
  GetCachedRules() override {
    return cached_rules_;
  }

 private:
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules_;

  DISALLOW_COPY_AND_ASSIGN(TestCachedRulesProvider);
};

}  // namespace

class WebEngineURLLoaderThrottleTest : public testing::Test {
 public:
  WebEngineURLLoaderThrottleTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~WebEngineURLLoaderThrottleTest() override = default;

  void SetUp() override { SetCorsExemptHeaders({}); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests rules are properly applied when wildcard-filtering is used on hosts.
TEST_F(WebEngineURLLoaderThrottleTest, WildcardHosts) {
  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.SetHeader("Header", "Value");
  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = base::Optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  std::vector<mojom::UrlRequestRulePtr> rules;
  rules.push_back(std::move(rule));

  TestCachedRulesProvider provider;
  provider.SetCachedRules(
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          std::move(rules)));

  WebEngineURLLoaderThrottle throttle(&provider);
  bool defer = false;

  network::ResourceRequest request1;
  request1.url = GURL("http://test.net");
  throttle.WillStartRequest(&request1, &defer);
  EXPECT_TRUE(request1.headers.HasHeader("Header"));

  network::ResourceRequest request2;
  request2.url = GURL("http://subdomain.test.net");
  throttle.WillStartRequest(&request2, &defer);
  EXPECT_TRUE(request2.headers.HasHeader("Header"));

  network::ResourceRequest request3;
  request3.url = GURL("http://domaintest.net");
  throttle.WillStartRequest(&request3, &defer);
  EXPECT_FALSE(request3.headers.HasHeader("Header"));

  network::ResourceRequest request4;
  request4.url = GURL("http://otherdomain.net");
  throttle.WillStartRequest(&request4, &defer);
  EXPECT_FALSE(request4.headers.HasHeader("Header"));
}

// Verifies that injected headers are correctly exempted from CORS checks if
// their names are registered as CORS exempt.
TEST_F(WebEngineURLLoaderThrottleTest, CorsAwareHeaders) {
  // Use the mixed case form for CORS exempt header #1, and the uppercased form
  // of header #2.
  SetCorsExemptHeaders(
      {kMixedCaseCorsExemptHeader, kUpperCaseCorsExemptHeader2});

  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.SetHeader(kRequiresCorsHeader, "Value");

  // Inject the uppercased form for CORS exempt header #1, and the mixed case
  // form of header #2.
  add_headers->headers.SetHeader(kUpperCaseCorsExemptHeader, "Value");
  add_headers->headers.SetHeader(kMixedCaseCorsExemptHeader2, "Value");

  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = base::Optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  std::vector<mojom::UrlRequestRulePtr> rules;
  rules.push_back(std::move(rule));

  TestCachedRulesProvider provider;
  provider.SetCachedRules(
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          std::move(rules)));

  WebEngineURLLoaderThrottle throttle(&provider);

  network::ResourceRequest request;
  request.url = GURL("http://test.net");
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  // Verify that the cors-exempt and cors-required headers were partitioned into
  // the "cors_exempt_headers" and "headers" arrays, respectively.
  EXPECT_TRUE(
      request.cors_exempt_headers.HasHeader(kUpperCaseCorsExemptHeader));
  EXPECT_TRUE(
      request.cors_exempt_headers.HasHeader(kMixedCaseCorsExemptHeader2));
  EXPECT_TRUE(request.headers.HasHeader(kRequiresCorsHeader));

  // Verify that the headers were not also placed in the other array.
  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(kRequiresCorsHeader));
  EXPECT_FALSE(request.headers.HasHeader(kUpperCaseCorsExemptHeader));
  EXPECT_FALSE(request.headers.HasHeader(kMixedCaseCorsExemptHeader2));
}

// Tests URL replacement rules that replace to a data URL do not append query or
// ref from the original URL.
TEST_F(WebEngineURLLoaderThrottleTest, DataReplacementUrl) {
  const char kCssDataURI[] = "data:text/css,";

  mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
      mojom::UrlRequestRewriteReplaceUrl::New();
  replace_url->url_ends_with = ".css";
  replace_url->new_url = GURL(kCssDataURI);
  mojom::UrlRequestActionPtr rewrite =
      mojom::UrlRequestAction::NewReplaceUrl(std::move(replace_url));
  std::vector<mojom::UrlRequestActionPtr> actions;
  actions.push_back(std::move(rewrite));
  mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
  rule->hosts_filter = base::Optional<std::vector<std::string>>({"*.test.net"});
  rule->actions = std::move(actions);

  std::vector<mojom::UrlRequestRulePtr> rules;
  rules.push_back(std::move(rule));

  TestCachedRulesProvider provider;
  provider.SetCachedRules(
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          std::move(rules)));

  WebEngineURLLoaderThrottle throttle(&provider);
  bool defer = false;

  network::ResourceRequest request;
  request.url = GURL("http://test.net/style.css?query#ref");
  throttle.WillStartRequest(&request, &defer);
  EXPECT_EQ(request.url, base::StringPiece(kCssDataURI));
}

class TestThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  TestThrottleDelegate() = default;
  ~TestThrottleDelegate() override = default;

  bool canceled() const { return canceled_; }
  base::StringPiece cancel_reason() const { return cancel_reason_; }

  void Reset() {
    canceled_ = false;
    cancel_reason_.clear();
  }

  // URLLoaderThrottle::Delegate implementation.
  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    canceled_ = true;
    cancel_reason_ = custom_reason.as_string();
  }
  void Resume() override {}

 private:
  bool canceled_ = false;
  std::string cancel_reason_;
};

// Tests that resource loads can be allowed or blocked based on the
// UrlRequestAction policy.
TEST_F(WebEngineURLLoaderThrottleTest, AllowAndDeny) {
  std::vector<mojom::UrlRequestRulePtr> rules;

  {
    mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
    rule->hosts_filter = base::Optional<std::vector<std::string>>({"test.net"});
    rule->actions.push_back(mojom::UrlRequestAction::NewPolicy(
        mojom::UrlRequestAccessPolicy::kAllow));
    rules.push_back(std::move(rule));
  }
  {
    mojom::UrlRequestRulePtr rule = mojom::UrlRequestRule::New();
    rule->actions.push_back(mojom::UrlRequestAction::NewPolicy(
        mojom::UrlRequestAccessPolicy::kDeny));
    rules.push_back(std::move(rule));
  }

  TestCachedRulesProvider provider;
  provider.SetCachedRules(
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          std::move(rules)));

  WebEngineURLLoaderThrottle throttle(&provider);
  bool defer = false;

  TestThrottleDelegate delegate;
  throttle.set_delegate(&delegate);

  network::ResourceRequest request1;
  request1.url = GURL("http://test.net");
  throttle.WillStartRequest(&request1, &defer);
  EXPECT_FALSE(delegate.canceled());

  delegate.Reset();

  network::ResourceRequest request2;
  request2.url = GURL("http://blocked.net");
  throttle.WillStartRequest(&request2, &defer);
  EXPECT_TRUE(delegate.canceled());
  EXPECT_EQ(delegate.cancel_reason(),
            "Resource load blocked by embedder policy.");
}
