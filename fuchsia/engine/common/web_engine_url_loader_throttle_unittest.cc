// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests rules are properly applied when wildcard-filtering is used on hosts.
TEST_F(WebEngineURLLoaderThrottleTest, WildcardHosts) {
  mojom::UrlRequestRewriteAddHeadersPtr add_headers =
      mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.SetHeader("Header", "Value");
  mojom::UrlRequestRewritePtr rewrite =
      mojom::UrlRequestRewrite::NewAddHeaders(std::move(add_headers));
  std::vector<mojom::UrlRequestRewritePtr> rewrites;
  rewrites.push_back(std::move(rewrite));
  mojom::UrlRequestRewriteRulePtr rule = mojom::UrlRequestRewriteRule::New();
  rule->hosts_filter = base::Optional<std::vector<std::string>>({"*.test.net"});
  rule->rewrites = std::move(rewrites);

  std::vector<mojom::UrlRequestRewriteRulePtr> rules;
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

// Tests URL replacement rules that replace to a data URL do not append query or
// ref from the original URL.
TEST_F(WebEngineURLLoaderThrottleTest, DataReplacementUrl) {
  const char kCssDataURI[] = "data:text/css,";

  mojom::UrlRequestRewriteReplaceUrlPtr replace_url =
      mojom::UrlRequestRewriteReplaceUrl::New();
  replace_url->url_ends_with = ".css";
  replace_url->new_url = GURL(kCssDataURI);
  mojom::UrlRequestRewritePtr rewrite =
      mojom::UrlRequestRewrite::NewReplaceUrl(std::move(replace_url));
  std::vector<mojom::UrlRequestRewritePtr> rewrites;
  rewrites.push_back(std::move(rewrite));
  mojom::UrlRequestRewriteRulePtr rule = mojom::UrlRequestRewriteRule::New();
  rule->hosts_filter = base::Optional<std::vector<std::string>>({"*.test.net"});
  rule->rewrites = std::move(rewrites);

  std::vector<mojom::UrlRequestRewriteRulePtr> rules;
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
