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

absl::optional<std::string> PathnameConvertToRegex(
    const blink::SafeUrlPattern& url_pattern) {
  if (url_pattern.pathname.empty()) {
    return absl::nullopt;
  }
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  VLOG(3) << "path regex string:" << pattern.GenerateRegexString();
  return pattern.GenerateRegexString();
}

absl::optional<std::string> HostnameConvertToRegex(
    const blink::SafeUrlPattern& url_pattern) {
  if (url_pattern.hostname.empty()) {
    return absl::nullopt;
  }
  liburlpattern::Options options = {.delimiter_list = ".",
                                    .prefix_list = "",
                                    .sensitive = false,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.hostname, options, "[^\\.]+?");
  std::string regex_string = pattern.GenerateRegexString();
  VLOG(3) << "host regex string: " << regex_string;
  return regex_string;
}

std::string PathnameConvertToPattern(const blink::SafeUrlPattern& url_pattern) {
  if (url_pattern.pathname.empty()) {
    return std::string();
  }
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  return pattern.GeneratePatternString();
}

std::string HostnameConvertToPattern(const blink::SafeUrlPattern& url_pattern) {
  if (url_pattern.hostname.empty()) {
    return std::string();
  }
  liburlpattern::Options options = {.delimiter_list = ".",
                                    .prefix_list = "",
                                    .sensitive = false,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.hostname, options, "[^\\.]+?");
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

std::string RunningStatusToString(
    const blink::ServiceWorkerRouterRunningStatusCondition& running_status) {
  switch (running_status.status) {
    case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
        kRunning:
      return "running";
    case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
        kNotRunning:
      return "not-running";
  }
}

bool IsValidCondition(const blink::ServiceWorkerRouterCondition& condition) {
  switch (condition.type) {
    case blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern:
      return condition.url_pattern.has_value() &&
             !(condition.url_pattern->hostname.empty() &&
               condition.url_pattern->pathname.empty());
    case blink::ServiceWorkerRouterCondition::ConditionType::kRequest:
      return condition.request.has_value() &&
             (condition.request->method.has_value() ||
              condition.request->mode.has_value() ||
              condition.request->destination.has_value());
    case blink::ServiceWorkerRouterCondition::ConditionType::kRunningStatus:
      return condition.running_status.has_value();
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

bool IsMatchedRunningCondition(
    const blink::ServiceWorkerRouterRunningStatusCondition& condition,
    const blink::EmbeddedWorkerStatus& running_status) {
  // returns true if both condition matches.
  bool is_condition_running = condition.status ==
                              blink::ServiceWorkerRouterRunningStatusCondition::
                                  RunningStatusEnum::kRunning;
  bool is_status_running =
      running_status == blink::EmbeddedWorkerStatus::RUNNING;
  return is_condition_running == is_status_running;
}

}  // namespace

namespace content {

class ServiceWorkerRouterEvaluator::RouterRule {
 public:
  RouterRule()
      : pathname_patterns_(RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED)),
        hostname_patterns_(RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED)) {}
  ~RouterRule() = default;
  bool SetRule(const blink::ServiceWorkerRouterRule& rule);
  bool IsConditionMatched(
      const network::ResourceRequest& request,
      absl::optional<blink::EmbeddedWorkerStatus> running_status) const;
  const std::vector<blink::ServiceWorkerRouterSource>& sources() const {
    return sources_;
  }
  bool need_running_status() const { return need_running_status_; }

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
      const network::ResourceRequest& request,
      absl::optional<blink::EmbeddedWorkerStatus> running_status) const;
  // To process SafeUrlPattern faster, all patterns are combined into the
  // `RE::Set` and compiled when `ServiceWorkerRouterEvaluator` is initialized.
  RE2::Set pathname_patterns_;
  size_t pathname_pattern_length_ = 0;
  RE2::Set hostname_patterns_;
  size_t hostname_pattern_length_ = 0;
  // Non-SafeUrlPattern conditions are processed one by one.
  std::vector<blink::ServiceWorkerRouterCondition> non_url_pattern_conditions_;
  std::vector<blink::ServiceWorkerRouterSource> sources_;
  bool need_running_status_ = false;
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
      if (condition.type ==
          blink::ServiceWorkerRouterCondition::ConditionType::kRunningStatus) {
        need_running_status_ = true;
      }
      continue;
    }

    // Code for SafeUrlPattern conditions.
    auto pathregex = PathnameConvertToRegex(*condition.url_pattern);
    if (pathregex) {
      if (pathname_patterns_.Add(*pathregex, nullptr) == -1) {
        // Failed to parse the regex.
        RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kParseError);
        return false;
      }
      // Counts the conditions to ensure all conditions are matched.
      ++pathname_pattern_length_;
    }
    auto hostregex = HostnameConvertToRegex(*condition.url_pattern);
    if (hostregex) {
      if (hostname_patterns_.Add(*hostregex, nullptr) == -1) {
        // Failed to parse the regex.
        RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kParseError);
        return false;
      }
      // Counts the conditions to ensure all conditions are matched.
      ++hostname_pattern_length_;
    }
  }
  if (pathname_pattern_length_ > 0 && !pathname_patterns_.Compile()) {
    // Failed to compile the regex.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kCompileError);
    return false;
  }
  if (hostname_pattern_length_ > 0 && !hostname_patterns_.Compile()) {
    // Failed to compile the regex.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kCompileError);
    return false;
  }
  return true;
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsConditionMatched(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
  return IsUrlPatternConditionMatched(request) &&
         IsNonUrlPatternConditionMatched(request, running_status);
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsUrlPatternConditionMatched(
    const network::ResourceRequest& request) const {
  if (hostname_pattern_length_ > 0) {
    std::vector<int> vec;
    if (!hostname_patterns_.Match(request.url.host(), &vec)) {
      return false;
    }
    // ensure it matches all included patterns.
    if (vec.size() != hostname_pattern_length_) {
      return false;
    }
  }
  if (pathname_pattern_length_ > 0) {
    std::vector<int> vec;
    if (!pathname_patterns_.Match(request.url.path(), &vec)) {
      return false;
    }
    // ensure it matches all included patterns.
    if (vec.size() != pathname_pattern_length_) {
      return false;
    }
  }
  return true;
}

bool ServiceWorkerRouterEvaluator::RouterRule::IsNonUrlPatternConditionMatched(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
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
      case blink::ServiceWorkerRouterCondition::ConditionType::kRunningStatus:
        if (running_status &&
            !IsMatchedRunningCondition(*c.running_status, *running_status)) {
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
    need_running_status_ |= rule->need_running_status();
    compiled_rules_.emplace_back(std::move(rule));
  }
  RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kNoError);
  is_valid_ = true;
}

