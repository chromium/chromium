// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"

#include <lib/fidl/cpp/binding.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class UrlRequestRewriteRulesManagerTest : public testing::Test {
 public:
  UrlRequestRewriteRulesManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        url_request_rewrite_rules_manager_(
            UrlRequestRewriteRulesManager::CreateForTesting()) {}
  ~UrlRequestRewriteRulesManagerTest() override = default;

 protected:
  zx_status_t UpdateRulesFromRewrite(fuchsia::web::UrlRequestRewrite rewrite) {
    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(std::move(rewrite));

    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));

    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    return url_request_rewrite_rules_manager_->OnRulesUpdated(std::move(rules),
                                                              []() {});
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<UrlRequestRewriteRulesManager>
      url_request_rewrite_rules_manager_;

  DISALLOW_COPY_AND_ASSIGN(UrlRequestRewriteRulesManagerTest);
};

// Tests AddHeaders rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteRulesManagerTest, ConvertAddHeader) {
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteAddHeaders("Test", "Value")),
            ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_FALSE(cached_rules->data[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->data[0]->schemes_filter);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_add_headers());

  const net::HttpRequestHeaders& headers =
      cached_rules->data[0]->rewrites[0]->get_add_headers()->headers;
  ASSERT_EQ(headers.GetHeaderVector().size(), 1u);

  std::string value;
  ASSERT_TRUE(headers.GetHeader("Test", &value));
  ASSERT_EQ(value.compare("Value"), 0);
}

// Tests RemoveHeader rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteRulesManagerTest, ConvertRemoveHeader) {
  EXPECT_EQ(UpdateRulesFromRewrite(cr_fuchsia::CreateRewriteRemoveHeader(
                base::make_optional("Test"), "Header")),
            ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_FALSE(cached_rules->data[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->data[0]->schemes_filter);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_remove_header());

  const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header1 =
      cached_rules->data[0]->rewrites[0]->get_remove_header();
  ASSERT_TRUE(remove_header1->query_pattern);
  ASSERT_EQ(remove_header1->query_pattern.value().compare("Test"), 0);
  ASSERT_EQ(remove_header1->header_name.compare("Header"), 0);

  // Create a RemoveHeader rewrite with no pattern.
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteRemoveHeader(base::nullopt, "Header")),
            ZX_OK);
  cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_FALSE(cached_rules->data[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->data[0]->schemes_filter);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_remove_header());

  const mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header2 =
      cached_rules->data[0]->rewrites[0]->get_remove_header();
  ASSERT_FALSE(remove_header2->query_pattern);
  ASSERT_EQ(remove_header2->header_name.compare("Header"), 0);
}

// Tests SubstituteQueryPattern rewrites are properly converted to their Mojo
// equivalent.
TEST_F(UrlRequestRewriteRulesManagerTest, ConvertSubstituteQueryPattern) {
  EXPECT_EQ(
      UpdateRulesFromRewrite(cr_fuchsia::CreateRewriteSubstituteQueryPattern(
          "Pattern", "Substitution")),
      ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_FALSE(cached_rules->data[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->data[0]->schemes_filter);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(
      cached_rules->data[0]->rewrites[0]->is_substitute_query_pattern());

  const mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
      substitute_query_pattern =
          cached_rules->data[0]->rewrites[0]->get_substitute_query_pattern();
  ASSERT_EQ(substitute_query_pattern->pattern.compare("Pattern"), 0);
  ASSERT_EQ(substitute_query_pattern->substitution.compare("Substitution"), 0);
}

// Tests ReplaceUrl rewrites are properly converted to their Mojo equivalent.
TEST_F(UrlRequestRewriteRulesManagerTest, ConvertReplaceUrl) {
  GURL url("http://site.xyz");
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteReplaceUrl("/something", url.spec())),
            ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_FALSE(cached_rules->data[0]->hosts_filter);
  ASSERT_FALSE(cached_rules->data[0]->schemes_filter);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_replace_url());

  const mojom::UrlRequestRewriteReplaceUrlPtr& replace_url =
      cached_rules->data[0]->rewrites[0]->get_replace_url();
  ASSERT_EQ(replace_url->url_ends_with.compare("/something"), 0);
  ASSERT_EQ(replace_url->new_url.spec().compare(url.spec()), 0);
}

