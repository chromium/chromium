// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

#include "third_party/blink/common/privacy_budget/aggregating_sample_collector.h"
#include "third_party/blink/common/privacy_budget/identifiability_sample_collector_test_utils.h"

namespace blink {

namespace {
// Only used for testing. Not thread safe.
IdentifiabilitySampleCollector* testing_overriding_collector = nullptr;
}  // namespace

// static
IdentifiabilitySampleCollector* IdentifiabilitySampleCollector::Get() {
  auto* overridden = testing_overriding_collector;
  if (overridden)
    return overridden;
  return internal::GetCollectorInstance();
}

IdentifiabilitySampleCollector::~IdentifiabilitySampleCollector() = default;

void SetCollectorInstanceForTesting(
    IdentifiabilitySampleCollector* new_collector) {
  testing_overriding_collector = new_collector;
}

void ResetCollectorInstanceStateForTesting() {
  internal::GetCollectorInstance()->ResetForTesting();
}

}  // namespace blink
