// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/set.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceWorkerRouterEvaluatorErrorEnums {
  kNoError = 0,
  kInvalidType = 1,
  kParseError = 2,
  kCompileError = 3,
  kEmptyCondition = 4,
  kEmptySource = 5,
  kInvalidSource = 6,

  kMaxValue = kInvalidSource,
};

void RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums e) {
  base::UmaHistogramEnumeration("ServiceWorker.RouterEvaluator.Error", e);
}

void RecordMatchedSourceType(
    const std::vector<blink::ServiceWorkerRouterSource>& sources) {
  base::UmaHistogramEnumeration(
      "ServiceWorker.RouterEvaluator.MatchedFirstSourceType", sources[0].type);
}

std::string ConvertToRegex(const blink::UrlPattern& url_pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  VLOG(3) << "regex string:" << pattern.GenerateRegexString();
  return pattern.GenerateRegexString();
}

std::string ConvertToPattern(const blink::UrlPattern& url_pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  return pattern.GeneratePatternString();
}

bool IsValidSources(
    const std::vector<blink::ServiceWorkerRouterSource> sources) {
  if (sources.empty()) {
    // At least a source must exist.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kEmptySource);
    return false;
  }
  // TODO(crbug.com/1371756): support other sources in the future.
  // Currently, only network source is supported.
  for (const auto& s : sources) {
    if (s.type != blink::ServiceWorkerRouterSource::SourceType::kNetwork ||
        !s.network_source) {
      RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
      return false;
    }
  }
  return true;
}

}  // namespace

namespace content {

struct ServiceWorkerRouterEvaluator::RouterRule {
  RouterRule()
      : url_patterns(RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED)) {}
  ~RouterRule() = default;

  RE2::Set url_patterns;
  size_t url_pattern_length = 0;
  std::vector<blink::ServiceWorkerRouterSource> sources;
};

ServiceWorkerRouterEvaluator::ServiceWorkerRouterEvaluator(
    blink::ServiceWorkerRouterRules rules)
    : rules_(std::move(rules)) {
  Compile();
}
ServiceWorkerRouterEvaluator::~ServiceWorkerRouterEvaluator() = default;

void ServiceWorkerRouterEvaluator::Compile() {
  for (const auto& r : rules_.rules) {
    std::unique_ptr<RouterRule> rule = absl::make_unique<RouterRule>();
    if (r.conditions.empty()) {
      // At least one condition must be set.
      RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kEmptyCondition);
      return;
    }
    for (const auto& condition : r.conditions) {
      if (condition.type !=
          blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern) {
        // Unexpected condition type.
        RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kInvalidType);
        return;
      }
      if (rule->url_patterns.Add(ConvertToRegex(*condition.url_pattern),
                                 nullptr) == -1) {
        // Failed to parse the regex.
        RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kParseError);
        return;
      }
    }
    rule->url_pattern_length = r.conditions.size();
    if (!rule->url_patterns.Compile()) {
      // Failed to compile the regex.
      RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kCompileError);
      return;
    }
    if (!IsValidSources(r.sources)) {
      return;
    }
    rule->sources = r.sources;
    compiled_rules_.emplace_back(std::move(rule));
  }
  RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kNoError);
  is_valid_ = true;
}

std::vector<blink::ServiceWorkerRouterSource>
ServiceWorkerRouterEvaluator::Evaluate(
    const network::ResourceRequest& request) const {
  CHECK(is_valid_);
  for (const auto& rule : compiled_rules_) {
    std::vector<int> vec;
    if (rule->url_patterns.Match(request.url.path(), &vec) &&
        // ensure it matches all included patterns.
        vec.size() == rule->url_pattern_length) {
      RecordMatchedSourceType(rule->sources);
      return rule->sources;
    }
  }
  return std::vector<blink::ServiceWorkerRouterSource>();
}

base::Value ServiceWorkerRouterEvaluator::ToValue() const {
  base::Value::List out;
  for (const auto& r : rules_.rules) {
    base::Value::Dict rule;
    base::Value::List condition;
    base::Value::List source;
    for (const auto& c : r.conditions) {
      base::Value::Dict out_c;
      CHECK_EQ(c.type,
               blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern);
      out_c.Set("urlPattern", ConvertToPattern(*c.url_pattern));
      condition.Append(std::move(out_c));
    }
    for (const auto& s : r.sources) {
      CHECK_EQ(s.type, blink::ServiceWorkerRouterSource::SourceType::kNetwork);
      source.Append("network");
    }
    rule.Set("condition", std::move(condition));
    rule.Set("source", std::move(source));
    out.Append(std::move(rule));
  }
  return base::Value(std::move(out));
}

std::string ServiceWorkerRouterEvaluator::ToString() const {
  std::string json;
  base::JSONWriter::Write(ToValue(), &json);
  return json;
}

}  // namespace content
