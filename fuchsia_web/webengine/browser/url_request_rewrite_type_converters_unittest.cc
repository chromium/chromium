// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/url_request_rewrite_type_converters.h"

#include <lib/fidl/cpp/binding.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

url_rewrite::mojom::UrlRequestRewriteRulesPtr ConvertFuchsiaRulesToMojom(
    fuchsia::web::UrlRequestRewrite rewrite) {
  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(std::move(rewrite));

  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));

  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));

  return mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
      std::move(rules));
}

}  // namespace

// Tests AddHeaders rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertAddHeader) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      ConvertFuchsiaRulesToMojom(CreateRewriteAddHeaders("Test", "Value"));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_add_headers());

  const std::vector<url_rewrite::mojom::UrlHeaderPtr>& headers =
      rules->rules[0]->actions[0]->get_add_headers()->headers;
  ASSERT_EQ(headers.size(), 1u);
  ASSERT_EQ(headers[0]->name, "Test");
  ASSERT_EQ(headers[0]->value, "Value");
}

// Tests RemoveHeader rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertRemoveHeader) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      ConvertFuchsiaRulesToMojom(
          CreateRewriteRemoveHeader(std::make_optional("Test"), "Header"));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_remove_header());

  const url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header1 =
      rules->rules[0]->actions[0]->get_remove_header();
  ASSERT_TRUE(remove_header1->query_pattern);
  ASSERT_EQ(remove_header1->query_pattern.value().compare("Test"), 0);
  ASSERT_EQ(remove_header1->header_name.compare("Header"), 0);

  // Create a RemoveHeader rewrite with no pattern.
  rules = ConvertFuchsiaRulesToMojom(
      CreateRewriteRemoveHeader(std::nullopt, "Header"));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_remove_header());

  const url_rewrite::mojom::UrlRequestRewriteRemoveHeaderPtr& remove_header2 =
      rules->rules[0]->actions[0]->get_remove_header();
  ASSERT_FALSE(remove_header2->query_pattern);
  ASSERT_EQ(remove_header2->header_name.compare("Header"), 0);
}

// Tests SubstituteQueryPattern rewrites are properly converted to their Mojo
// equivalent.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertSubstituteQueryPattern) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      ConvertFuchsiaRulesToMojom(
          CreateRewriteSubstituteQueryPattern("Pattern", "Substitution"));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_substitute_query_pattern());

  const url_rewrite::mojom::UrlRequestRewriteSubstituteQueryPatternPtr&
      substitute_query_pattern =
          rules->rules[0]->actions[0]->get_substitute_query_pattern();
  ASSERT_EQ(substitute_query_pattern->pattern.compare("Pattern"), 0);
  ASSERT_EQ(substitute_query_pattern->substitution.compare("Substitution"), 0);
}

// Tests ReplaceUrl rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertReplaceUrl) {
  GURL url("http://site.xyz");
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      ConvertFuchsiaRulesToMojom(
          CreateRewriteReplaceUrl("/something", url.spec()));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_replace_url());

  const url_rewrite::mojom::UrlRequestRewriteReplaceUrlPtr& replace_url =
      rules->rules[0]->actions[0]->get_replace_url();
  ASSERT_EQ(replace_url->url_ends_with.compare("/something"), 0);
  ASSERT_EQ(replace_url->new_url.spec().compare(url.spec()), 0);
}

// Tests AppendToQuery rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertAppendToQuery) {
  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      ConvertFuchsiaRulesToMojom(CreateRewriteAppendToQuery("foo=bar&foo"));
  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_FALSE(rules->rules[0]->hosts_filter);
  ASSERT_FALSE(rules->rules[0]->schemes_filter);
  ASSERT_EQ(rules->rules[0]->actions.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->actions[0]->is_append_to_query());

  const url_rewrite::mojom::UrlRequestRewriteAppendToQueryPtr& append_to_query =
      rules->rules[0]->actions[0]->get_append_to_query();
  ASSERT_EQ(append_to_query->query.compare("foo=bar&foo"), 0);
}

// Tests host names containing non-ASCII characters are properly converted.
TEST(UrlRequestRewriteTypeConvertersTest, ConvertInternationalHostName) {
  const char kNonAsciiHostName[] = "t\u00E8st.net";
  const char kNonAsciiHostNameWithWildcard[] = "*.t\u00E8st.net";
  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  rule.set_hosts_filter({kNonAsciiHostName, kNonAsciiHostNameWithWildcard});

  std::vector<fuchsia::web::UrlRequestRewriteRule> fidl_rules;
  fidl_rules.push_back(std::move(rule));

  url_rewrite::mojom::UrlRequestRewriteRulesPtr rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          std::move(fidl_rules));

  ASSERT_EQ(rules->rules.size(), 1u);
  ASSERT_TRUE(rules->rules[0]->hosts_filter);
  ASSERT_EQ(rules->rules[0]->hosts_filter.value().size(), 2u);
  EXPECT_EQ(rules->rules[0]->hosts_filter.value()[0].compare("xn--tst-6la.net"),
            0);
  EXPECT_EQ(
      rules->rules[0]->hosts_filter.value()[1].compare("*.xn--tst-6la.net"), 0);
}
