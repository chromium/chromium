// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_router_rule_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom.h"
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
      condition.type = blink::ServiceWorkerRouterCondition::Type::kUrlPattern;
      blink::SafeUrlPattern url_pattern;
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
      condition.type = blink::ServiceWorkerRouterCondition::Type::kRequest;
      blink::ServiceWorkerRouterRequestCondition request;
      request.method = "GET";
      request.mode = network::mojom::RequestMode::kNavigate;
      request.destination = network::mojom::RequestDestination::kDocument;
      condition.request = request;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type = blink::ServiceWorkerRouterCondition::Type::kRequest;
      blink::ServiceWorkerRouterRequestCondition request;
      condition.request = request;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type =
          blink::ServiceWorkerRouterCondition::Type::kRunningStatus;
      blink::ServiceWorkerRouterRunningStatusCondition running_status;
      running_status.status = blink::ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kRunning;
      condition.running_status = running_status;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterCondition condition;
      condition.type = blink::ServiceWorkerRouterCondition::Type::kOr;
      blink::ServiceWorkerRouterOrCondition or_condition;

      blink::ServiceWorkerRouterCondition fake;
      fake.type = blink::ServiceWorkerRouterCondition::Type::kRequest;
      blink::ServiceWorkerRouterRequestCondition request;
      fake.request = request;

      or_condition.conditions = std::vector(3, fake);
      condition.or_condition = or_condition;
      rule.conditions.push_back(condition);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::Type::kNetwork;
      source.network_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::Type::kRace;
      source.race_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::Type::kFetchEvent;
      source.fetch_event_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::Type::kCache;
      source.cache_source.emplace();
      rule.sources.push_back(source);
    }
    {
      blink::ServiceWorkerRouterSource source;
      source.type = blink::ServiceWorkerRouterSource::Type::kCache;
      blink::ServiceWorkerRouterCacheSource cache_source;
      cache_source.cache_name = "example cache name";
      source.cache_source = cache_source;
      rule.sources.push_back(source);
    }
    rules.rules.push_back(rule);
  }
  TestRoundTrip(rules);
}

}  // namespace

}  // namespace blink
