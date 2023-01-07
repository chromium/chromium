// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEFAULT_RANDOM_GENERATOR_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEFAULT_RANDOM_GENERATOR_H_

#include "content/browser/attribution_reporting/attribution_random_generator.h"
#include "content/common/content_export.h"

namespace content {

// Uses the secure top-level functions of the same name in `base/rand_util.h`.
class CONTENT_EXPORT AttributionDefaultRandomGenerator
    : public AttributionRandomGenerator {
 public:
  AttributionDefaultRandomGenerator() = default;

  ~AttributionDefaultRandomGenerator() override = default;

  AttributionDefaultRandomGenerator(const AttributionDefaultRandomGenerator&) =
      delete;
  AttributionDefaultRandomGenerator(AttributionDefaultRandomGenerator&&) =
      delete;

  AttributionDefaultRandomGenerator& operator=(
      const AttributionDefaultRandomGenerator&) = delete;
  AttributionDefaultRandomGenerator& operator=(
      AttributionDefaultRandomGenerator&&) = delete;

  // AttributionRandomGenerator:
  double RandDouble() override;
  int RandInt(int min, int max) override;
  void RandomShuffle(std::vector<AttributionReport>& reports) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEFAULT_RANDOM_GENERATOR_H_