std::vector<blink::ServiceWorkerRouterSource>
ServiceWorkerRouterEvaluator::EvaluateInternal(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
  CHECK(is_valid_);
  for (const auto& rule : compiled_rules_) {
    if (rule->IsConditionMatched(request, running_status)) {
      VLOG(3) << "matched request url=" << request.url;
      RecordMatchedSourceType(rule->sources());
      return rule->sources();
    }
  }
  VLOG(3) << "not matched request url=" << request.url;
  return std::vector<blink::ServiceWorkerRouterSource>();
}

std::vector<blink::ServiceWorkerRouterSource>
ServiceWorkerRouterEvaluator::Evaluate(
    const network::ResourceRequest& request,
    blink::EmbeddedWorkerStatus running_status) const {
  return EvaluateInternal(request, running_status);
}

std::vector<blink::ServiceWorkerRouterSource>
ServiceWorkerRouterEvaluator::EvaluateWithoutRunningStatus(
    const network::ResourceRequest& request) const {
  CHECK(!need_running_status_);
  return EvaluateInternal(request, absl::nullopt);
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
          base::Value out_value;
          std::string host = HostnameConvertToPattern(*c.url_pattern);
          std::string path = PathnameConvertToPattern(*c.url_pattern);
          if (!host.empty()) {
            base::Value::Dict host_path;
            host_path.Set("host", host);
            host_path.Set("path", path);
            out_value = base::Value(std::move(host_path));
          } else {
            out_value = base::Value(path);
          }
          out_c.Set("urlPattern", std::move(out_value));
          condition.Append(std::move(out_c));
          break;
        }
        case blink::ServiceWorkerRouterCondition::ConditionType::kRequest: {
          base::Value::Dict out_c;
          out_c.Set("request", RequestToValue(*c.request));
          condition.Append(std::move(out_c));
          break;
        }
        case blink::ServiceWorkerRouterCondition::ConditionType::
            kRunningStatus: {
          base::Value::Dict out_c;
          out_c.Set("running_status", RunningStatusToString(*c.running_status));
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
