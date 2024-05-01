// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpattern_urlpatterninit_usvstring.h"
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

std::optional<ServiceWorkerRouterCondition> RouterConditionToBlink(
    v8::Isolate* isolate,
    RouterCondition* v8_condition,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state);

[[nodiscard]] bool ExceedsMaxConditionDepth(const RouterCondition* v8_condition,
                                            ExceptionState& exception_state,
                                            int depth = 0) {
  CHECK(v8_condition);
  if (depth >= blink::kServiceWorkerRouterConditionMaxRecursionDepth) {
    exception_state.ThrowTypeError("Conditions are nested too much");
    return true;
  }
  if (v8_condition->hasOrConditions()) {
    for (const auto& v8_ob : v8_condition->orConditions()) {
      if (ExceedsMaxConditionDepth(v8_ob, exception_state, depth + 1)) {
        CHECK(exception_state.HadException());
        return true;
      }
    }
  }
  if (v8_condition->hasNotCondition()) {
    if (ExceedsMaxConditionDepth(v8_condition->notCondition(), exception_state,
                                 depth + 1)) {
      CHECK(exception_state.HadException());
      return true;
    }
  }
  return false;
}

std::optional<SafeUrlPattern> RouterUrlPatternConditionToBlink(
    v8::Isolate* isolate,
    const V8URLPatternCompatible* url_pattern_compatible,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  // If |url_pattern_compatible| is not a constructed URLPattern,
  // |url_pattern_base_url| as baseURL will give additional information to
  // appropriately complement missing fields. For more details, see
  // https://urlpattern.spec.whatwg.org/#other-specs-javascript.
  //
  // note: The empty string passname may result in an unintuitive output,
  // because the step 17 in 3.2. URLPatternInit processing will make the new
  // pathname field be a substring from 0 to slash index + 1 within baseURLPath.
  // https://urlpattern.spec.whatwg.org/#canon-processing-for-init
  URLPattern* url_pattern = URLPattern::From(
      isolate, url_pattern_compatible, url_pattern_base_url, exception_state);
  if (!url_pattern) {
    CHECK(exception_state.HadException());
    return std::nullopt;
  }

  std::optional<SafeUrlPattern> safe_url_pattern =
      url_pattern->ToSafeUrlPattern(exception_state);
  if (!safe_url_pattern) {
    CHECK(exception_state.HadException());
    return std::nullopt;
  }
  return safe_url_pattern;
}

std::optional<ServiceWorkerRouterRequestCondition>
RouterRequestConditionToBlink(RouterCondition* v8_condition,
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
    return std::nullopt;
  }
  return request;
}

