// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include <string_view>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace content {

namespace {

// TODO(crbug.com/40241479): consolidate the default URLPatternInit.
// service_worker_router_type_converter_test.cc has the same function,
// and we can also initialize legacy URLPattern with this in
// service_worker_database.cc.
blink::SafeUrlPattern DefaultURLPattern() {
  blink::SafeUrlPattern url_pattern;

  // The following matches everything. i.e. "*".
  liburlpattern::Part part;
  part.modifier = liburlpattern::Modifier::kNone;
  part.type = liburlpattern::PartType::kFullWildcard;
  part.name = "0";

  url_pattern.protocol.push_back(part);
  url_pattern.username.push_back(part);
  url_pattern.password.push_back(part);
  url_pattern.hostname.push_back(part);
  url_pattern.port.push_back(part);
  url_pattern.pathname.push_back(part);
  url_pattern.search.push_back(part);
  url_pattern.hash.push_back(part);

  return url_pattern;
}

std::string ParseEncodeCallback(std::string_view input) {
  return std::string(input);
}

TEST(ServiceWorkerRouterEvaluator, EmptyRule) {
  blink::ServiceWorkerRouterRules rules;
  ServiceWorkerRouterEvaluator evaluator(rules);
  EXPECT_EQ(0U, evaluator.rules().rules.size());

  ASSERT_TRUE(evaluator.IsValid());
  network::ResourceRequest request;
  request.method = "GET";
  request.url = GURL("https://example.com/");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, SimpleMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, SimpleExactMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("/test/page.html", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, OneConditionMisMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      // Match
      blink::ServiceWorkerRouterRunningStatusCondition running_status;
      running_status.status = blink::ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kRunning;
      rule.condition = blink::ServiceWorkerRouterCondition::WithRunningStatus(
          running_status);
    }
    {
      // Not match
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("/notmatch/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  const auto eval_result =
      evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kRunning);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, AllConditionMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterRunningStatusCondition running_status;
      running_status.status = blink::ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kRunning;
      rule.condition = blink::ServiceWorkerRouterCondition::WithRunningStatus(
          running_status);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  const auto eval_result =
      evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kRunning);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, ChooseMatchedRoute) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("*.html", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("*.css", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kRace;
      source.race_source.emplace();
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
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  // Four sources rule should match because of *.css URLPattern.
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
  EXPECT_EQ(network::mojom::ServiceWorkerRouterSourceType::kRace,
            eval_result->sources[0].type);
}

TEST(ServiceWorkerRouterEvaluator, SimpleHostnameMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("*.example.com", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.hostname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL("https://www.example.com/test/page.html");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, SimpleExactHostnameMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("www.example.com", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.hostname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL("https://www.example.com/test/page.html");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingHostnameCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("*.example.com", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL("https://www.example.org/notmatched/page.html");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, MatchingVariousCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern;
      {
        auto parse_result = liburlpattern::Parse("https", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.protocol = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("user*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.username = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("pass*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.password = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("*.example.org", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.hostname = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("80*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.port = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("*.html", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.pathname = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("query=test", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.search = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("test_hash", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.hash = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, MatchingDefaultURLPattern) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      rule.condition = blink::ServiceWorkerRouterCondition::WithUrlPattern(
          DefaultURLPattern());
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingProtocol) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result = liburlpattern::Parse("wss", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.protocol = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingUsername) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result =
            liburlpattern::Parse("not_matching_user", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.username = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingPassword) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result =
            liburlpattern::Parse("not_matching_pass", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.password = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingPort) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result = liburlpattern::Parse("1234", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.port = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingSearch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result =
            liburlpattern::Parse("not_matching_query", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.search = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, NotMatchingHash) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      {
        auto parse_result =
            liburlpattern::Parse("not_matching", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.hash = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL(
      "https://username:password@www.example.org:8000/matched/"
      "page.html?query=test#test_hash");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, SimpleIgnoreCaseMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("/test/*.html", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      url_pattern.options.ignore_case = true;
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL("https://example.com/TeSt/page.HTML");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_TRUE(eval_result.has_value());
  EXPECT_EQ(1U, eval_result->sources.size());
}

TEST(ServiceWorkerRouterEvaluator, SimpleRespectCaseAndMismatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result =
          liburlpattern::Parse("/test/*.html", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      // Respects case.
      url_pattern.options.ignore_case = false;
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
  request.url = GURL("https://example.com/TeSt/page.HTML");
  const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
  EXPECT_FALSE(eval_result.has_value());
}

TEST(ServiceWorkerRouterEvaluator, EmptyCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    // No condition is set.
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
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
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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

TEST(ServiceWorkerRouterEvaluator, RequestMatch) {
  auto verify =
      [](const blink::ServiceWorkerRouterRequestCondition& request_condition,
         const network::ResourceRequest& request, bool expect_match) {
        blink::ServiceWorkerRouterRules rules;
        {
          blink::ServiceWorkerRouterRule rule;
          {
            rule.condition = blink::ServiceWorkerRouterCondition::WithRequest(
                request_condition);
          }
          {
            blink::ServiceWorkerRouterSource source;
            source.type =
                network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
            source.fetch_event_source.emplace();
            rule.sources.push_back(source);
          }
          rules.rules.push_back(rule);
        }
        ASSERT_EQ(1U, rules.rules.size());
        ServiceWorkerRouterEvaluator evaluator(rules);
        ASSERT_EQ(1U, evaluator.rules().rules.size());
        EXPECT_TRUE(evaluator.IsValid());

        const auto eval_result =
            evaluator.EvaluateWithoutRunningStatus(request);
        if (expect_match) {
          EXPECT_TRUE(eval_result.has_value());
          EXPECT_EQ(1U, eval_result->sources.size());
        } else {
          EXPECT_FALSE(eval_result.has_value());
        }
      };

  network::ResourceRequest request;
  request.method = "GET";
  request.mode = network::mojom::RequestMode::kCors;
  request.destination = network::mojom::RequestDestination::kFrame;
  request.url = GURL("https://example.com/test/page.html");

  // match cases.
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.method = "GET";
    verify(rc, request, /*expect_match=*/true);
  }
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.mode = network::mojom::RequestMode::kCors;
    verify(rc, request, /*expect_match=*/true);
  }
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.destination = network::mojom::RequestDestination::kFrame;
    verify(rc, request, /*expect_match=*/true);
  }
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.method = "GET";
    rc.mode = network::mojom::RequestMode::kCors;
    rc.destination = network::mojom::RequestDestination::kFrame;
    verify(rc, request, /*expect_match=*/true);
  }

  // not matched case.
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.method = "POST";
    verify(rc, request, /*expect_match=*/false);
  }
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.mode = network::mojom::RequestMode::kNoCors;
    verify(rc, request, /*expect_match=*/false);
  }
  {
    blink::ServiceWorkerRouterRequestCondition rc;
    rc.destination = network::mojom::RequestDestination::kAudio;
    verify(rc, request, /*expect_match=*/false);
  }
}

TEST(ServiceWorkerRouterEvaluator, RunningStatusMatch) {
  auto verify = [](const blink::ServiceWorkerRouterRunningStatusCondition&
                       running_status_condition,
                   blink::EmbeddedWorkerStatus running_status,
                   bool expect_match) {
    blink::ServiceWorkerRouterRules rules;
    {
      blink::ServiceWorkerRouterRule rule;
      {
        rule.condition = blink::ServiceWorkerRouterCondition::WithRunningStatus(
            running_status_condition);
      }
      {
        blink::ServiceWorkerRouterSource source;
        source.type =
            network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
        source.fetch_event_source.emplace();
        rule.sources.push_back(source);
      }
      rules.rules.push_back(rule);
    }
    ASSERT_EQ(1U, rules.rules.size());
    ServiceWorkerRouterEvaluator evaluator(rules);
    ASSERT_EQ(1U, evaluator.rules().rules.size());
    EXPECT_TRUE(evaluator.IsValid());
    EXPECT_TRUE(evaluator.need_running_status());

    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/");

    const auto eval_result = evaluator.Evaluate(request, running_status);
    if (expect_match) {
      EXPECT_TRUE(eval_result.has_value());
      EXPECT_EQ(1U, eval_result->sources.size());
    } else {
      EXPECT_FALSE(eval_result.has_value());
    }
  };

  {
    blink::ServiceWorkerRouterRunningStatusCondition rc;
    rc.status = blink::ServiceWorkerRouterRunningStatusCondition::
        RunningStatusEnum::kRunning;
    verify(rc, blink::EmbeddedWorkerStatus::kRunning,
           /*expect_match=*/true);
  }
  {
    blink::ServiceWorkerRouterRunningStatusCondition rc;
    rc.status = blink::ServiceWorkerRouterRunningStatusCondition::
        RunningStatusEnum::kRunning;
    verify(rc, blink::EmbeddedWorkerStatus::kStopped,
           /*expect_match=*/false);
    verify(rc, blink::EmbeddedWorkerStatus::kStarting,
           /*expect_match=*/false);
    verify(rc, blink::EmbeddedWorkerStatus::kStopping,
           /*expect_match=*/false);
  }
  {
    blink::ServiceWorkerRouterRunningStatusCondition rc;
    rc.status = blink::ServiceWorkerRouterRunningStatusCondition::
        RunningStatusEnum::kNotRunning;
    verify(rc, blink::EmbeddedWorkerStatus::kRunning,
           /*expect_match=*/false);
  }
  {
    blink::ServiceWorkerRouterRunningStatusCondition rc;
    rc.status = blink::ServiceWorkerRouterRunningStatusCondition::
        RunningStatusEnum::kNotRunning;
    verify(rc, blink::EmbeddedWorkerStatus::kStopped,
           /*expect_match=*/true);
    verify(rc, blink::EmbeddedWorkerStatus::kStarting,
           /*expect_match=*/true);
    verify(rc, blink::EmbeddedWorkerStatus::kStopping,
           /*expect_match=*/true);
  }
}

TEST(ServiceWorkerRouterEvaluator, EmptyOrConditionAlwaysUnMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      rule.condition = blink::ServiceWorkerRouterCondition::WithOrCondition({});
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());
  EXPECT_FALSE(evaluator.need_running_status());

  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_FALSE(eval_result.has_value());
  }
  {
    network::ResourceRequest request;
    request.method = "POST";
    request.url = GURL("https://example.com/");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_FALSE(eval_result.has_value());
  }
  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/");
    const auto eval_result =
        evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kRunning);
    EXPECT_FALSE(eval_result.has_value());
  }
}

TEST(ServiceWorkerRouterEvaluator, OrConditionMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterOrCondition outer_or;
      {
        blink::ServiceWorkerRouterOrCondition inner_or;
        {
          blink::ServiceWorkerRouterRunningStatusCondition running_status;
          running_status.status =
              blink::ServiceWorkerRouterRunningStatusCondition::
                  RunningStatusEnum::kRunning;
          inner_or.conditions.emplace_back(
              blink::ServiceWorkerRouterCondition::WithRunningStatus(
                  running_status));
        }
        outer_or.conditions.emplace_back(
            blink::ServiceWorkerRouterCondition::WithOrCondition(inner_or));
      }
      {
        blink::SafeUrlPattern url_pattern = DefaultURLPattern();
        auto parse_result =
            liburlpattern::Parse("/test/page.html", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.pathname = parse_result.value().PartList();
        outer_or.conditions.emplace_back(
            blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern));
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithOrCondition(outer_or);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());
  EXPECT_TRUE(evaluator.need_running_status());
  size_t max_depth, max_width;
  std::tie(max_depth, max_width) = evaluator.GetMaxDepthAndWidth();
  EXPECT_EQ(2U, max_depth);
  EXPECT_EQ(2U, max_width);

  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/");
    const auto eval_result =
        evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kRunning);
    EXPECT_TRUE(eval_result.has_value());
    EXPECT_EQ(1U, eval_result->sources.size());
  }
  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/");
    const auto eval_result =
        evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kStopped);
    EXPECT_FALSE(eval_result.has_value());
  }
  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://www.example.com/test/page.html");
    const auto eval_result =
        evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kStopped);
    EXPECT_TRUE(eval_result.has_value());
    EXPECT_EQ(1U, eval_result->sources.size());
  }
  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://www.example.com/test/page.html");
    const auto eval_result =
        evaluator.Evaluate(request, blink::EmbeddedWorkerStatus::kRunning);
    EXPECT_TRUE(eval_result.has_value());
    EXPECT_EQ(1U, eval_result->sources.size());
  }
}

