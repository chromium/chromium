// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_router_condition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_rule.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_source_enum.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_router_url_pattern_condition.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace {

absl::optional<blink::ServiceWorkerRouterCondition>
RouterUrlPatternConditionToBlink(
    blink::RouterUrlPatternCondition* v8_condition) {
  if (!v8_condition) {
    // No UrlCondition configured.
    return absl::nullopt;
  }
  if (v8_condition->urlPattern().empty()) {
    // No URLPattern configured.
    return absl::nullopt;
  }
  // TODO(crbug.com/1371756): unify the code with manifest_parser.cc
  WTF::StringUTF8Adaptor utf8(v8_condition->urlPattern());
  auto parse_result = liburlpattern::Parse(
      base::StringPiece(utf8.data(), utf8.size()),
      [](base::StringPiece input) { return std::string(input); });
  if (!parse_result.ok()) {
    return absl::nullopt;
  }
  std::vector<liburlpattern::Part> part_list;
  for (auto& part : parse_result.value().PartList()) {
    // We don't allow custom regex for security reasons as this will be used
    // in the browser process.
    if (part.type == liburlpattern::PartType::kRegex) {
      DLOG(INFO) << "regex URLPattern is not allowed as Router Condition";
      return absl::nullopt;
    }
    part_list.push_back(std::move(part));
  }
  blink::ServiceWorkerRouterCondition condition;
  condition.type =
      blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern;
  blink::UrlPattern url_pattern;
  url_pattern.pathname = std::move(part_list);
  condition.url_pattern = std::move(url_pattern);
  return condition;
}

absl::optional<blink::ServiceWorkerRouterSource> RouterSourceEnumToBlink(
    blink::V8RouterSourceEnum v8_source_enum) {
  if (v8_source_enum != blink::V8RouterSourceEnum::Enum::kNetwork) {
    return absl::nullopt;
  }
  blink::ServiceWorkerRouterSource source;
  source.type = blink::ServiceWorkerRouterSource::SourceType::kNetwork;
  source.network_source.emplace();
  return source;
}

}  // namespace

namespace mojo {

absl::optional<blink::ServiceWorkerRouterRule>
TypeConverter<absl::optional<blink::ServiceWorkerRouterRule>,
              blink::RouterRule*>::Convert(const blink::RouterRule* input) {
  if (!input) {
    return absl::nullopt;
  }

  blink::ServiceWorkerRouterRule rule;
  absl::optional<blink::ServiceWorkerRouterCondition> condition =
      RouterUrlPatternConditionToBlink(input->condition());
  if (!condition) {
    return absl::nullopt;
  }
  absl::optional<blink::ServiceWorkerRouterSource> source =
      RouterSourceEnumToBlink(input->source());
  if (!source) {
    return absl::nullopt;
  }

  // TODO(crbug.com/1371756): support multiple conditions and sources.
  // i.e. support full form shown in
  // https://github.com/yoshisatoyanagisawa/service-worker-static-routing-api/blob/main/final-form.md
  //
  // https://github.com/yoshisatoyanagisawa/service-worker-static-routing-api/blob/main/README.md
  // explains the first step. It does not cover cases sequence of conditions or
  // sources are set. The current IDL has been implemented for this level, but
  // the mojo IPC has been implemented to support the final form.
  rule.conditions.emplace_back(*condition);
  rule.sources.emplace_back(*source);
  return rule;
}

}  // namespace mojo
