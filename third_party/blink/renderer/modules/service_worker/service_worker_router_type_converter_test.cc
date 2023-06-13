// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source_enum.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_url_pattern_condition.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {

TEST(ServiceWorkerRouterTypeConverterTest, Basic) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_url_pattern = blink::RouterUrlPatternCondition::Create();
  idl_url_pattern->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_url_pattern);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterCondition expected_condition;
  expected_condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  blink::UrlPattern expected_url_pattern;
  auto parse_result = liburlpattern::Parse(
      kFakeUrlPattern,
      [](base::StringPiece input) { return std::string(input); });
  ASSERT_TRUE(parse_result.ok());
  expected_url_pattern.pathname = parse_result.value().PartList();
  expected_condition.url_pattern = std::move(expected_url_pattern);
  expected_rule.conditions.emplace_back(expected_condition);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  auto blink_rule =
      mojo::ConvertTo<absl::optional<blink::ServiceWorkerRouterRule>>(idl_rule);
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, EmptyUrlPatternShouldBeNullopt) {
  constexpr const char kFakeUrlPattern[] = "";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_url_pattern = blink::RouterUrlPatternCondition::Create();
  idl_url_pattern->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_url_pattern);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  auto blink_rule =
      mojo::ConvertTo<absl::optional<blink::ServiceWorkerRouterRule>>(idl_rule);
  EXPECT_FALSE(blink_rule.has_value());
}

TEST(ServiceWorkerRouterTypeConverterTest, RegexpUrlPatternShouldBeNullopt) {
  constexpr const char kFakeUrlPattern[] = "/fake/(\\\\d+)";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_url_pattern = blink::RouterUrlPatternCondition::Create();
  idl_url_pattern->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_url_pattern);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  auto blink_rule =
      mojo::ConvertTo<absl::optional<blink::ServiceWorkerRouterRule>>(idl_rule);
  EXPECT_FALSE(blink_rule.has_value());
}

}  // namespace

}  // namespace blink
