// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_condition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source_enum.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_routersource_routersourceenum.h"
#include "third_party/blink/renderer/core/fetch/request_util.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

namespace {

absl::optional<std::vector<liburlpattern::Part>> ToPartList(
    const String& pattern,
    const String& field_name,
    ExceptionState& exception_state) {
  // This means that the field should not exist.
  if (pattern.empty()) {
    return std::vector<liburlpattern::Part>();
  }
  // TODO(crbug.com/1371756): unify the code with manifest_parser.cc
  WTF::StringUTF8Adaptor utf8(pattern);
  auto parse_result = liburlpattern::Parse(
      base::StringPiece(utf8.data(), utf8.size()),
      [](base::StringPiece input) { return std::string(input); });
  if (!parse_result.ok()) {
    exception_state.ThrowTypeError("Failed to parse URLPattern '" + pattern +
                                   "'");
    return absl::nullopt;
  }
  std::vector<liburlpattern::Part> part_list;
  for (auto& part : parse_result.value().PartList()) {
    // We don't allow custom regex for security reasons as this will be used
    // in the browser process.
    if (part.type == liburlpattern::PartType::kRegex) {
      DLOG(INFO) << "regex URLPattern is not allowed as Router Condition";
      exception_state.ThrowTypeError("Used regular expression in '" + pattern +
                                     "', which is prohibited.");
      return absl::nullopt;
    }
    part_list.push_back(std::move(part));
  }
  return part_list;
}

// TODO(crbug.com/1371756): Make URLPattern has a method to construct the
// SafeURLPattern and remove the conversion from here.
// The method should take a exception_state to raise on regex usage.
absl::optional<SafeUrlPattern> ConvertURLPatternInputToSafeUrlPattern(
    const V8URLPatternInput* url_pattern_input,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  URLPattern* url_pattern;
  switch (url_pattern_input->GetContentType()) {
    case blink::V8URLPatternInput::ContentType::kUSVString:
      url_pattern = URLPattern::Create(url_pattern_input, url_pattern_base_url,
                                       exception_state);
      break;
    case blink::V8URLPatternInput::ContentType::kURLPatternInit:
      url_pattern = URLPattern::Create(url_pattern_input, exception_state);
      break;
  }
  if (!url_pattern) {
    CHECK(exception_state.HadException());
    return absl::nullopt;
  }

  SafeUrlPattern safe_url_pattern;
#define TO_PART(field)                                             \
  do {                                                             \
    auto part_list =                                               \
        ToPartList(url_pattern->field(), #field, exception_state); \
    if (!part_list.has_value()) {                                  \
      CHECK(exception_state.HadException());                       \
      return absl::nullopt;                                        \
    }                                                              \
    safe_url_pattern.field = std::move(*part_list);                \
  } while (0)
  TO_PART(protocol);
  TO_PART(username);
  TO_PART(password);
  TO_PART(hostname);
  TO_PART(port);
  TO_PART(pathname);
  TO_PART(search);
  TO_PART(hash);
#undef TO_PART
  // TODO(crbug.com/1371756): support URLPatternOptions.
  // Currently, URLPatternOptions are not included in URLPatternInit,
  // and we do not pass the option to the browser side.
  return safe_url_pattern;
}

absl::optional<ServiceWorkerRouterCondition> RouterUrlPatternConditionToBlink(
    RouterCondition* v8_condition,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  CHECK(v8_condition);

  absl::optional<SafeUrlPattern> url_pattern =
      ConvertURLPatternInputToSafeUrlPattern(
          v8_condition->urlPattern(), url_pattern_base_url, exception_state);
  if (!url_pattern) {
    CHECK(exception_state.HadException());
    return absl::nullopt;
  }
  blink::ServiceWorkerRouterCondition condition;
  condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  condition.url_pattern = std::move(*url_pattern);
  return condition;
}

absl::optional<ServiceWorkerRouterCondition> RouterRequestConditionToBlink(
    RouterCondition* v8_condition,
    ExceptionState& exception_state) {
  CHECK(v8_condition);
  bool request_condition_exist = false;
  ServiceWorkerRouterRequestCondition request;
  if (v8_condition->hasRequestMethod()) {
    request_condition_exist = true;
    request.method =
        FetchUtils::NormalizeMethod(AtomicString(v8_condition->requestMethod()))
            .Latin1();
  }
  if (v8_condition->hasRequestMode()) {
    request_condition_exist = true;
    request.mode = V8RequestModeToMojom(v8_condition->requestMode());
  }
  if (v8_condition->hasRequestDestination()) {
    request_condition_exist = true;
    request.destination =
        V8RequestDestinationToMojom(v8_condition->requestDestination());
  }

  if (!request_condition_exist) {
    exception_state.ThrowTypeError("Request condition should not be empty.");
    return absl::nullopt;
  }
  ServiceWorkerRouterCondition condition;
  condition.type = ServiceWorkerRouterCondition::ConditionType::kRequest;
  condition.request = std::move(request);
  return condition;
}

absl::optional<ServiceWorkerRouterCondition>
RouterRunningStatusConditionToBlink(RouterCondition* v8_condition,
                                    ExceptionState& exception_state) {
  CHECK(v8_condition);
  if (!v8_condition->hasRunningStatus()) {
    exception_state.ThrowTypeError(
        "RunningState condition should not be empty.");
    return absl::nullopt;
  }

  ServiceWorkerRouterRunningStatusCondition running_status;
  switch (v8_condition->runningStatus().AsEnum()) {
    case V8RunningStatusEnum::Enum::kRunning:
      running_status.status = ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kRunning;
      break;
    case V8RunningStatusEnum::Enum::kNotRunning:
      running_status.status = ServiceWorkerRouterRunningStatusCondition::
          RunningStatusEnum::kNotRunning;
      break;
  }
  ServiceWorkerRouterCondition condition;
  condition.type = ServiceWorkerRouterCondition::ConditionType::kRunningStatus;
  condition.running_status = std::move(running_status);
  return condition;
}

ServiceWorkerRouterSource RouterSourceEnumToBlink(
    V8RouterSourceEnum v8_source_enum) {
  switch (v8_source_enum.AsEnum()) {
    case V8RouterSourceEnum::Enum::kNetwork: {
      ServiceWorkerRouterSource source;
      source.type = ServiceWorkerRouterSource::SourceType::kNetwork;
      source.network_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kRaceNetworkAndFetchHandler: {
      ServiceWorkerRouterSource source;
      source.type = ServiceWorkerRouterSource::SourceType::kRace;
      source.race_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kFetchEvent: {
      ServiceWorkerRouterSource source;
      source.type = ServiceWorkerRouterSource::SourceType::kFetchEvent;
      source.fetch_event_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kCache: {
      ServiceWorkerRouterSource source;
      source.type = ServiceWorkerRouterSource::SourceType::kCache;
      source.cache_source.emplace();
      return source;
    }
  }
}

absl::optional<ServiceWorkerRouterSource> RouterSourceToBlink(
    const RouterSource* v8_source,
    ExceptionState& exception_state) {
  if (!v8_source) {
    exception_state.ThrowTypeError("Invalid source input");
    return absl::nullopt;
  }
  ServiceWorkerRouterSource source;
  if (v8_source->hasCacheName()) {
    source.type = ServiceWorkerRouterSource::SourceType::kCache;
    ServiceWorkerRouterCacheSource cache_source;
    cache_source.cache_name = AtomicString(v8_source->cacheName()).Latin1();
    source.cache_source = std::move(cache_source);
    return source;
  }
  exception_state.ThrowTypeError(
      "Got a dictionary for source but no field is set");
  return absl::nullopt;
}

absl::optional<ServiceWorkerRouterSource> RouterSourceInputToBlink(
    const V8RouterSourceInput* router_source_input,
    ExceptionState& exception_state) {
  switch (router_source_input->GetContentType()) {
    case blink::V8RouterSourceInput::ContentType::kRouterSourceEnum:
      return RouterSourceEnumToBlink(
          router_source_input->GetAsRouterSourceEnum());
    case blink::V8RouterSourceInput::ContentType::kRouterSource:
      return RouterSourceToBlink(router_source_input->GetAsRouterSource(),
                                 exception_state);
  }
}

}  // namespace

absl::optional<ServiceWorkerRouterRule> ConvertV8RouterRuleToBlink(
    const RouterRule* input,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  if (!input) {
    exception_state.ThrowTypeError("Invalid Input");
    return absl::nullopt;
  }

  if (!input->condition()) {
    exception_state.ThrowTypeError("No input condition has been set.");
    return absl::nullopt;
  }
  ServiceWorkerRouterRule rule;
  // Set up conditions.
  if (input->condition()->hasUrlPattern()) {
    absl::optional<ServiceWorkerRouterCondition> condition;
    condition = RouterUrlPatternConditionToBlink(
        input->condition(), url_pattern_base_url, exception_state);
    if (!condition) {
      CHECK(exception_state.HadException());
      return absl::nullopt;
    }
    rule.conditions.emplace_back(*condition);
  }
  if (input->condition()->hasRequestMethod() ||
      input->condition()->hasRequestMode() ||
      input->condition()->hasRequestDestination()) {
    absl::optional<ServiceWorkerRouterCondition> condition;
    condition =
        RouterRequestConditionToBlink(input->condition(), exception_state);
    if (!condition) {
      CHECK(exception_state.HadException());
      return absl::nullopt;
    }
    rule.conditions.emplace_back(*condition);
  }
  if (input->condition()->hasRunningStatus()) {
    absl::optional<ServiceWorkerRouterCondition> condition;
    condition = RouterRunningStatusConditionToBlink(input->condition(),
                                                    exception_state);
    if (!condition) {
      CHECK(exception_state.HadException());
      return absl::nullopt;
    }
    rule.conditions.emplace_back(*condition);
  }
  if (rule.conditions.empty()) {
    // At least one condition should exist per rule.
    exception_state.ThrowTypeError(
        "At least one condition must be set, but no condition has been set "
        "to the rule.");
    return absl::nullopt;
  }

  // Set up sources.
  // TODO(crbug.com/1371756): support multiple sources.
  // i.e. support full form shown in
  // https://github.com/yoshisatoyanagisawa/service-worker-static-routing-api/blob/main/final-form.md
  //
  // https://github.com/yoshisatoyanagisawa/service-worker-static-routing-api/blob/main/README.md
  // explains the first step. It does not cover cases sequence of sources
  // are set. The current IDL has been implemented for this level, but
  // the mojo IPC has been implemented to support the final form.
  auto source = RouterSourceInputToBlink(input->source(), exception_state);
  if (!source) {
    CHECK(exception_state.HadException());
    return absl::nullopt;
  }
  rule.sources.emplace_back(*source);
  return rule;
}

}  // namespace blink
