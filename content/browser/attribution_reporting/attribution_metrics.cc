// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace content {

void RecordRegisterConversionAllowed(bool allowed) {
  base::UmaHistogramBoolean("Conversions.RegisterConversionAllowed", allowed);
}

void RecordRegisterImpressionAllowed(bool allowed) {
  base::UmaHistogramBoolean("Conversions.RegisterImpressionAllowed", allowed);
}

}  // namespace content
