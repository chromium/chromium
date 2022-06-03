// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_
#define FUCHSIA_BASE_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <vector>

#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"

namespace cr_fuchsia {

std::vector<fuchsia::legacymetrics::Histogram> GetLegacyMetricsDeltas();

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_
