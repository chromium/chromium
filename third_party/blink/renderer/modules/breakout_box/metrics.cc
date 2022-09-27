// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "base/metrics/histogram_functions.h"

namespace blink {

void RecordBreakoutBoxUsage(BreakoutBoxUsage usage) {
  base::UmaHistogramEnumeration("Media.BreakoutBox.Usage", usage);
}

}  // namespace blink
