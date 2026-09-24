// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_DEVTOOLS_PRERENDER_ATTEMPT_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_DEVTOOLS_PRERENDER_ATTEMPT_H_

#include <optional>

#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"

namespace content {

// Represents information shared to DevTools clients by CDP event
// Preload.prerenderStatusUpdated. Counterpart of the frontend is
// SDK.PreloadingModel.PrerenderAttemptInternal.
//
// All status updates will be shared as soon as they are made.
class CONTENT_EXPORT DevToolsPrerenderAttempt {
 public:
  DevToolsPrerenderAttempt() = default;
  ~DevToolsPrerenderAttempt() = default;

  DevToolsPrerenderAttempt(const DevToolsPrerenderAttempt&) = delete;
  DevToolsPrerenderAttempt& operator=(const DevToolsPrerenderAttempt&) = delete;
  DevToolsPrerenderAttempt(DevToolsPrerenderAttempt&&) = delete;
  DevToolsPrerenderAttempt& operator=(DevToolsPrerenderAttempt&&) = delete;

  void SetTriggeringOutcome(const PrerenderAttributes& attributes,
                            PreloadingTriggeringOutcome outcome);
  void SetFailureReason(const PrerenderAttributes& attributes,
                        PrerenderFinalStatus prerender_status);
  void SetFailureReason(const PrerenderAttributes& attributes,
                        const PrerenderCancellationReason& reasons);
  void SetEffectiveAction(const PrerenderAttributes& attributes,
                          blink::mojom::SpeculationAction effective_action);

 private:
  // The latest non-failure outcome, re-emitted when the effective action
  // changes without ending the attempt.
  std::optional<PreloadingTriggeringOutcome> last_outcome_;
  bool failed_ = false;
  // Set when the browser commits to an in-place upgrade. The renderer resume
  // message is fire-and-forget, and the original action remains immutable.
  std::optional<blink::mojom::SpeculationAction> effective_action_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_DEVTOOLS_PRERENDER_ATTEMPT_H_
