// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include <memory>
#include <tuple>

#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/utils.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

class BaseCondition;
class OrCondition;
class ConditionObject;

base::Value ConditionToValue(
    const blink::ServiceWorkerRouterCondition& condition);

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
  kExceedMaxConditionDepth = 8,
  kMaxValue = kExceedMaxConditionDepth,
};

void RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums e) {
  base::UmaHistogramEnumeration("ServiceWorker.RouterEvaluator.Error", e);
}

void RecordMatchedSourceType(
    const std::vector<blink::ServiceWorkerRouterSource>& sources) {
  base::UmaHistogramEnumeration(
      "ServiceWorker.RouterEvaluator.MatchedFirstSourceType", sources[0].type);
}

// TODO(crbug.com/1371756): consolidate code with blink::url_pattern.
//
// The type and method come form
// third_party/blink/renderer/core/url_pattern/url_pattern_component.{h,cc}.
// GetOptions is not exported yet, and there is little benefit to depend
// on the blink::URLPattern now.
enum class URLPatternFieldType {
  kProtocol,
  kUsername,
  kPassword,
  kHostname,
  kPort,
  kPathname,
  kSearch,
  kHash,
};

// Utility method to get the correct liburlpattern parse options for a given
// type.
const std::tuple<liburlpattern::Options, std::string>
GetOptionsAndSegmentWildcardRegex(const blink::SafeUrlPattern& url_pattern,
                                  URLPatternFieldType type) {
  using liburlpattern::Options;

  Options options = {.delimiter_list = "",
                     .prefix_list = "",
                     .sensitive = !url_pattern.options.ignore_case,
                     .strict = true};

  if (type == URLPatternFieldType::kHostname) {
    options.delimiter_list = ".";
  } else if (type == URLPatternFieldType::kPathname) {
    // TODO(crbug.com/1371756): follows the original GetOptions behavior.
    // It sets the following delimiters for some limited protocols.
    options.delimiter_list = "/";
    options.prefix_list = "/";
  }
  std::string segment_wildcard_regex = base::StringPrintf(
      "[^%s]+?",
      liburlpattern::EscapeRegexpString(options.delimiter_list).c_str());

  return std::tie(options, segment_wildcard_regex);
}

liburlpattern::Pattern ConvertToPattern(
    const blink::SafeUrlPattern& url_pattern,
    URLPatternFieldType type) {
  std::vector<liburlpattern::Part> parts;
  switch (type) {
    case URLPatternFieldType::kProtocol:
      parts = url_pattern.protocol;
      break;
    case URLPatternFieldType::kUsername:
      parts = url_pattern.username;
      break;
    case URLPatternFieldType::kPassword:
      parts = url_pattern.password;
      break;
    case URLPatternFieldType::kHostname:
      parts = url_pattern.hostname;
      break;
    case URLPatternFieldType::kPort:
      parts = url_pattern.port;
      break;
    case URLPatternFieldType::kPathname:
      parts = url_pattern.pathname;
      break;
    case URLPatternFieldType::kSearch:
      parts = url_pattern.search;
      break;
    case URLPatternFieldType::kHash:
      parts = url_pattern.hash;
      break;
  }
  auto [options, swr] = GetOptionsAndSegmentWildcardRegex(url_pattern, type);
  return liburlpattern::Pattern(parts, options, swr);
}

std::string ConvertToRegex(const blink::SafeUrlPattern& url_pattern,
                           URLPatternFieldType type) {
  auto pattern = ConvertToPattern(url_pattern, type);
  std::string regex_string = pattern.GenerateRegexString();
  VLOG(3) << "regex string: " << regex_string;
  return regex_string;
}

