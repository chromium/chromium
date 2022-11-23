// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

void CountSpeculationRulesLoadOutcome(SpeculationRulesLoadOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Blink.SpeculationRules.LoadOutcome", outcome);
}

}  // namespace blink
