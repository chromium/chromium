// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
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
  kInvalidCondition = 7,

  kMaxValue = kInvalidCondition,
};

void RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums e) {
  base::UmaHistogramEnumeration("ServiceWorker.RouterEvaluator.Error", e);
}

void RecordMatchedSourceType(
    const std::vector<blink::ServiceWorkerRouterSource>& sources) {
  base::UmaHistogramEnumeration(
      "ServiceWorker.RouterEvaluator.MatchedFirstSourceType", sources[0].type);
}

std::string ConvertToRegex(const blink::SafeUrlPattern& url_pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  VLOG(3) << "regex string:" << pattern.GenerateRegexString();
  return pattern.GenerateRegexString();
}

std::string ConvertToPattern(const blink::SafeUrlPattern& url_pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  return pattern.GeneratePatternString();
}

base::Value RequestToValue(
    const blink::ServiceWorkerRouterRequestCondition& request) {
  base::Value::Dict ret;
  if (request.method) {
    ret.Set("method", *request.method);
  }
  if (request.mode) {
    ret.Set("mode", network::RequestModeToString(*request.mode));
  }
  if (request.destination) {
    ret.Set("destination",
            network::RequestDestinationToString(*request.destination));
  }
  return base::Value(std::move(ret));
}

bool IsValidCondition(const blink::ServiceWorkerRouterCondition& condition) {
  switch (condition.type) {
    case blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern:
      return condition.url_pattern.has_value();
    case blink::ServiceWorkerRouterCondition::ConditionType::kRequest:
      return condition.request.has_value() &&
             (condition.request->method.has_value() ||
              condition.request->mode.has_value() ||
              condition.request->destination.has_value());
  }
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
    switch (s.type) {
      case blink::ServiceWorkerRouterSource::SourceType::kNetwork:
        if (!s.network_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
      case blink::ServiceWorkerRouterSource::SourceType::kRace:
        if (!s.race_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
      case blink::ServiceWorkerRouterSource::SourceType::kFetchEvent:
        if (!s.fetch_event_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
    }
  }
  return true;
}

bool IsMatchedRequest(const blink::ServiceWorkerRouterRequestCondition& pattern,
                      const network::ResourceRequest& request) {
  if (pattern.method && *pattern.method != request.method) {
    return false;
  }
  if (pattern.mode && *pattern.mode != request.mode) {
    return false;
  }
  if (pattern.destination && *pattern.destination != request.destination) {
    return false;
  }
  return true;
}

}  // namespace

namespace content {

class ServiceWorkerRouterEvaluator::RouterRule {
 public:
  RouterRule()
      : url_patterns_(RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED)) {}
  ~RouterRule() = default;
  bool SetRule(const blink::ServiceWorkerRouterRule& rule);
  bool IsConditionMatched(const network::ResourceRequest& request) const;
  const std::vector<blink::ServiceWorkerRouterSource>& sources() const {
    return sources_;
  }

 private:
  // Returns true on success. Otherwise, false.
  bool SetConditions(
      const std::vector<blink::ServiceWorkerRouterCondition>& conditions);
  // Returns true on success. Otherwise, false.
  bool SetSources(
      const std::vector<blink::ServiceWorkerRouterSource>& sources) {
    if (!IsValidSources(sources)) {
      return false;
    }
    sources_ = sources;
    return true;
  }
  bool IsUrlPatternConditionMatched(
      const network::ResourceRequest& request) const;
  bool IsNonUrlPatternConditionMatched(
      const network::ResourceRequest& request) const;
  // To process SafeUrlPattern faster, the all patterns are combined to the
  // `RE::Set` and compiled when `ServiceWorkerRouterEvaluator` is initialized.
  RE2::Set url_patterns_;
  size_t url_pattern_length_ = 0;
  // Non-SafeUrlPattern conditions are processed one by one.
  std::vector<blink::ServiceWorkerRouterCondition> non_url_pattern_conditions_;
  std::vector<blink::ServiceWorkerRouterSource> sources_;
};

bool ServiceWorkerRouterEvaluator::RouterRule::SetRule(
    const blink::ServiceWorkerRouterRule& rule) {
  return SetConditions(rule.conditions) && SetSources(rule.sources);
}

bool ServiceWorkerRouterEvaluator::RouterRule::SetConditions(
    const std::vector<blink::ServiceWorkerRouterCondition>& conditions) {
  if (conditions.empty()) {
    // At least one condition must be set.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kEmptyCondition);
    return false;
  }
  for (const auto& condition : conditions) {
    if (!IsValidCondition(condition)) {
      RecordSetupError(
          ServiceWorkerRouterEvaluatorErrorEnums::kInvalidCondition);
      return false;
    }
    if (condition.type !=
        blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern) {
      non_url_pattern_conditions_.push_back(condition);
      continue;
    }

    // Code for SafeUrlPattern conditions.
    if (url_patterns_.Add(ConvertToRegex(*condition.url_pattern), nullptr) ==
        -1) {
      // Failed to parse the regex.
      RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kParseError);
      return false;
    }
    // Counts SafeUrlPattern conditions to ensure all conditions are matched.
    ++url_pattern_length_;
  }
  if (!url_patterns_.Compile()) {
    // Failed to compile the regex.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kCompileError);
    return false;
  }
  return true;
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsConditionMatched(
    const network::ResourceRequest& request) const {
  return IsUrlPatternConditionMatched(request) &&
         IsNonUrlPatternConditionMatched(request);
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsUrlPatternConditionMatched(
    const network::ResourceRequest& request) const {
  if (url_pattern_length_ == 0) {
    // No SafeUrlPattern conditions, which should be considered as success.
    return true;
  }

  std::vector<int> vec;
  if (!url_patterns_.Match(request.url.path(), &vec)) {
    return false;
  }
  // ensure it matches all included patterns.
  if (vec.size() != url_pattern_length_) {
    return false;
  }
  return true;
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsNonUrlPatternConditionMatched(
    const network::ResourceRequest& request) const {
  for (const auto& c : non_url_pattern_conditions_) {
    switch (c.type) {
      case blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern:
        NOTREACHED_NORETURN()
            << "UrlPattern should be separated in the compile time.";
      case blink::ServiceWorkerRouterCondition::ConditionType::kRequest:
        if (!IsMatchedRequest(*c.request, request)) {
          return false;
        }
        break;
    }
  }
  return true;
}

ServiceWorkerRouterEvaluator::ServiceWorkerRouterEvaluator(
    blink::ServiceWorkerRouterRules rules)
    : rules_(std::move(rules)) {
  Compile();
}
ServiceWorkerRouterEvaluator::~ServiceWorkerRouterEvaluator() = default;

void ServiceWorkerRouterEvaluator::Compile() {
  for (const auto& r : rules_.rules) {
    std::unique_ptr<RouterRule> rule = absl::make_unique<RouterRule>();
    if (!rule->SetRule(r)) {
      return;
    }
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
    if (rule->IsConditionMatched(request)) {
      RecordMatchedSourceType(rule->sources());
      return rule->sources();
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
      switch (c.type) {
        case blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern: {
          base::Value::Dict out_c;
          out_c.Set("urlPattern", ConvertToPattern(*c.url_pattern));
          condition.Append(std::move(out_c));
          break;
        }
        case blink::ServiceWorkerRouterCondition::ConditionType::kRequest: {
          base::Value::Dict out_c;
          out_c.Set("request", RequestToValue(*c.request));
          condition.Append(std::move(out_c));
          break;
        }
      }
    }
    for (const auto& s : r.sources) {
      switch (s.type) {
        case blink::ServiceWorkerRouterSource::SourceType::kNetwork:
          source.Append("network");
          break;
        case blink::ServiceWorkerRouterSource::SourceType::kRace:
          // TODO(crbug.com/1371756): we may need to update the name per target.
          source.Append("race-network-and-fetch-handler");
          break;
        case blink::ServiceWorkerRouterSource::SourceType::kFetchEvent:
          source.Append("fetch-event");
          break;
      }
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
