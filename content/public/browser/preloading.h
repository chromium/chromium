// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOADING_H_
#define CONTENT_PUBLIC_BROWSER_PRELOADING_H_

#include <cstdint>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {

// If you change any of the following enums or static variables, please follow
// the process in go/preloading-dashboard-updates to update the mapping
// reflected in dashboard, or if you are not a Googler, please file an FYI bug
// on https://crbug.new with component Internals>Preload.

// Defines the different types of preloading speedup techniques. Preloading is a
// holistic term to define all the speculative operations the browser does for
// loading content before a page navigates to make navigation faster.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingType {
  // No PreloadingType is present. This may include other preloading operations
  // which will be added later to PreloadingType as we expand.
  kUnspecified = 0,

  // TODO(crbug.com/1309934): Add preloading type 1 as we integrate
  // Preloading logging with preresolve.

  // Establishes a connection (including potential TLS handshake) with an
  // origin.
  kPreconnect = 2,

  // Prefetch operation comes after Preresolve and Preconnect. Chrome spends a
  // significant amount time idle, waiting for the main frame HTML fetch. The
  // prefetch preloading operation fetches the HTML before the user starts
  // navigating in order to load the page much faster.
  kPrefetch = 3,

  // This speedup technique comes with the most impact and overhead. We preload
  // and render a page before the user navigates to it. This will make the next
  // page navigation nearly instant as we would activate a fully prepared
  // RenderFrameHost. Both resources are fetched and JS is executed.
  kPrerender = 4,

  // Like prerendering, it fetches resources in advance; but unlike prerendering
  // it does not execute JavaScript or render any part of the page in advance.
  // NoState prefetch only supports the GET HTTP method and doesn't cache
  // resources with the no-store cache-control header.
  kNoStatePrefetch = 5,
};

// Defines various triggering mechanisms which triggers different preloading
// operations mentioned in preloading.h. The integer portion is used for UKM
// logging, and the string portion is used to dynamically compose UMA histogram
// names. Embedders are allowed to define more predictors.
class CONTENT_EXPORT PreloadingPredictor {
 public:
  constexpr PreloadingPredictor(int64_t ukm_value, base::StringPiece name)
      : ukm_value_(ukm_value), name_(name) {}
  int64_t ukm_value() const { return ukm_value_; }
  base::StringPiece name() const { return name_; }

 private:
  int64_t ukm_value_;
  base::StringPiece name_;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Advance numbering by +1 when adding a new element.
//
// To add additional predictors in content-internals and embedders, wrap the
// new `PreloadingPredictor` in the corresponding namespaces:
// - `content_preloading_predictor` for content-internals;
// - `chrome_preloading_predictor` for Chrome.
//
// See `content/browser/preloading/preloading.h` and
// `chrome/browser/preloading/chrome_preloading.h` for examples.
//
// Please limit content-public `PreloadingPredictor` between 0 to 49
// (inclusive) as 50 and beyond are reserved for content-internal and embedders.
// Both the value and the name should be unique across all the namespaces.
//
// The embedder `PreloadingPredictor` definitions should start at 100 (see
// `chrome/browser/preloading/chrome_preloading.h` for example).
namespace preloading_predictor {
// No PreloadingTrigger is present. This may include the small percentage of
// usages of browser triggers, link-rel, OptimizationGuideService e.t.c which
// will be added later as a separate elements.
static constexpr PreloadingPredictor kUnspecified(0, "Unspecified");

// Preloading is triggered by OnPointerDown event heuristics.
static constexpr PreloadingPredictor kUrlPointerDownOnAnchor(
    1,
    "UrlPointerDownOnAnchor");

// Preloading is triggered by OnPointerHover event heuristics.
static constexpr PreloadingPredictor kUrlPointerHoverOnAnchor(
    2,
    "UrlPointerHoverOnAnchor");

// Preloading was triggered by embedding a keyword for the rel attribute of
// the <link> HTML element to hint to browsers that the user might need it for
// next navigation.
static constexpr PreloadingPredictor kLinkRel(3, "LinkRel");

// TODO(crbug.com/1309934): Add more predictors as we integrate Preloading
// logging.
}  // namespace preloading_predictor

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

  // Preloading operation could be ineligible if it is not triggered
  // because some precondition was not satisfied. Preloading here could
  // be ineligible due to various reasons subjective to the preloading
  // operation like the following.
  // These values are used in both //chrome and //content after integration with
  // various preloading features.

  // Preloading operation was ineligible because preloading was disabled.
  kPreloadingDisabled = 2,

  // Preloading operation was ineligible because it was triggered from the
  // background or a hidden page.
  kHidden = 3,

  // Preloading operation was ineligible because it was invoked for cross origin
  // navigation while preloading was restricted to same-origin navigations.
  // (It's plausible that some preloading mechanisms in the future could work
  // for cross-origin navigations as well.)
  kCrossOrigin = 4,

