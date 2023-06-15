// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace content {

namespace {

TEST(ServiceWorkerRouterEvaluator, EmptyRule) {
  blink::ServiceWorkerRouterRules rules;
  ServiceWorkerRouterEvaluator evaluator(rules);
  EXPECT_EQ(0U, evaluator.rules().rules.size());

  ASSERT_TRUE(evaluator.IsValid());
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_TRUE(sources.empty());
}

TEST(ServiceWorkerRouterEvaluator, SimpleMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/test/page.html");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_EQ(1U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, SimpleExactMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/page.html",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/test/page.html");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_EQ(1U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/notmatched/page.html");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_EQ(0U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, OneConditionMisMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/notmatch/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/test/page.html");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_EQ(0U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, AllConditionMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "*.html", [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/test/page.html");
  const auto sources = evaluator.Evaluate(request);
  EXPECT_EQ(1U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, ChooseMatchedRoute) {
  // Since we currently only support Network source type, and we cannot
  // make the returned value difference by the source type, we use the number
  // of sources to allow the test to distinguish one rule to the other.
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "*.html", [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      // This is two sources rule.
      // As mentioned above, this is used as a marker that tells the rule
      // matches.
      rule.sources.push_back(source);
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "*.css", [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      // This is four sources rule.
      // As mentioned above, this is used as a marker that tells the rule
      // matches.
      rule.sources.push_back(source);
      rule.sources.push_back(source);
      rule.sources.push_back(source);
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(2U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(2U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/top/test.css");
  const auto sources = evaluator.Evaluate(request);
  // Four sources rule should match because of *.css URLPattern.
  EXPECT_EQ(4U, sources.size());
}

TEST(ServiceWorkerRouterEvaluator, EmptyCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    // No condition is set.
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_FALSE(evaluator.IsValid());
}

TEST(ServiceWorkerRouterEvaluator, EmptySource) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    // No source is set.
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_FALSE(evaluator.IsValid());
}

TEST(ServiceWorkerRouterEvaluator, InvalidSource) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      // source.network_source is not set.
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_FALSE(evaluator.IsValid());
}

TEST(ServiceWorkerRouterEvaluator, ToValueEmptyRule) {
  blink::ServiceWorkerRouterRules rules;
  ServiceWorkerRouterEvaluator evaluator(rules);
  EXPECT_EQ(0U, evaluator.rules().rules.size());
  base::Value::List v;
  EXPECT_EQ(v, evaluator.ToValue());
}

TEST(ServiceWorkerRouterEvaluator, ToValueBasicSimpleRule) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
      blink::UrlPattern url_pattern;
      auto parse_result = liburlpattern::Parse(
          "/test/*",
          [](base::StringPiece input) { return std::string(input); });
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      condition.url_pattern = url_pattern;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());
  base::Value::List expected_rules;
  {
    base::Value::Dict rule;
    {
      {
        base::Value::List conditions;
        {
          base::Value::Dict condition;
          condition.Set("urlPattern", "/test/*");
          conditions.Append(std::move(condition));
        }
        rule.Set("condition", std::move(conditions));
      }
      {
        base::Value::List sources;
        sources.Append("network");
        rule.Set("source", std::move(sources));
      }
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

}  // namespace

}  // namespace content
