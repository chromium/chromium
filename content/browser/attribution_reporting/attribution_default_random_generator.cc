// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_default_random_generator.h"

#include "base/rand_util.h"
#include "content/browser/attribution_reporting/attribution_report.h"

namespace content {

double AttributionDefaultRandomGenerator::RandDouble() {
  return base::RandDouble();
}

int AttributionDefaultRandomGenerator::RandInt(int min, int max) {
  return base::RandInt(min, max);
}

void AttributionDefaultRandomGenerator::RandomShuffle(
    std::vector<AttributionReport>& reports) {
  base::RandomShuffle(reports.begin(), reports.end());
}

}  // namespace content
