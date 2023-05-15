// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESPONSIVENESS_CALCULATOR_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

// Allows the embedder to be notified when the responsiveness metric
// (Browser.MainThreadsCongestion) is emitted.
class CONTENT_EXPORT ResponsivenessCalculatorDelegate {
 public:
  // Invoked whenever a measurement interval ended, which includes when the
  // application is suspended and the current interval should be discarded.
  virtual void OnMeasurementIntervalEnded() = 0;
  // Invoked when a sample is emitted to the responsiveness histogram.
  virtual void OnResponsivenessEmitted(int num_congested_slices,
                                       int min,
                                       int exclusive_max,
                                       size_t buckets) = 0;

  virtual ~ResponsivenessCalculatorDelegate() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESPONSIVENESS_CALCULATOR_DELEGATE_H_