std::optional<ServiceWorkerRouterRunningStatusCondition>
RouterRunningStatusConditionToBlink(RouterCondition* v8_condition,
                                    ExceptionState& exception_state) {
  CHECK(v8_condition);
  if (!v8_condition->hasRunningStatus()) {
    exception_state.ThrowTypeError(
        "RunningState condition should not be empty.");
    return std::nullopt;
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
  return running_status;
}

std::optional<ServiceWorkerRouterOrCondition> RouterOrConditionToBlink(
    v8::Isolate* isolate,
    RouterCondition* v8_condition,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  ServiceWorkerRouterOrCondition or_condition;
  const auto& v8_objects = v8_condition->orConditions();
  or_condition.conditions.reserve(v8_objects.size());
  for (auto&& v8_ob : v8_objects) {
    std::optional<ServiceWorkerRouterCondition> c = RouterConditionToBlink(
        isolate, v8_ob, url_pattern_base_url, exception_state);
    if (!c) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
    or_condition.conditions.emplace_back(std::move(*c));
  }
  return or_condition;
}

std::optional<ServiceWorkerRouterNotCondition> RouterNotConditionToBlink(
    v8::Isolate* isolate,
    RouterCondition* v8_condition,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  std::optional<ServiceWorkerRouterCondition> c =
      RouterConditionToBlink(isolate, v8_condition->notCondition(),
                             url_pattern_base_url, exception_state);
  if (!c) {
    CHECK(exception_state.HadException());
    return std::nullopt;
  }
  ServiceWorkerRouterNotCondition not_condition;
  not_condition.condition =
      std::make_unique<blink::ServiceWorkerRouterCondition>(*c);
  return not_condition;
}

std::optional<ServiceWorkerRouterCondition> RouterConditionToBlink(
    v8::Isolate* isolate,
    RouterCondition* v8_condition,
    const KURL& url_pattern_base_url,
    ExceptionState& exception_state) {
  std::optional<SafeUrlPattern> url_pattern;
  if (v8_condition->hasUrlPattern()) {
    url_pattern =
        RouterUrlPatternConditionToBlink(isolate, v8_condition->urlPattern(),
                                         url_pattern_base_url, exception_state);
    if (!url_pattern.has_value()) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
  }
  std::optional<ServiceWorkerRouterRequestCondition> request;
  if (v8_condition->hasRequestMethod() || v8_condition->hasRequestMode() ||
      v8_condition->hasRequestDestination()) {
    request = RouterRequestConditionToBlink(v8_condition, exception_state);
    if (!request.has_value()) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
  }
  std::optional<ServiceWorkerRouterRunningStatusCondition> running_status;
  if (v8_condition->hasRunningStatus()) {
    running_status =
        RouterRunningStatusConditionToBlink(v8_condition, exception_state);
    if (!running_status.has_value()) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
  }
  std::optional<ServiceWorkerRouterOrCondition> or_condition;
  if (v8_condition->hasOrConditions()) {
    // Not checking here for the `or` is actually exclusive.
    or_condition = RouterOrConditionToBlink(
        isolate, v8_condition, url_pattern_base_url, exception_state);
    if (!or_condition.has_value()) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
  }
  std::optional<ServiceWorkerRouterNotCondition> not_condition;
  if (v8_condition->hasNotCondition()) {
    if (!base::FeatureList::IsEnabled(
            features::kServiceWorkerStaticRouterNotConditionEnabled)) {
      exception_state.ThrowTypeError("The 'not' condition is not enabled.");
      return std::nullopt;
    }
    // Not checking here for the `not` is actually exclusive.
    not_condition = RouterNotConditionToBlink(
        isolate, v8_condition, url_pattern_base_url, exception_state);
    if (!not_condition.has_value()) {
      CHECK(exception_state.HadException());
      return std::nullopt;
    }
  }
  blink::ServiceWorkerRouterCondition ret(url_pattern, request, running_status,
                                          or_condition, not_condition);
  if (ret.IsEmpty()) {
    // At least one condition should exist per rule.
    exception_state.ThrowTypeError(
        "At least one condition must be set, but no condition has been set "
        "to the rule.");
    return std::nullopt;
  }
  if (!ret.IsOrConditionExclusive()) {
    // `or` condition must be exclusive.
    exception_state.ThrowTypeError(
        "Cannot set other conditions when the `or` condition is specified");
    return std::nullopt;
  }
  if (!ret.IsNotConditionExclusive()) {
    // `not` condition must be exclusive.
    exception_state.ThrowTypeError(
        "Cannot set other conditions when the `not` condition is specified");
    return std::nullopt;
  }
  return ret;
}

std::optional<ServiceWorkerRouterSource> RouterSourceEnumToBlink(
    V8RouterSourceEnum v8_source_enum,
    mojom::blink::ServiceWorkerFetchHandlerType fetch_handler_type,
    ExceptionState& exception_state) {
  switch (v8_source_enum.AsEnum()) {
    case V8RouterSourceEnum::Enum::kNetwork: {
      ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
      source.network_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kRaceNetworkAndFetchHandler: {
      ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kRace;
      source.race_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kFetchEvent: {
      if (fetch_handler_type ==
          mojom::blink::ServiceWorkerFetchHandlerType::kNoHandler) {
        exception_state.ThrowTypeError(
            "fetch-event source is specified without a fetch handler");
        return std::nullopt;
      }
      ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
      source.fetch_event_source.emplace();
      return source;
    }
    case V8RouterSourceEnum::Enum::kCache: {
      ServiceWorkerRouterSource source;
      source.type = network::mojom::ServiceWorkerRouterSourceType::kCache;
      source.cache_source.emplace();
      return source;
    }
  }
}

std::optional<ServiceWorkerRouterSource> RouterSourceToBlink(
    const RouterSource* v8_source,
    ExceptionState& exception_state) {
  if (!v8_source) {
    exception_state.ThrowTypeError("Invalid source input");
    return std::nullopt;
  }
  ServiceWorkerRouterSource source;
  if (v8_source->hasCacheName()) {
    source.type = network::mojom::ServiceWorkerRouterSourceType::kCache;
    ServiceWorkerRouterCacheSource cache_source;
    cache_source.cache_name = AtomicString(v8_source->cacheName()).Latin1();
    source.cache_source = std::move(cache_source);
    return source;
  }
  exception_state.ThrowTypeError(
      "Got a dictionary for source but no field is set");
  return std::nullopt;
}

std::optional<ServiceWorkerRouterSource> RouterSourceInputToBlink(
    const V8RouterSourceInput* router_source_input,
    mojom::blink::ServiceWorkerFetchHandlerType fetch_handler_type,
    ExceptionState& exception_state) {
  switch (router_source_input->GetContentType()) {
    case blink::V8RouterSourceInput::ContentType::kRouterSourceEnum:
      return RouterSourceEnumToBlink(
          router_source_input->GetAsRouterSourceEnum(), fetch_handler_type,
          exception_state);
    case blink::V8RouterSourceInput::ContentType::kRouterSource:
      return RouterSourceToBlink(router_source_input->GetAsRouterSource(),
                                 exception_state);
  }
}

}  // namespace

std::optional<ServiceWorkerRouterRule> ConvertV8RouterRuleToBlink(
    v8::Isolate* isolate,
    const RouterRule* input,
    const KURL& url_pattern_base_url,
    mojom::blink::ServiceWorkerFetchHandlerType fetch_handler_type,
    ExceptionState& exception_state) {
  if (!input) {
    exception_state.ThrowTypeError("Invalid Input");
    return std::nullopt;
  }

  if (!input->hasCondition()) {
    exception_state.ThrowTypeError("No input condition has been set.");
    return std::nullopt;
  }
  ServiceWorkerRouterRule rule;
  // Set up conditions.
  if (ExceedsMaxConditionDepth(input->condition(), exception_state)) {
    CHECK(exception_state.HadException());
    return std::nullopt;
  }
  std::optional<ServiceWorkerRouterCondition> condition =
      RouterConditionToBlink(isolate, input->condition(), url_pattern_base_url,
                             exception_state);
  if (!condition.has_value()) {
    return std::nullopt;
  }
  rule.condition = std::move(*condition);

  // Set up sources.
  // TODO(crbug.com/1371756): support multiple sources.
  // i.e. support full form shown in
  // https://github.com/WICG/service-worker-static-routing-api/blob/main/final-form.md
  //
  // The ServiceWorker specification (https://w3c.github.io/ServiceWorker/)
  // does not cover cases sequence of sources are set. The current IDL has
  // been implemented for this level, but the mojo IPC has been implemented
  // to support the final form.
  if (!input->hasSource()) {
    exception_state.ThrowTypeError("No input source has been set.");
    return std::nullopt;
  }
  const std::optional<ServiceWorkerRouterSource> source =
      RouterSourceInputToBlink(input->source(), fetch_handler_type,
                               exception_state);
  if (!source.has_value()) {
    CHECK(exception_state.HadException());
    return std::nullopt;
  }
  rule.sources.emplace_back(*source);
  return rule;
}

}  // namespace blink
