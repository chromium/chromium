// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOADING_H_
#define CONTENT_PUBLIC_BROWSER_PRELOADING_H_

#include <stdint.h>

namespace content {

// Defines the different types of preloading speedup techniques. Preloading is a
// holistic term to define all the speculative operations the browser does for
// loading content before a page navigates to make navigation faster.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingType {
  // No PreloadingType is present. This may include other preloading operations
  // which will be added later to PreloadingType as we expand.
  kUnspecified = 0,

  // TODO(crbug.com/1309934): Add more preloading types from 1 - 3 as we
  // integrate Preloading logging with various preloading types.

  // This speedup technique comes with the most impact and overhead. We preload
  // and render a page before the user navigates to it. This will make the next
  // page navigation nearly instant as we would activate a fully prepared
  // RenderFrameHost. Both resources are fetched and JS is executed.
  kPrerender = 4,
};

// Defines various triggering mechanisms which triggers different preloading
// operations mentioned in preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingPredictor {
  // No PreloadingTrigger is present. This may include the small percentage of
  // usages of browser triggers, link-rel, OptimizationGuideService e.t.c which
  // will be added later as a separate elements.
  kUnspecified = 0,

  // TODO(crbug.com/1309934): Add more predictors as we integrate Preloading
  // logging.

  // > 100 values are reserved for embedder-specific values, such as the
  // ChromePreloadingPredictor enum.
};

// This constant is used to define the value from which embedders can add more
// enums beyond this value. We mask it by 100 to avoid usage of the same numbers
// for logging.
static constexpr int64_t kPreloadingPredictorContentEnd = 100;

// Defines if a preloading operation is eligible for a given preloading
// trigger.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingEligibility {
  // Preloading operation is not defined for a particular preloading trigger
  // prediction.
  kUnspecified = 0,

  // Preloading operation is eligible and is triggered for a preloading
  // predictor.
  kEligible = 1,

  // TODO(crbug.com/1309934): Add more specific ineligibility reasons subject to
  // each preloading operation.
};

// Defines the post-triggering outcome once the preloading operation is
// triggered.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingTriggeringOutcome {
  // No TriggeringOutcome is present.
  kUnspecified = 0,

  // Status is NotTriggered for attempts that were not triggered due to various
  // ineligibility reasons.
  kNotTriggered = 1,

  // For attempts that we wanted to trigger, but for which we already had an
  // equivalent attempt (same preloading operation and same URL/target) in
  // progress.
  kDuplicate = 2,

  // Preloading was triggered and did not fail, but did not complete in time
  // before the user navigated away (or the browser was shut down).
  kRunning = 3,

  // Preloading triggered and is ready to be used for the next navigation. This
  // doesn't mean preloading attempt was actually used.
  kReady = 4,

  // Preloading was triggered, completed successfully and was used for the next
  // navigation.
  kSuccess = 5,

  // Preloading was triggered but encountered an error and failed.
  kFailure = 6,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOADING_H_