  // Preloading was ineligible due to low memory restrictions.
  kLowMemory = 5,

  // Preloading was ineligible as running JavaScript was disabled for the URL.
  kJavascriptDisabled = 6,

  // Preloading was ineligible because the Data Saver setting was enabled.
  kDataSaverEnabled = 7,

  // Preloading was ineligible because it was triggered from a page that has an
  // effective url.
  kHasEffectiveUrl = 8,

  // Preloading was ineligible because only single renderer process is only
  // allowed.
  kSingleProcess = 9,

  // Preloading was ineligible for link-rel:next URLs.
  kLinkRelNext = 10,

  // Preloading was ineligible due to the page having third party cookies.
  kThirdPartyCookies = 11,

  // Preloading was ineligible due to being called before we reached the time
  // limit to invoke one more preloading operation.
  kPreloadingInvokedWithinTimelimit = 12,

  // Preloading was ineligible because we can't create a new renderer process
  // for exceeding the renderer processes limit.
  kRendererProcessLimitExceeded = 13,

  // Preloading was ineligible because the Battery Saver setting was enabled.
  kBatterySaverEnabled = 14,

  // Values between `kPreloadingEligibilityCommonEnd` (inclusive) and
  // `kPreloadingEligibilityContentEnd` (exclusive) are reserved for enums
  // defined under `//content`.
  kPreloadingEligibilityCommonEnd = 50,

  // TODO(crbug.com/1309934): Add more specific ineligibility reasons subject to
  // each preloading operation
  // This constant is used to define the value from which embedders can add more
  // enums beyond this value.
  kPreloadingEligibilityContentEnd = 100,
};

// The outcome of the holdback check. This is not part of eligibility status to
// clarify that this check needs to happen after we are done verifying the
// eligibility of a preloading attempt. In general, eligibility checks can be
// reordered, but the holdback check always needs to come after verifying that
// the preloading attempt was eligible.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingHoldbackStatus {
  // The preloading holdback status has not been set yet. This should only
  // happen when the preloading attempt was not eligible.
  kUnspecified = 0,

  // The preload was eligible to be triggered and was not disabled via a field
  // trial holdback. Given enough time, the preload will trigger.
  kAllowed = 1,

  // The preload was eligible to be triggered but was disabled via a field
  // trial holdback. This is useful for measuring the impact of preloading.
  kHoldback = 2,
};

// Defines the post-triggering outcome once the preloading operation is
// triggered.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please update
// "PreloadingTriggeringOutcome" in `tools/metrics/histograms/enums.xml` when
// new enums are added.
enum class PreloadingTriggeringOutcome {
  // The outcome is kUnspecified for attempts that were not triggered due to
  // various ineligibility reasons or due to a field trial holdback.
  kUnspecified = 0,

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

  // Some preloading features don't provide a way to keep track of the
  // progression of the preloading attempt. In those cases, we just log
  // kTriggeredButOutcomeUnknown, meaning that preloading was triggered but we
  // don't know if it was successful.
  kTriggeredButOutcomeUnknown = 7,

  // A preloading feature (e.g., prefetch) completed successfully and was
  // upgraded to Prerender. For PreloadingType::kPrefetch this is different from
  // kSuccess reason which only tracks when the navigation directly uses the
  // Prefetch. Please note that this doesn't ensure Prerender was successful.
  kTriggeredButUpgradedToPrerender = 8,

  // Preloading was triggered but was pending for starting its initial
  // navigation. This outcome should not be recorded when
  // `kPrerender2SequentialPrerendering` is disabled.
  kTriggeredButPending = 9,

  // Required by UMA histogram macro.
  kMaxValue = kTriggeredButPending,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreloadingFailureReason {
  // The failure reason is unspecified if the triggering outcome is not
  // kFailure.
  kUnspecified = 0,

  // This constant is used to define the value from which specifying preloading
  // types can add more enums beyond this value. We mask it by 100 to avoid
  // usage of the same numbers for logging. The semantics of values beyond 100
  // can vary by preloading type (for example 101 might mean "the page was
  // destroyed" for prerender, but "the user already had cookies for a
  // cross-origin prefetch"
  // for prefetch).
  //
  // Values between kPreloadingFailureReasonCommonEnd (included) and
  // kPreloadingFailureReasonContentEnd (excluded) are reserved for enums
  // defined in //content.
  kPreloadingFailureReasonCommonEnd = 100,

  // Values beyond this value are for failure reasons defined by the embedder.
  // The semantics of those values can vary by preloading type (1000 can mean
  // "limit exceeded" for preconnect but "cancelled" for prerender).
  kPreloadingFailureReasonContentEnd = 1000,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOADING_H_
