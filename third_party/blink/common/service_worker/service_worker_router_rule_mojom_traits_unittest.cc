// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {

void TestRoundTrip(const blink::ServiceWorkerRouterRules& in) {
  blink::ServiceWorkerRouterRules result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ServiceWorkerRouterRules>(
          in, result));
  EXPECT_EQ(in, result);
}

TEST(ServiceWorkerRouterRulesTest, EmptyRoundTrip) {
  TestRoundTrip(blink::ServiceWorkerRouterRules());
}

TEST(ServiceWorkerRouterRulesTest, SimpleRoundTrip) {
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
  TestRoundTrip(rules);
}

}  // namespace

}  // namespace blink