std::string ConvertToPatternString(const blink::SafeUrlPattern& url_pattern,
                                   URLPatternFieldType type) {
  auto pattern = ConvertToPattern(url_pattern, type);
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

base::Value OrConditionToValue(
    const blink::ServiceWorkerRouterOrCondition& or_condition) {
  base::Value::List ret;
  ret.reserve(or_condition.conditions.size());
  for (const auto& c : or_condition.conditions) {
    ret.Append(ConditionToValue(c));
  }
  return base::Value(std::move(ret));
}

base::Value ConditionToValue(
    const blink::ServiceWorkerRouterCondition& condition) {
  base::Value::Dict out_c;
  const auto& [url_pattern, request, running_status, or_condition] =
      condition.get();
  if (url_pattern) {
    base::Value::Dict url_pattern_value;
#define TO_VALUE(type, type_name)                            \
  do {                                                       \
    auto value = ConvertToPatternString(*url_pattern, type); \
    url_pattern_value.Set(type_name, value);                 \
  } while (0)

    TO_VALUE(URLPatternFieldType::kProtocol, "protocol");
    TO_VALUE(URLPatternFieldType::kUsername, "username");
    TO_VALUE(URLPatternFieldType::kPassword, "password");
    TO_VALUE(URLPatternFieldType::kHostname, "hostname");
    TO_VALUE(URLPatternFieldType::kPort, "port");
    TO_VALUE(URLPatternFieldType::kPathname, "pathname");
    TO_VALUE(URLPatternFieldType::kSearch, "search");
    TO_VALUE(URLPatternFieldType::kHash, "hash");
#undef TO_VALUE
      out_c.Set("urlPattern", std::move(url_pattern_value));
  }
  if (request) {
    out_c.Set("request", RequestToValue(*request));
  }
  if (running_status) {
    out_c.Set("running_status", RunningStatusToString(*running_status));
  }
  if (or_condition) {
    out_c.Set("or", OrConditionToValue(*or_condition));
  }
  return base::Value(std::move(out_c));
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
      case blink::ServiceWorkerRouterSource::Type::kNetwork:
        if (!s.network_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
      case blink::ServiceWorkerRouterSource::Type::kRace:
        if (!s.race_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
      case blink::ServiceWorkerRouterSource::Type::kFetchEvent:
        if (!s.fetch_event_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
      case blink::ServiceWorkerRouterSource::Type::kCache:
        if (!s.cache_source) {
          RecordSetupError(
              ServiceWorkerRouterEvaluatorErrorEnums::kInvalidSource);
          return false;
        }
        break;
    }
  }
  return true;
}

[[nodiscard]] bool ExceedsMaxConditionDepth(
    const blink::ServiceWorkerRouterCondition& condition,
    int depth = 0) {
  if (depth >= blink::kServiceWorkerRouterConditionMaxRecursionDepth) {
    return true;
  }
  const auto& or_condition =
      std::get<const std::optional<blink::ServiceWorkerRouterOrCondition>&>(
          condition.get());
  if (!or_condition) {
    return false;
  }
  for (const auto& c : or_condition->conditions) {
    if (ExceedsMaxConditionDepth(c, depth + 1)) {
      return true;
    }
  }
  return false;
}

bool MatchRequestCondition(
    const blink::ServiceWorkerRouterRequestCondition& pattern,
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

bool MatchRunningCondition(
    const blink::ServiceWorkerRouterRunningStatusCondition& condition,
    const blink::EmbeddedWorkerStatus& running_status) {
  // returns true if both condition matches.
  bool is_condition_running = condition.status ==
                              blink::ServiceWorkerRouterRunningStatusCondition::
                                  RunningStatusEnum::kRunning;
  bool is_status_running =
      running_status == blink::EmbeddedWorkerStatus::kRunning;
  return is_condition_running == is_status_running;
}

class BaseCondition {
 public:
  BaseCondition() = default;
  // Not copyable
  BaseCondition(const BaseCondition&) = delete;
  // Movable
  BaseCondition(BaseCondition&&) = default;
  BaseCondition& operator=(BaseCondition&&) = default;
  // Returns true on success. Otherwise, false.
  bool Set(const blink::ServiceWorkerRouterCondition& condition);
  bool Match(const network::ResourceRequest& request,
             absl::optional<blink::EmbeddedWorkerStatus> running_status) const;
  bool need_running_status() const { return need_running_status_; }

 private:
  bool MatchUrlPatternConditions(const network::ResourceRequest& request) const;
  bool MatchNonUrlPatternConditions(
      const network::ResourceRequest& request,
      absl::optional<blink::EmbeddedWorkerStatus> running_status) const;

  std::unique_ptr<RE2> protocol_pattern_;
  std::unique_ptr<RE2> username_pattern_;
  std::unique_ptr<RE2> password_pattern_;
  std::unique_ptr<RE2> hostname_pattern_;
  std::unique_ptr<RE2> port_pattern_;
  std::unique_ptr<RE2> pathname_pattern_;
  std::unique_ptr<RE2> search_pattern_;
  std::unique_ptr<RE2> hash_pattern_;
  bool has_url_pattern_ = false;
  // Non-SafeUrlPattern conditions are processed one by one.
  blink::ServiceWorkerRouterCondition non_url_pattern_condition_;
  bool need_running_status_ = false;
};

bool BaseCondition::Set(const blink::ServiceWorkerRouterCondition& condition) {
  if (condition.IsEmpty()) {
    // At least one condition must be set.
    RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kEmptyCondition);
    return false;
  }
  const auto& [url_pattern, request, running_status, or_condition] =
      condition.get();

  CHECK(!or_condition);

  non_url_pattern_condition_ = {absl::nullopt, request, running_status,
                                absl::nullopt};
  if (running_status) {
    need_running_status_ = true;
  }
  if (url_pattern) {
#define SET_PATTERN(type_name, type)                                         \
  do {                                                                       \
    auto regex = ConvertToRegex(*url_pattern, type);                         \
    type_name##_pattern_ = std::make_unique<RE2>(regex, RE2::Options());     \
    if (!type_name##_pattern_->ok()) {                                       \
      RecordSetupError(ServiceWorkerRouterEvaluatorErrorEnums::kParseError); \
      return false;                                                          \
    }                                                                        \
  } while (0)
    SET_PATTERN(protocol, URLPatternFieldType::kProtocol);
    SET_PATTERN(username, URLPatternFieldType::kUsername);
    SET_PATTERN(password, URLPatternFieldType::kPassword);
    SET_PATTERN(hostname, URLPatternFieldType::kHostname);
    SET_PATTERN(port, URLPatternFieldType::kPort);
    SET_PATTERN(pathname, URLPatternFieldType::kPathname);
    SET_PATTERN(search, URLPatternFieldType::kSearch);
    SET_PATTERN(hash, URLPatternFieldType::kHash);
#undef SET_PATTERN
    has_url_pattern_ = true;
    // TODO(crbug.com/1371756): consider fast path on empty parts and "*".
    // Currently, regular expressions are executed even for empty parts cases,
    // which try to match inputs with "^$".  It is also executed for "*".
    // If performance to evaluate regular expressions matter, fast path can
    // be needed.
  }
  return true;
}

bool BaseCondition::Match(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
  return MatchUrlPatternConditions(request) &&
         MatchNonUrlPatternConditions(request, running_status);
}

bool BaseCondition::MatchUrlPatternConditions(
    const network::ResourceRequest& request) const {
  if (!has_url_pattern_) {  // nothing need to be matched.
    return true;
  }
#define PATTERN_MATCH(type_name, field)                                  \
  CHECK(type_name##_pattern_);                                           \
  if (!RE2::FullMatch(request.url.field(), *type_name##_pattern_)) {     \
    VLOG(3) << "not matched. url=" << request.url << " field=" << #field \
            << " value=" << request.url.field();                         \
    return false;                                                        \
  }
  PATTERN_MATCH(protocol, scheme);
  PATTERN_MATCH(username, username);
  PATTERN_MATCH(password, password);
  PATTERN_MATCH(hostname, host);
  PATTERN_MATCH(port, port);
  PATTERN_MATCH(pathname, path);
  PATTERN_MATCH(search, query);
  PATTERN_MATCH(hash, ref);
#undef PATTERN_MATCH
  return true;
}

bool BaseCondition::MatchNonUrlPatternConditions(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
  const auto& [url_pattern, request_pattern, running_status_pattern,
               or_condition] = non_url_pattern_condition_.get();
  CHECK(!url_pattern);
  CHECK(!or_condition);
  if (request_pattern && !MatchRequestCondition(*request_pattern, request)) {
    return false;
  }
  if (running_status && running_status_pattern &&
      !MatchRunningCondition(*running_status_pattern, *running_status)) {
    return false;
  }
  return true;
}

class OrCondition {
 public:
  OrCondition() = default;
  // Not copyable
  OrCondition(const OrCondition&) = delete;
  // Movable
  OrCondition(OrCondition&&) = default;
  OrCondition& operator=(OrCondition&&) = default;
  // Returns true on success. Otherwise, false.
  bool Set(const std::vector<blink::ServiceWorkerRouterCondition>& conditions);
  bool Match(const network::ResourceRequest& request,
             absl::optional<blink::EmbeddedWorkerStatus> running_status) const;
  bool need_running_status() const { return need_running_status_; }

 private:
  bool MatchUrlPatternConditions(const network::ResourceRequest& request) const;
  bool MatchNonUrlPatternConditions(
      const network::ResourceRequest& request,
      absl::optional<blink::EmbeddedWorkerStatus> running_status) const;

  std::vector<ConditionObject> conditions_;
  bool need_running_status_ = false;
};

class ConditionObject {
 public:
  // Returns true on success. Otherwise, false.
  bool Set(const blink::ServiceWorkerRouterCondition& condition) {
    if (!condition.IsValid()) {
      RecordSetupError(
          ServiceWorkerRouterEvaluatorErrorEnums::kInvalidCondition);
      return false;
    }
    const auto& or_condition =
        std::get<const absl::optional<blink::ServiceWorkerRouterOrCondition>&>(
            condition.get());
    if (or_condition) {
      OrCondition v;
      bool success = v.Set(or_condition->conditions);
      value_ = std::move(v);
      return success;
    } else {
      BaseCondition v;
      bool success = v.Set(condition);
      value_ = std::move(v);
      return success;
    }
  }
  bool Match(const network::ResourceRequest& request,
             absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
    return absl::visit(
        [&request, running_status](const auto& condition) {
          return condition.Match(request, running_status);
        },
        value_);
  }
  bool need_running_status() const {
    return absl::visit(
        [](const auto& condition) { return condition.need_running_status(); },
        value_);
  }

 private:
  absl::variant<BaseCondition, OrCondition> value_;
};

bool OrCondition::Set(
    const std::vector<blink::ServiceWorkerRouterCondition>& conditions) {
  conditions_.reserve(conditions.size());
  for (const auto& c : conditions) {
    conditions_.emplace_back();

    ConditionObject& ob = conditions_.back();
    if (!ob.Set(c)) {
      conditions_.clear();
      return false;
    }
    need_running_status_ = need_running_status_ || ob.need_running_status();
  }
  return true;
}

bool OrCondition::Match(
    const network::ResourceRequest& request,
    absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
  for (const auto& c : conditions_) {
    if (c.Match(request, running_status)) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace content {

class ServiceWorkerRouterEvaluator::RouterRule {
 public:
  bool SetRule(const blink::ServiceWorkerRouterRule& rule) {
    if (ExceedsMaxConditionDepth(rule.condition)) {
      // Too many recursion in the condition.
      RecordSetupError(
          ServiceWorkerRouterEvaluatorErrorEnums::kExceedMaxConditionDepth);
      return false;
    }
    return condition_.Set(rule.condition) && SetSources(rule.sources);
  }
  bool Match(const network::ResourceRequest& request,
             absl::optional<blink::EmbeddedWorkerStatus> running_status) const {
    return condition_.Match(request, running_status);
  }
  const std::vector<blink::ServiceWorkerRouterSource>& sources() const {
    return sources_;
  }
  bool need_running_status() const { return condition_.need_running_status(); }

 private:
  // Returns true on success. Otherwise, false.
  bool SetSources(
      const std::vector<blink::ServiceWorkerRouterSource>& sources) {
    if (!IsValidSources(sources)) {
      return false;
    }
    sources_ = sources;
    return true;
  }

  ConditionObject condition_;
  std::vector<blink::ServiceWorkerRouterSource> sources_;
};

ServiceWorkerRouterEvaluator::ServiceWorkerRouterEvaluator(
    blink::ServiceWorkerRouterRules rules)
    : rules_(std::move(rules)) {
  Compile();
}
ServiceWorkerRouterEvaluator::~ServiceWorkerRouterEvaluator() = default;

void ServiceWorkerRouterEvaluator::Compile() {
  for (const auto& r : rules_.rules) {
    std::unique_ptr<RouterRule> rule = std::make_unique<RouterRule>();
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
    if (rule->Match(request, running_status)) {
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
    base::Value condition = ConditionToValue(r.condition);
    base::Value::List source;
    for (const auto& s : r.sources) {
      switch (s.type) {
        case blink::ServiceWorkerRouterSource::Type::kNetwork:
          source.Append("network");
          break;
        case blink::ServiceWorkerRouterSource::Type::kRace:
          // TODO(crbug.com/1371756): we may need to update the name per target.
          source.Append("race-network-and-fetch-handler");
          break;
        case blink::ServiceWorkerRouterSource::Type::kFetchEvent:
          source.Append("fetch-event");
          break;
        case blink::ServiceWorkerRouterSource::Type::kCache:
          if (s.cache_source->cache_name) {
            base::Value::Dict out_s;
            out_s.Set("cache_name", *s.cache_source->cache_name);
            source.Append(std::move(out_s));
          } else {
            source.Append("cache");
          }
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
