// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_condition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source_enum.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {

TEST(ServiceWorkerRouterTypeConverterTest, Basic) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterCondition expected_condition;
  expected_condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  blink::SafeUrlPattern expected_url_pattern;
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

  V8TestingScope scope;
  auto blink_rule =
      ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, EmptyUrlPatternShouldBeNullopt) {
  constexpr const char kFakeUrlPattern[] = "";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  V8TestingScope scope;
  auto blink_rule =
      ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_FALSE(blink_rule.has_value());
}

TEST(ServiceWorkerRouterTypeConverterTest, RegexpUrlPatternShouldBeNullopt) {
  auto verify = [](const WTF::String& test_url_pattern) {
    auto* idl_rule = blink::RouterRule::Create();
    auto* idl_condition = blink::RouterCondition::Create();
    idl_condition->setUrlPattern(test_url_pattern);
    idl_rule->setCondition(idl_condition);
    idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

    V8TestingScope scope;
    auto blink_rule =
        ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
    EXPECT_FALSE(blink_rule.has_value());
  };
  verify("/fake/(\\\\d+)");
  verify("://fake(\\\\d+).com/");
}

TEST(ServiceWorkerRouterTypeConverterTest, Race) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(
      blink::V8RouterSourceEnum::Enum::kRaceNetworkAndFetchHandler);

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterCondition expected_condition;
  expected_condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  blink::SafeUrlPattern expected_url_pattern;
  auto parse_result = liburlpattern::Parse(
      kFakeUrlPattern,
      [](base::StringPiece input) { return std::string(input); });
  ASSERT_TRUE(parse_result.ok());
  expected_url_pattern.pathname = parse_result.value().PartList();
  expected_condition.url_pattern = std::move(expected_url_pattern);
  expected_rule.conditions.emplace_back(expected_condition);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::SourceType::kRace;
  expected_source.race_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule =
      ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, FetchEvent) {
  constexpr const char kFakeUrlPattern[] = "/fake";
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setUrlPattern(kFakeUrlPattern);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kFetchEvent);

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterCondition expected_condition;
  expected_condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  blink::SafeUrlPattern expected_url_pattern;
  auto parse_result = liburlpattern::Parse(
      kFakeUrlPattern,
      [](base::StringPiece input) { return std::string(input); });
  ASSERT_TRUE(parse_result.ok());
  expected_url_pattern.pathname = parse_result.value().PartList();
  expected_condition.url_pattern = std::move(expected_url_pattern);
  expected_rule.conditions.emplace_back(expected_condition);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type =
      blink::ServiceWorkerRouterSource::SourceType::kFetchEvent;
  expected_source.fetch_event_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule =
      ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, Request) {
  auto* idl_rule = blink::RouterRule::Create();
  auto* idl_condition = blink::RouterCondition::Create();
  idl_condition->setRequestMethod("FakeRequestMethod");
  idl_condition->setRequestMode(blink::V8RequestMode::Enum::kNavigate);
  idl_condition->setRequestDestination(
      blink::V8RequestDestination::Enum::kDocument);
  idl_rule->setCondition(idl_condition);
  idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

  blink::ServiceWorkerRouterRule expected_rule;
  blink::ServiceWorkerRouterCondition expected_condition;
  expected_condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kRequest;
  blink::ServiceWorkerRouterRequestCondition expected_request;
  expected_request.method = "FakeRequestMethod";
  expected_request.mode = network::mojom::RequestMode::kNavigate;
  expected_request.destination = network::mojom::RequestDestination::kDocument;
  expected_condition.request = std::move(expected_request);
  expected_rule.conditions.emplace_back(expected_condition);
  blink::ServiceWorkerRouterSource expected_source;
  expected_source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
  expected_source.network_source.emplace();
  expected_rule.sources.emplace_back(expected_source);

  V8TestingScope scope;
  auto blink_rule =
      ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(blink_rule.has_value());
  EXPECT_EQ(expected_rule, *blink_rule);
}

TEST(ServiceWorkerRouterTypeConverterTest, RequestMethodNormalize) {
  auto validate_normalize = [](const WTF::String& input,
                               const std::string& expected) {
    auto* idl_rule = blink::RouterRule::Create();
    auto* idl_condition = blink::RouterCondition::Create();
    idl_condition->setRequestMethod(input);
    idl_rule->setCondition(idl_condition);
    idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

    blink::ServiceWorkerRouterRule expected_rule;
    blink::ServiceWorkerRouterCondition expected_condition;
    expected_condition.type =
        blink::ServiceWorkerRouterCondition::ConditionType::kRequest;
    blink::ServiceWorkerRouterRequestCondition expected_request;
    expected_request.method = expected;
    expected_condition.request = std::move(expected_request);
    expected_rule.conditions.emplace_back(expected_condition);
    blink::ServiceWorkerRouterSource expected_source;
    expected_source.type =
        blink::ServiceWorkerRouterSource::SourceType::kNetwork;
    expected_source.network_source.emplace();
    expected_rule.sources.emplace_back(expected_source);

    V8TestingScope scope;
    auto blink_rule =
        ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());
    EXPECT_TRUE(blink_rule.has_value());
    EXPECT_EQ(expected_rule, *blink_rule);
  };
  validate_normalize("DeLeTe", "DELETE");
  validate_normalize("gEt", "GET");
  validate_normalize("HeAd", "HEAD");
  validate_normalize("oPtIoNs", "OPTIONS");
  validate_normalize("PoSt", "POST");
  validate_normalize("pUt", "PUT");
  validate_normalize("anythingElse", "anythingElse");
}

TEST(ServiceWorkerRouterTypeConverterTest, RunningStatus) {
  auto verify =
      [](blink::V8RunningStatusEnum::Enum idl_status,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum
             blink_status) {
        auto* idl_rule = blink::RouterRule::Create();
        auto* idl_condition = blink::RouterCondition::Create();
        idl_condition->setRunningStatus(idl_status);
        idl_rule->setCondition(idl_condition);
        idl_rule->setSource(blink::V8RouterSourceEnum::Enum::kNetwork);

        blink::ServiceWorkerRouterRule expected_rule;
        blink::ServiceWorkerRouterCondition expected_condition;
        expected_condition.type =
            blink::ServiceWorkerRouterCondition::ConditionType::kRunningStatus;
        blink::ServiceWorkerRouterRunningStatusCondition expected_status;
        expected_status.status = blink_status;
        expected_condition.running_status = std::move(expected_status);
        expected_rule.conditions.emplace_back(expected_condition);
        blink::ServiceWorkerRouterSource expected_source;
        expected_source.type =
            blink::ServiceWorkerRouterSource::SourceType::kNetwork;
        expected_source.network_source.emplace();
        expected_rule.sources.emplace_back(expected_source);

        V8TestingScope scope;
        auto blink_rule =
            ConvertV8RouterRuleToBlink(idl_rule, scope.GetExceptionState());
        EXPECT_FALSE(scope.GetExceptionState().HadException());
        EXPECT_TRUE(blink_rule.has_value());
        EXPECT_EQ(expected_rule, *blink_rule);
      };
  verify(blink::V8RunningStatusEnum::Enum::kRunning,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kRunning);
  verify(blink::V8RunningStatusEnum::Enum::kNotRunning,
         blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kNotRunning);
}

}  // namespace

}  // namespace blink
