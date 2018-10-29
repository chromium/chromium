// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "build/build_config.h"

namespace blink {
namespace features {

const base::Feature kAutofillPreviewStyleExperiment{
    "AutofillPreviewStyleExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable mojo Blob URL interface and better blob URL lifetime management.
// Can be enabled independently of NetworkService.
const base::Feature kMojoBlobURLs{"MojoBlobURLs",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Nested workers. See https://crbug.com/31666
const base::Feature kNestedWorkers{"NestedWorkers",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Onion souping for all DOMStorage. https://crbug.com/781870
const base::Feature kOnionSoupDOMStorage{"OnionSoupDOMStorage",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
const base::Feature kPortals{"Portals", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kRecordAnchorMetricsClicked is enabled, then metrics of anchor elements
// clicked by the user will be extracted and recorded.
// If kRecordAnchorMetricsVisible is enabled, then metrics of anchor elements
// in the first viewport after the page load will be extracted and recorded.
const base::Feature kRecordAnchorMetricsClicked{
    "RecordAnchorMetricsClicked", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kRecordAnchorMetricsVisible{
    "RecordAnchorMetricsVisible", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kServiceWorkerImportedScriptUpdateCheck{
    "ServiceWorkerImportedScriptUpdateCheck",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables reading a subresource's body data and side data in parallel.
const base::Feature kServiceWorkerParallelSideDataReading{
    "ServiceWorkerParallelSideDataReading", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable new service worker glue for NetworkService. Can be
// enabled independently of NetworkService.
const base::Feature kServiceWorkerServicification{
    "ServiceWorkerServicification", base::FEATURE_ENABLED_BY_DEFAULT};

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopInBackground {
  "stop-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Freeze non-timer task queues in background, after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopNonTimersInBackground {
  "stop-non-timers-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Writable files and native filesystem access. https://crbug.com/853326
const base::Feature kWritableFilesAPI{"WritableFilesAPI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Tracks "jank" from layout objects changing their visual location between
// animation frames (see crbug.com/581518).
const base::Feature kJankTracking{"JankTracking",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Changes the default RTCPeerConnection constructor behavior to use Unified
// Plan as the SDP semantics. When the feature is enabled, Unified Plan is used
// unless the default is overridden (by passing {sdpSemantics:'plan-b'} as the
// argument).
const base::Feature kRTCUnifiedPlanByDefault{"RTCUnifiedPlanByDefault",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const char kAutofillPreviewStyleExperimentBgColorParameterName[] = "bg_color";

const char kAutofillPreviewStyleExperimentColorParameterName[] = "color";

const char kMixedContentAutoupgradeModeParamName[] = "mode";
const char kMixedContentAutoupgradeModeBlockable[] = "blockable";
const char kMixedContentAutoupgradeModeOptionallyBlockable[] =
    "optionally-blockable";

}  // namespace features
}  // namespace blink
