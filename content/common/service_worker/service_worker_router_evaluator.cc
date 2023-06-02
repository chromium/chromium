// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_router_evaluator.h"

#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

std::string ConvertToRegex(const blink::UrlPattern& url_pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(url_pattern.pathname, options, "[^/]+?");
  VLOG(3) << "regex string:" << pattern.GenerateRegexString();
  return pattern.GenerateRegexString();
}

}  // namespace

namespace content {

ServiceWorkerRouterEvaluator::RouterRule::RouterRule()
    : url_patterns(RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED)) {}

ServiceWorkerRouterEvaluator::RouterRule::~RouterRule() = default;

ServiceWorkerRouterEvaluator::ServiceWorkerRouterEvaluator(
    blink::ServiceWorkerRouterRules rules)
    : rules_(std::move(rules)) {
  Compile();
}
ServiceWorkerRouterEvaluator::~ServiceWorkerRouterEvaluator() = default;

void ServiceWorkerRouterEvaluator::Compile() {
  for (const auto& r : rules_.rules) {
    std::unique_ptr<RouterRule> rule = absl::make_unique<RouterRule>();
    for (const auto& condition : r.conditions) {
      CHECK_EQ(condition.type,
               blink::ServiceWorkerRouterCondition::ConditionType::kUrlPattern);
      if (rule->url_patterns.Add(ConvertToRegex(*condition.url_pattern),
                                 nullptr) == -1) {
        // Failed to parse the regex.
        return;
      }
    }
    rule->url_pattern_length = r.conditions.size();
    if (!rule->url_patterns.Compile()) {
      // Failed to compile the regex.
      return;
    }
    rule->sources = r.sources;
    compiled_rules_.emplace_back(std::move(rule));
  }
  is_valid_ = true;
}

std::vector<blink::ServiceWorkerRouterSource>
ServiceWorkerRouterEvaluator::Evaluate(
    const network::ResourceRequest& request) const {
  for (const auto& rule : compiled_rules_) {
    std::vector<int> vec;
    if (rule->url_patterns.Match(request.url.path(), &vec) &&
        // ensure it matches all included patterns.
        vec.size() == rule->url_pattern_length) {
      return rule->sources;
    }
  }
  return std::vector<blink::ServiceWorkerRouterSource>();
}

}  // namespace content
