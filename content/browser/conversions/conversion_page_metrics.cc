// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_page_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace content {

ConversionPageMetrics::ConversionPageMetrics() = default;

ConversionPageMetrics::~ConversionPageMetrics() {
  // TODO(https://crbug.com/1044099): Consider limiting registrations per page
  // based on this metric.
  base::UmaHistogramExactLinear("Conversions.RegisteredConversionsPerPage",
                                num_conversions_on_current_page_, 100);

  base::UmaHistogramExactLinear("Conversions.RegisteredImpressionsPerPage",
                                num_impressions_on_current_page_, 100);
}

void ConversionPageMetrics::OnConversion(const StorableConversion& conversion) {
  num_conversions_on_current_page_++;
}

void ConversionPageMetrics::OnImpression() {
  num_impressions_on_current_page_++;
}

}  // namespace content