TEST(ServiceWorkerRouterEvaluator, NotConditionMatch) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      blink::ServiceWorkerRouterNotCondition not_condition;
      not_condition.condition =
          std::make_unique<blink::ServiceWorkerRouterCondition>();
      *not_condition.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithNotCondition(not_condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  {  // Expect not matching to /test/.
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/test/page.html");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_FALSE(eval_result.has_value());
  }
  {  // matching anything else.
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/page/page.html");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_TRUE(eval_result.has_value());
    EXPECT_EQ(1U, eval_result->sources.size());
  }
}

TEST(ServiceWorkerRouterEvaluator, NotConditionMatchNested) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      auto parse_result = liburlpattern::Parse("/test/*", ParseEncodeCallback);
      ASSERT_TRUE(parse_result.ok());
      url_pattern.pathname = parse_result.value().PartList();
      blink::ServiceWorkerRouterNotCondition not_condition;
      not_condition.condition =
          std::make_unique<blink::ServiceWorkerRouterCondition>();
      *not_condition.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
      blink::ServiceWorkerRouterNotCondition not_not_condition;
      not_not_condition.condition =
          std::make_unique<blink::ServiceWorkerRouterCondition>();
      *not_not_condition.condition =
          blink::ServiceWorkerRouterCondition::WithNotCondition(not_condition);
      rule.condition = blink::ServiceWorkerRouterCondition::WithNotCondition(
          not_not_condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  ASSERT_EQ(1U, rules.rules.size());

  ServiceWorkerRouterEvaluator evaluator(rules);
  ASSERT_EQ(1U, evaluator.rules().rules.size());
  EXPECT_TRUE(evaluator.IsValid());

  {  // Expect matching to /test/.
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/test/page.html");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_TRUE(eval_result.has_value());
    EXPECT_EQ(1U, eval_result->sources.size());
  }
  {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL("https://example.com/page/page.html");
    const auto eval_result = evaluator.EvaluateWithoutRunningStatus(request);
    EXPECT_FALSE(eval_result.has_value());
  }
}

