// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_statistics.h"

#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

void SelectorStatisticsCollector::ReserveCapacity(wtf_size_t size) {
  per_rule_statistics_.reserve(size);
}

void SelectorStatisticsCollector::BeginCollectionForRule(const RuleData* rule) {
  rule_ = rule;
  fast_reject_ = false;
  did_match_ = false;
  start_ = base::TimeTicks::Now();
}

void SelectorStatisticsCollector::EndCollectionForCurrentRule() {
  if (rule_) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_;
    per_rule_statistics_.emplace_back(rule_, fast_reject_, did_match_, elapsed);
  }
  rule_ = nullptr;
}

}  // namespace blink