// Tests validation is working as expected.
TEST_F(UrlRequestRewriteRulesManagerTest, Validation) {
  // Empty rewrite.
  EXPECT_EQ(UpdateRulesFromRewrite(fuchsia::web::UrlRequestRewrite()),
            ZX_ERR_INVALID_ARGS);

  // Invalid AddHeaders header name.
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteAddHeaders("Te\nst1", "Value")),
            ZX_ERR_INVALID_ARGS);

  // Invalid AddHeaders header value.
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteAddHeaders("Test1", "Val\nue")),
            ZX_ERR_INVALID_ARGS);

  // Empty AddHeaders.
  {
    fuchsia::web::UrlRequestRewrite rewrite;
    rewrite.set_add_headers(fuchsia::web::UrlRequestRewriteAddHeaders());
    EXPECT_EQ(UpdateRulesFromRewrite(std::move(rewrite)), ZX_ERR_INVALID_ARGS);
  }

  // Invalid RemoveHeader header name.
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteRemoveHeader("Query", "Head\ner")),
            ZX_ERR_INVALID_ARGS);

  // Empty RemoveHeader.
  {
    fuchsia::web::UrlRequestRewrite rewrite;
    rewrite.set_add_headers(fuchsia::web::UrlRequestRewriteAddHeaders());
    EXPECT_EQ(UpdateRulesFromRewrite(std::move(rewrite)), ZX_ERR_INVALID_ARGS);
  }

  // Empty SubstituteQueryPattern.
  {
    fuchsia::web::UrlRequestRewrite rewrite;
    rewrite.set_substitute_query_pattern(
        fuchsia::web::UrlRequestRewriteSubstituteQueryPattern());
    EXPECT_EQ(UpdateRulesFromRewrite(std::move(rewrite)), ZX_ERR_INVALID_ARGS);
  }

  // Invalid ReplaceUrl url_ends_with.
  EXPECT_EQ(UpdateRulesFromRewrite(cr_fuchsia::CreateRewriteReplaceUrl(
                "some%00thing", GURL("http://site.xyz").spec())),
            ZX_ERR_INVALID_ARGS);

  // Invalid ReplaceUrl new_url.
  EXPECT_EQ(UpdateRulesFromRewrite(cr_fuchsia::CreateRewriteReplaceUrl(
                "/something", "http:site:xyz")),
            ZX_ERR_INVALID_ARGS);

  // Empty ReplaceUrl.
  {
    fuchsia::web::UrlRequestRewrite rewrite;
    rewrite.set_replace_url(fuchsia::web::UrlRequestRewriteReplaceUrl());
    EXPECT_EQ(UpdateRulesFromRewrite(std::move(rewrite)), ZX_ERR_INVALID_ARGS);
  }
}

// Tests rules are properly renewed after new rules are sent.
TEST_F(UrlRequestRewriteRulesManagerTest, RuleRenewal) {
  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteAddHeaders("Test1", "Value")),
            ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_add_headers());
  ASSERT_TRUE(
      cached_rules->data[0]->rewrites[0]->get_add_headers()->headers.HasHeader(
          "Test1"));

  EXPECT_EQ(UpdateRulesFromRewrite(
                cr_fuchsia::CreateRewriteAddHeaders("Test2", "Value")),
            ZX_OK);

  // We should have the new rules.
  cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();
  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_EQ(cached_rules->data[0]->rewrites.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->rewrites[0]->is_add_headers());
  ASSERT_TRUE(
      cached_rules->data[0]->rewrites[0]->get_add_headers()->headers.HasHeader(
          "Test2"));
}

// Tests host names containing non-ASCII characters are properly converted.
TEST_F(UrlRequestRewriteRulesManagerTest, ConvertInternationalHostName) {
  const char kNonAsciiHostName[] = "t\u00E8st.net";
  const char kNonAsciiHostNameWithWildcard[] = "*.t\u00E8st.net";
  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  rule.set_hosts_filter({kNonAsciiHostName, kNonAsciiHostNameWithWildcard});

  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  EXPECT_EQ(url_request_rewrite_rules_manager_->OnRulesUpdated(std::move(rules),
                                                               []() {}),
            ZX_OK);
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules = url_request_rewrite_rules_manager_->GetCachedRules();

  ASSERT_EQ(cached_rules->data.size(), 1u);
  ASSERT_TRUE(cached_rules->data[0]->hosts_filter);
  ASSERT_EQ(cached_rules->data[0]->hosts_filter.value().size(), 2u);
  EXPECT_EQ(
      cached_rules->data[0]->hosts_filter.value()[0].compare("xn--tst-6la.net"),
      0);
  EXPECT_EQ(cached_rules->data[0]->hosts_filter.value()[1].compare(
                "*.xn--tst-6la.net"),
            0);
}