blink::ServiceWorkerRouterCondition generateNestedNotCondition(int depth) {
  if (depth <= 0) {
    return blink::ServiceWorkerRouterCondition::WithUrlPattern(
        DefaultURLPattern());
  }
  blink::ServiceWorkerRouterNotCondition not_condition;
  not_condition.condition =
      std::make_unique<blink::ServiceWorkerRouterCondition>();
  *not_condition.condition = generateNestedNotCondition(depth - 1);
  return blink::ServiceWorkerRouterCondition::WithNotCondition(not_condition);
}

TEST(ServiceWorkerRouterEvaluator, NotConditionShouldNotExceedMaxDepth) {
  auto notTest = [](int depth, bool expect_valid) {
    blink::ServiceWorkerRouterRules rules;
    {
      blink::ServiceWorkerRouterRule rule;
      { rule.condition = generateNestedNotCondition(depth); }
      {
        blink::ServiceWorkerRouterSource source;
        source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
        source.network_source.emplace();
        rule.sources.push_back(source);
      }
      rules.rules.push_back(rule);
    }
    ASSERT_EQ(1U, rules.rules.size());

    ServiceWorkerRouterEvaluator evaluator(rules);
    ASSERT_EQ(1U, evaluator.rules().rules.size());
    if (expect_valid) {
      EXPECT_TRUE(evaluator.IsValid());
    } else {
      EXPECT_FALSE(evaluator.IsValid());
    }
  };

  notTest(1, true);
  notTest(blink::kServiceWorkerRouterConditionMaxRecursionDepth + 1, false);
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
      blink::SafeUrlPattern url_pattern = DefaultURLPattern();
      blink::ServiceWorkerRouterRequestCondition request;
      {
        request.method = "GET";
        request.mode = network::mojom::RequestMode::kCors;
        request.destination = network::mojom::RequestDestination::kFrame;
      }
      blink::ServiceWorkerRouterRunningStatusCondition running_status;
      {
        running_status.status =
            blink::ServiceWorkerRouterRunningStatusCondition::
                RunningStatusEnum::kRunning;
      }
      rule.condition = {url_pattern, request, running_status, std::nullopt,
                        std::nullopt};
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kRace;
      source.race_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
      source.fetch_event_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kCache;
      source.cache_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kCache;
      blink::ServiceWorkerRouterCacheSource cache_source;
      cache_source.cache_name = "example_cache_name";
      source.cache_source = cache_source;
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
      rule.Set("id", 1);
      {
        base::Value::Dict condition;
        {
          base::Value::Dict url_pattern;
          url_pattern.Set("protocol", "*");
          url_pattern.Set("username", "*");
          url_pattern.Set("password", "*");
          url_pattern.Set("hostname", "*");
          url_pattern.Set("port", "*");
          url_pattern.Set("pathname", "*");
          url_pattern.Set("search", "*");
          url_pattern.Set("hash", "*");
          condition.Set("urlPattern", std::move(url_pattern));
        }
        {
          base::Value::Dict request;
          request.Set("method", "GET");
          request.Set("mode", "cors");
          request.Set("destination", "frame");
          condition.Set("request", base::Value(std::move(request)));
        }
        condition.Set("running_status", "running");

        rule.Set("condition", std::move(condition));
      }
      {
        base::Value::List sources;
        sources.Append("network");
        sources.Append("race-network-and-fetch-handler");
        sources.Append("fetch-event");
        sources.Append("cache");
        {
          base::Value::Dict source;
          source.Set("cache_name", "example_cache_name");
          sources.Append(std::move(source));
        }
        rule.Set("source", std::move(sources));
      }
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

TEST(ServiceWorkerRouterEvaluator, ToValueEmptyOrCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      rule.condition = blink::ServiceWorkerRouterCondition::WithOrCondition({});
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
      rule.Set("id", 1);
      {
        base::Value::Dict condition;
        condition.Set("or", base::Value::List());

        rule.Set("condition", std::move(condition));
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

TEST(ServiceWorkerRouterEvaluator, ToValueNestedOrCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterOrCondition inner_or;
      {
        blink::ServiceWorkerRouterRunningStatusCondition running_status;
        running_status.status =
            blink::ServiceWorkerRouterRunningStatusCondition::
                RunningStatusEnum::kRunning;
        inner_or.conditions.emplace_back(
            blink::ServiceWorkerRouterCondition::WithRunningStatus(
                running_status));
      }
      auto inner =
          blink::ServiceWorkerRouterCondition::WithOrCondition(inner_or);
      rule.condition = blink::ServiceWorkerRouterCondition::WithOrCondition(
          {std::vector(1, std::move(inner))});
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
    rule.Set("id", 1);
    {
      base::Value::Dict outer;
      {
        base::Value::List outer_conditions;
        {
          base::Value::Dict inner;
          base::Value::List inner_conditions;
          {
            base::Value::Dict condition;
            condition.Set("running_status", "running");
            inner_conditions.Append(std::move(condition));
          }
          inner.Set("or", std::move(inner_conditions));
          outer_conditions.Append(std::move(inner));
        }
        outer.Set("or", std::move(outer_conditions));
      }
      rule.Set("condition", std::move(outer));
    }
    {
      base::Value::List sources;
      sources.Append("network");
      rule.Set("source", std::move(sources));
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

TEST(ServiceWorkerRouterEvaluator, ToValueNotCondition) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::ServiceWorkerRouterRunningStatusCondition running_status;
      running_status.status = blink::ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kRunning;
      blink::ServiceWorkerRouterNotCondition not_condition;
      not_condition.condition =
          std::make_unique<blink::ServiceWorkerRouterCondition>();
      *not_condition.condition =
          blink::ServiceWorkerRouterCondition::WithRunningStatus(
              running_status);
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithNotCondition(not_condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
    rule.Set("id", 1);
    {
      base::Value::Dict condition;
      {
        base::Value::Dict running_status;
        running_status.Set("running_status", "running");
        condition.Set("not", std::move(running_status));
      }
      rule.Set("condition", std::move(condition));
    }
    {
      base::Value::List sources;
      sources.Append("network");
      rule.Set("source", std::move(sources));
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

TEST(ServiceWorkerRouterEvaluator, ToValueUrlPatternWithFields) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      blink::SafeUrlPattern url_pattern;
      {
        auto parse_result = liburlpattern::Parse("https", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.protocol = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("user*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.username = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("pass*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.password = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("*.example.org", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.hostname = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("80*", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.port = parse_result.value().PartList();
      }
      {
        auto parse_result = liburlpattern::Parse("*.html", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.pathname = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("query=test", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.search = parse_result.value().PartList();
      }
      {
        auto parse_result =
            liburlpattern::Parse("test_hash", ParseEncodeCallback);
        ASSERT_TRUE(parse_result.ok());
        url_pattern.hash = parse_result.value().PartList();
      }
      rule.condition =
          blink::ServiceWorkerRouterCondition::WithUrlPattern(url_pattern);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
    rule.Set("id", 1);
    {
      base::Value::Dict condition;
      {
        base::Value::Dict url_pattern;
        url_pattern.Set("protocol", "https");
        url_pattern.Set("username", "user*");
        url_pattern.Set("password", "pass*");
        url_pattern.Set("hostname", "*.example.org");
        url_pattern.Set("port", "80*");
        url_pattern.Set("pathname", "*.html");
        url_pattern.Set("search", "query=test");
        url_pattern.Set("hash", "test_hash");
        condition.Set("urlPattern", std::move(url_pattern));
      }
      rule.Set("condition", std::move(condition));
    }
    {
      base::Value::List sources;
      sources.Append("network");
      rule.Set("source", std::move(sources));
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

TEST(ServiceWorkerRouterEvaluator, ToValueUrlPatternWithoutFields) {
  blink::ServiceWorkerRouterRules rules;
  {
    blink::ServiceWorkerRouterRule rule;
    {
      rule.condition = blink::ServiceWorkerRouterCondition::WithUrlPattern({});
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
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
    rule.Set("id", 1);
    {
      base::Value::Dict condition;
      {
        base::Value::Dict url_pattern;
        url_pattern.Set("protocol", "");
        url_pattern.Set("username", "");
        url_pattern.Set("password", "");
        url_pattern.Set("hostname", "");
        url_pattern.Set("port", "");
        url_pattern.Set("pathname", "");
        url_pattern.Set("search", "");
        url_pattern.Set("hash", "");
        condition.Set("urlPattern", std::move(url_pattern));
      }
      rule.Set("condition", std::move(condition));
    }
    {
      base::Value::List sources;
      sources.Append("network");
      rule.Set("source", std::move(sources));
    }
    expected_rules.Append(std::move(rule));
  }
  EXPECT_EQ(expected_rules, evaluator.ToValue());
}

}  // namespace

}  // namespace content
