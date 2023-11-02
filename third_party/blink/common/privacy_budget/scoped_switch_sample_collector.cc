// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/scoped_switch_sample_collector.h"

#include "third_party/blink/common/privacy_budget/aggregating_sample_collector.h"
#include "third_party/blink/common/privacy_budget/identifiability_sample_collector_test_utils.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

namespace blink {
namespace test {

ScopedSwitchSampleCollector::ScopedSwitchSampleCollector(
    IdentifiabilitySampleCollector* new_aggregator) {
  SetCollectorInstanceForTesting(new_aggregator);
}

ScopedSwitchSampleCollector::~ScopedSwitchSampleCollector() {
  // No need to restore original collector since
  // `SetCollectorInstanceForTesting` doesn't allow nested scopes.
  SetCollectorInstanceForTesting(nullptr);
}

}  // namespace test
}  // namespace blink
