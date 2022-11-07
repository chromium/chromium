// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnonymousIframeOriginTrial);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutomaticLazyFrameLoadingToAds);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kTimeoutMillisForLazyAds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSkipFrameCountForLazyAds;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutomaticLazyFrameLoadingToEmbeds);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kTimeoutMillisForLazyEmbeds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSkipFrameCountForLazyEmbeds;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutomaticLazyFrameLoadingToEmbedUrls);
enum class AutomaticLazyFrameLoadingToEmbedLoadingStrategy {
  kAllowList,
  kNonAds,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AutomaticLazyFrameLoadingToEmbedLoadingStrategy>
    kAutomaticLazyFrameLoadingToEmbedLoadingStrategyParam;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheDedicatedWorker);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBlockingDownloadsInAdFrameWithoutUserActivation);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConversionMeasurement);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExcludeLowEntropyImagesFromLCP);
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kMinimumEntropyForLCP;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGMSCoreEmoji);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPaintHolding);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPaintHoldingCrossOrigin);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEagerCacheStorageSetupForServiceWorkers);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreaming);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSmallScriptStreaming);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConsumeCodeCacheOffThread);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUserLevelMemoryPressureSignal);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFreezePurgeMemoryAllPagesFrozen);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForOverlayPopupDetection);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForLargeStickyAdDetection);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDisplayLocking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEditingNG);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLayoutNGBlockInInline);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNavigationPredictor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnchorElementInteraction);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOSKResizesVisualViewportByDefault);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPlzDedicatedWorker);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPortalsCrossOrigin);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFrames);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFullUserAgent);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPath2DPaintCache);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIs);

enum class FencedFramesImplementationType {
  kShadowDOM,
  kMPArch,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    FencedFramesImplementationType>
    kFencedFramesImplementationTypeParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPI);
// Maximum number of URLs allowed to be included in the input parameter for
// runURLSelectionOperation().
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit;
// Maximum length of Shared Storage script key and script value.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageStringLength;
// Maximum number of database entries at a time that any single origin is
// permitted.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageEntriesPerOrigin;
// Maximum database page size in bytes. Must be a power of two between
// 512 and 65536, inclusive.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStoragePageSize;
// Maximum database in-memory cache size, in pages.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageCacheSize;
// Maximum number of tries to initialize the database.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageInitTries;
// Maximum number of keys or key-value pairs returned in each batch by
// the async `keys()` and `entries()` iterators, respectively.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageIteratorBatchSize;
// Maximum number of bits of entropy allowed per origin to output via the Shared
// Storage API.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageBitBudget;
// Interval over which `kSharedStorageBitBudget` is defined.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageBudgetInterval;
// Initial interval from service startup after which
// SharedStorageManager first checks for any stale origins, purging any that it
// finds.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageStaleOriginPurgeInitialInterval;
// Second and subsequent intervals from service startup after
// which SharedStorageManager checks for any stale origins, purging any that it
// finds.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageStaleOriginPurgeRecurringInterval;
// Length of time between origin creation and origin expiration. When an
// origin's data is older than this threshold, it will be auto-purged.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageOriginStalenessThreshold;
// Maximum depth of fenced frame where sharedStorage.selectURL() is allowed to
// be invoked. The depth of a fenced frame is the number of the fenced frame
// boundaries above that frame (i.e. the outermost main frame's frame tree has
// fenced frame depth 0, a topmost fenced frame tree embedded in the outermost
// main frame has fenced frame depth 1, etc).
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageMaxAllowedFencedFrameDepthForSelectURL;

// Enables the multiple prerendering in a sequential way:
// https://crbug.com/1355151
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2SequentialPrerendering);

// The number of prerenderings that can run concurrently. This only applies for
// prerenderings triggered by speculation rules.
BLINK_COMMON_EXPORT extern const char
    kPrerender2MaxNumOfRunningSpeculationRules[];
// Enables restrictions on how much memory is required on a device to use
// Prerender2. This is a separate feature from kPrerender2 so that the
// restrictions can be disabled entirely to allow bots to run the tests without
// needing to explicitly enable Prerender2, which some tests do not want to do
// because they want to test the default behavior.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2MemoryControls);
// A field trial param that controls how much physical memory is required on a
// device to use Prerender2. If the device's physical memory does not exceed
// this value, pages will not be prerendered even when kPrerender2 is enabled.
BLINK_COMMON_EXPORT extern const char kPrerender2MemoryThresholdParamName[];
// A field trial param that controls how much physical memory is allowed to be
// used by Chrome. If the current memory usage in Chrome exceeds this percent,
// pages will not be prerendered even when kPrerender2 is enabled.
BLINK_COMMON_EXPORT extern const char
    kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName[];
// Enables same-site cross origin Prerender2
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSameSiteCrossOriginForSpeculationRulesPrerender);
// Enables to keep prerenderings alive in the background when their visibility
// state changes to HIDDEN.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2InBackground);
// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsPrerender2Enabled();
// Returns true when the same-site cross origin Prerender2 feature is
// enabled.
BLINK_COMMON_EXPORT bool
IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled();
// Returns true if the Android On-Screen-Keyboard is in "resize visual
// viewport" mode.
BLINK_COMMON_EXPORT bool OSKResizesVisualViewportByDefault();

// Fenced Frames:
BLINK_COMMON_EXPORT bool IsFencedFramesEnabled();
// Note: This performs a string comparison on the feature param which is slow.
// When possible, prefer to use the equivalent accessors on blink::Page in the
// renderer and on content::FrameTree in the browser, which cache the value.
BLINK_COMMON_EXPORT bool IsFencedFramesMPArchBased();
BLINK_COMMON_EXPORT bool IsFencedFramesShadowDOMBased();

// Whether we will create initial NavigationEntry or not on FrameTree creation,
// which also impacts the session history replacement decisions made in the
// renderer.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInitialNavigationEntry);
BLINK_COMMON_EXPORT bool IsInitialNavigationEntryEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPreviewsResourceLoadingHintsSpecificResourceTypes);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPurgeRendererMemoryWhenBackgrounded);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRTCOfferExtmapAllowMixed);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRTCGpuCodecSupportWaiter);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kRTCGpuCodecSupportWaiterTimeoutParam;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kV8OptimizeWorkersForPerformance);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebMeasureMemoryViaPerformanceManager);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcMultiplexCodec);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcHideLocalIpsWithMdns);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIntensiveWakeUpThrottling);
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[];
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleForegroundTimers);

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResourceLoadViaDataPipe);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerUpdateDelay);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStopInBackground);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDropInputEventsBeforeFirstPaint);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFileHandlingIcons);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowSyncXHRInPageDismissal);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchPrivacyChanges);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeJpeg420ImagesToYUV);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeLossyWebPImagesToYUV);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebFontsCacheAwareTimeoutAdaption);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePriority);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLightweightNoStatePrefetch);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSaveDataImgSrcset);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceWebContentsDarkMode);
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkImageBehavior>
    kForceDarkImageBehaviorParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkForegroundLightnessThresholdParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkBackgroundLightnessThresholdParam;

// Returns true when PlzDedicatedWorker is enabled.
BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseMinMaxVEADimensions);

// Blink garbage collection.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapCompaction);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapConcurrentMarking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapConcurrentSweeping);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapIncrementalMarking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapIncrementalMarkingStress);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetLowPriorityForBeacon);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheStorageCodeCacheHintHeader);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDispatchBeforeUnloadOnFreeze);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyCanvas2dImageChromium);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebviewAccelerateSmallCanvases);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2dStaysGPUOnReadback);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDiscardCodeCacheAfterFirstUse);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheCodeOnIdle);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kCacheCodeOnIdleDelayParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAlignFontDisplayAutoTimeoutWithLCPGoal);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam;
enum class AlignFontDisplayAutoTimeoutWithLCPGoalMode {
  kToFailurePeriod,
  kToSwapPeriod
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleInstallingServiceWorker);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInstallingServiceWorkerOutstandingThrottledLimit;

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gatherig accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInputPredictorTypeChoice);

// Enables resampling input events on main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingInputEvents);

// Elevates the InputTargetClient mojo interface to input, since its input
// blocking.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInputTargetClientHighPriority);

// Enables resampling GestureScroll events on compositor thread.
// Uses the kPredictorName* values in ui_base_features.h as the 'predictor'
// feature param.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingScrollEvents);

// Enables filtering of predicted scroll events on compositor thread.
// Uses the kFilterName* values in ui_base_features.h as the 'filter' feature
// param.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFilteringScrollPrediction);

// Enables changing the influence of acceleration based on change of direction.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKalmanHeuristics);

// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKalmanDirectionCutOff);

// Parameters for blink::features::kSkipTouchEventFilter.
// Which event types will be always forwarded is controlled by the "type"
// FeatureParam, which can be either "discrete" (default) or "all".
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamName[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamValueDiscrete[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamValueAll[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterFilteringProcessParamName[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[];
BLINK_COMMON_EXPORT
extern const char
    kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[];

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCompressParkableStrings);
BLINK_COMMON_EXPORT bool ParkableStringsUseSnappy();
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSnappyForParkableStrings);
BLINK_COMMON_EXPORT bool IsParkableStringsToDiskEnabled();
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelayFirstParkingOfStrings);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kReducedReferrerGranularity);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kContentCaptureConstantStreaming);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCheckOfflineCapability);
enum class CheckOfflineCapabilityMode {
  kWarnOnly,
  kEnforce,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<CheckOfflineCapabilityMode>
    kCheckOfflineCapabilityParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreferCompositingToLCDText);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableUrlHandlers);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppManifestLockScreen);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppBorderless);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLoadingTasksUnfreezable);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTargetBlankImpliesNoOpener);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMediaStreamTrackUseConfigMaxFrameRate);

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSendCnameAliasesToSubresourceFilterFromRenderer);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDisableDocumentDomainByDefault);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScopeMemoryCachePerContext);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnablePenetratingImageSelection);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundTracingPerformanceMark);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSanitizerAPINamespaces);

// Kill switch for the blocking of the navigation of top from a cross origin
// iframe to a different scheme. TODO(https://crbug.com/1151507): Remove in
// M92.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBlockCrossOriginTopNavigationToDiffentScheme);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kJXL);

// Main controls for ad serving API features.
//
// Backend storage + kill switch for Interest Group API origin trials.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInterestGroupStorage);
//
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOwners;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOpsBeforeMaintenance;
// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledge);
// Runtime flag that changes default Permissions Policy for features
// join-ad-interest-group and run-ad-auction to a more restricted EnableForSelf.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAdInterestGroupAPIRestrictedPolicyByDefault);
// Debug reporting runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBiddingAndScoringDebugReportingAPI);
// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowURNsInIframes);

// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopics);
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kBrowsingTopicsTimePeriodPerEpoch;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsNumberOfEpochsToExpose;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsNumberOfTopTopicsPerEpoch;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsUseRandomTopicProbabilityPercent;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsConfigVersion;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBrowsingTopicsTaxonomyVersion;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBrowsingTopicsBypassIPIsPubliclyRoutableCheck);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePageViewportInLCP);

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowDropAlphaForMediaStream);

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCORSErrorsIssueOnly);

// Makes Persistent quota the same as Temporary quota.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPersistentQuotaIsTemporaryQuota);
BLINK_COMMON_EXPORT bool IsPersistentQuotaIsTemporaryQuota();

// If enabled, the ResourceLoadScheculer will take the current network state
// into consideration, when it plans to delay a low-priority throttleable
// requests in the tight mode. The factors include:
//  - The total number of the in-flight multiplexed connections (e.g.,
//    H2/SPDY/QUIC).
//  - HTTP RTT estimate.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDelayLowPriorityRequestsAccordingToNetworkState);

// When enabled, this turns off an LCP calculation optimization that's ignoring
// initially invisible images, and resulting in LCP correctness issues. See
// https://crbug.com/1249622
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIncludeInitiallyInvisibleImagesInLCP);

// When enabled, this includes SVG background images in LCP calculation.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIncludeBackgroundSVGInLCP);

// Number of the requests that can be handled in the tight mode.
BLINK_COMMON_EXPORT
extern const base::FeatureParam<int> kMaxNumOfThrottleableRequestsInTightMode;

// The HTTP RTT threshold: decide whether the
// `kDelayLowPriorityRequestsAccordingToNetworkState` feature can take effect
// practically according to the network connection state.
BLINK_COMMON_EXPORT
extern const base::FeatureParam<base::TimeDelta> kHttpRttThreshold;

// The cost reduction for the multiplexed requests when
// `kDelayLowPriorityRequestsAccordingToNetworkState` is enabled.
BLINK_COMMON_EXPORT
extern const base::FeatureParam<double> kCostReductionOfMultiplexedRequests;

// If enabled, the major version number returned by Chrome will be locked at
// 99. The minor version number returned by Chrome will be forced to the
// value of the major version number. The purpose of this
// feature is a back up plan for if the major version moving from
// two to three digits breaks unexpected things.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kForceMajorVersionInMinorPositionInUserAgent);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth_DEPRECATED);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetTimeoutWithoutClamp);
// window.setTimeout() has a feature to remove 1ms clamp to improve performance
// and battery life. Enterprise policy can override this to control the feature.
// Normally, the result of this feature calculation is cached; allow tests
// to clear the cache to recompute the feature value.
BLINK_COMMON_EXPORT void
ClearSetTimeoutWithout1MsClampPolicyOverrideCacheForTesting();
BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMaxUnthrottledTimeoutNestingLevel);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxUnthrottledTimeoutNestingLevelParam;
BLINK_COMMON_EXPORT void ClearUnthrottledNestedTimeoutOverrideCacheForTesting();
BLINK_COMMON_EXPORT bool IsMaxUnthrottledTimeoutNestingLevelEnabled();
BLINK_COMMON_EXPORT int GetMaxUnthrottledTimeoutNestingLevel();

// If enabled, ContentToVisibleTimeReporter logs
// Browser.Tabs.TotalSwitchDuration2.* instead of
// Browser.Tabs.TotalSwitchDuration.*.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTabSwitchMetrics2);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPAnimatedImagesReporting);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEarlyBodyLoad);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEarlyCodeCache);

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultEnabled);

// This flag enables a console warning in cases where document.domain is set
// without origin agent clustering being explicitly disabled.
// (This is a transitory behaviour on the road to perma-enabling
// kOriginAgentClusterDefaultEnabled above.)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultWarning);

#if BUILDFLAG(IS_ANDROID)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchAndroidFonts);
#endif

// Allows pages that support App Install Banners to stay eligible for the
// back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheAppBanner);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDefaultStyleSheetsEarlyInit);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSystemColorChooser);

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoForcedFrameUpdatesForWebTests);

// If enabled, the client hints cache will be loaded on browser restarts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDurableClientHintsCache);

// Gates Multi-Screen Window Placement additional enhancements.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWindowPlacementFullscreenCompanionWindow);

// A parameter for kReduceUserAgentMinorVersion;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kUserAgentFrozenBuildVersion;

// Parameters for kReduceUserAgentPlatformOsCpu;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kAllExceptLegacyWindowsPlatform;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLegacyWindowsPlatform;

// If enabled, we only report FCP if there’s a successful commit to the
// compositor. Otherwise, FCP may be reported if first BeginMainFrame results in
// a commit failure (see crbug.com/1257607).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kReportFCPOnlyOnSuccessfulCommit);

// If enabled, the `CropTarget.fromElement()` method will allow for the use
// of additional element tag tyeps, instead of just <div> and <iframe>.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRegionCaptureExperimentalSubtypes);

// Experiment for measuring how often an overridden User-Agent string is made by
// appending or prepending to the original User-Agent string.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUserAgentOverrideExperiment);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLAccess);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUACHOverrideBlank);

#if BUILDFLAG(IS_WIN)
// Enables prewarming the default font families.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrewarmDefaultFontFamilies);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmStandard;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmFixed;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmSerif;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmSansSerif;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmCursive;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmFantasy;
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsSaveData);

// Enables establishing the GPU channel asnchronously when requesting a new
// layer tree frame sink.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEstablishGpuChannelAsync);

// If enabled, script source text will be decoded and hashed off the main
// thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeScriptSourceOffThread);

// If enabled, async script execution will be delayed than usual.
// See https://crbug.com/1340837.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelayAsyncScriptExecution);
enum class DelayAsyncScriptDelayType {
  kFinishedParsing,
  kFirstPaintOrFinishedParsing,
  kEachLcpCandidate,
  kEachPaint,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<DelayAsyncScriptDelayType>
    kDelayAsyncScriptExecutionDelayParam;
enum class DelayAsyncScriptTarget {
  kAll,
  kCrossSiteOnly,
  // Unlike other options (that are more like scheduling changes within the
  // spec),  kCrossSiteWithAllowList and kCrossSiteWithAllowListReportOnly are
  // used only for LazyEmbeds intervention.
  kCrossSiteWithAllowList,
  kCrossSiteWithAllowListReportOnly,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<DelayAsyncScriptTarget>
    kDelayAsyncScriptTargetParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDelayAsyncScriptExecutionDelayLimitParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDelayAsyncScriptExecutionFeatureLimitParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kDelayAsyncScriptAllowList;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kDelayAsyncScriptExecutionMainFrameOnlyParam;

// If enabled, async scripts will be run on a lower priority task queue.
// See https://crbug.com/1348467.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowPriorityAsyncScriptExecution);
// The timeout value for kLowPriorityAsyncScriptExecution. Async scripts run on
// lower priority queue until this timeout elapsed.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTimeoutForLowPriorityAsyncScriptExecution;
// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |low_pri_async_exec_feature_limit|. Zero value means no limit.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kLowPriorityAsyncScriptExecutionFeatureLimitParam;
// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam;
// kLowPriorityAsyncScriptExecution will be applied only for main frame's
// scripts.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionMainFrameOnlyParam;

// If enabled, async scripts will be loaded with a lower fetch priority.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowPriorityScriptLoading);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityScriptLoadingCrossSiteOnlyParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kLowPriorityScriptLoadingFeatureLimitParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kLowPriorityScriptLoadingDenyListParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityScriptLoadingMainFrameOnlyParam;

// If enabled, DOMContentLoaded will be fired after all async scripts are
// executed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDOMContentLoadedWaitForAsyncScript);

// If enabled, parser-blocking scripts are force-deferred.
// https://crbug.com/1339112
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceDeferScriptIntervention);

// If enabled, parser-blocking scripts are loaded asynchronously but the
// execution order is respected. See https://crbug.com/1344772
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceInOrderScript);

// If enabled, parser-blocking scripts are loaded asynchronously. The target
// scripts are selectively applied via the allowlist provided from the feature
// param. See https://crbug.com/1356396
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSelectiveInOrderScript);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSelectiveInOrderScriptTarget);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kSelectiveInOrderScriptAllowList;

// If enabled, allows MediaStreamVideoSource objects to be restarted by a
// successful source switch. Normally, switching the source would only allowed
// on streams that are in started state. However, changing the source also first
// stops the stream before performing the switch and sometimes it can be useful
// to do a change directly on a paused stream.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowSourceSwitchOnPausedVideoMediaStream);

// If enabled, expose non-standard stats in the WebRTC getStats API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcExposeNonStandardStats);

// If enabled, CSS rulesets with many different rules on the same attribute
// will be attempted accelerated with a substring set tree.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSubstringSetTreeForAttributeBuckets);

// If enabled, style invalidation will use a Bloom filter for storing
// CSS classes that need (only) self-invalidation, instead of having them
// in the main hash map.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInvalidationSetClassBloomFilter);

// Whether the pending beacon API is enabled or not.
// https://github.com/WICG/unload-beacon/blob/main/README.md
// - kPendingBeaconAPI = {true: {"requires_origin_trial": false}} to enable the
//   features globally.
// - kPendingBeaconAPI = {true: {"requires_origin_trial": true}} to enable the
//   features only for execution context with OT token.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPendingBeaconAPI);
// If true, the execution context from client request needs to have OT token in
// it, in addition to `kPendingBeaconAPI` being set to true, such that the API
// can be enabled. If false, setting `kPendingBeaconAPI` to true enable the API
// both in Chromium & in Blink.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPendingBeaconAPIRequiresOriginTrial;
// Allows control to decide whether to forced sending out beacons on navigating
// away a page (transitioning to dispatch pagehide event).
// Details in https://github.com/WICG/unload-beacon/issues/30
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPendingBeaconAPIForcesSendingOnNavigation;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// If enabled, font lookup tables will be prefetched on renderer startup.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchFontLookupTables);
#endif

// If enabled, inline scripts will be stream compiled using a background HTML
// scanner.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrecompileInlineScripts);

// If enabled, CSS will be tokenized in a background thread when possible.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPretokenizeCSS);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPretokenizeInlineSheets;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPretokenizeExternalSheets;

// TODO(accessibility): This flag is set to accommodate JAWS on Windows so they
// can adjust to us not simulating click events on a focus action. It should be
// disabled by default (and removed) before 5/17/2023.
// See https://crbug.com/1326622 for more info.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSimulateClickOnAXFocus);

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedPreloadScanner);

// If enabled, allows the use of WebSQL in non-secure contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLNonSecureContextAccess);

// Switch to temporary turn back on file system url navigation.
// TODO(https://crbug.com/1332598): Remove this feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFileSystemUrlNavigation);

// TODO(https://crbug.com/1360512): this feature creates a carveout for
// enabling filesystem: URL navigation within Chrome Apps regardless of whether
// kFileSystemUrlNavigation is enabled or not.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFileSystemUrlNavigationForChromeAppsOnly);

// Early exit when the style or class attribute of an element is set to the same
// value as before.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEarlyExitOnNoopClassOrStyleChange);

// Stylus handwriting recognition to text input feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusWritingToInput);

// TODO(https://crbug.com/1201109): temporary flag to disable new ArrayBuffer
// size limits, so that tests can be written against code receiving these
// buffers. Remove when the bindings code instituting these limits is removed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableArrayBufferSizeLimitsForTesting);

// If enabled, the HTMLDocumentParser will use a budget based on elapsed time
// rather than token count.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTimedHTMLParserBudget);

// Allows reading/writing unsanitized content from/to the clipboard. Currently,
// it is only applicable to HTML format. See crbug.com/1268679.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClipboardUnsanitizedContent);

// If set, HTMLTokenizer is run on a background thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedHtmlTokenizer);

// The maximum number of tokens the background thread will generate before
// NextParseResults() is called.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kThreadedHtmlTokenizerTokenMaxCount;

// If enabled, the WebRTC_* threads in peerconnection module will use
// kResourceEfficient thread type.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebRtcThreadsUseResourceEfficientType);

// If enabled, fine-grained UMA metrics for IntersectionObserver will only be
// collected on 10% of animation frames.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleIntersectionObserverUMA);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcMetronome);

// If enabled, all of FileSystemAccessSyncAccessHandle methods are synchronous.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSyncAccessHandleAllSyncSurface);

// Disables centralized browser-side management of web cache memory limits.
//
// TODO(crbug.com/1340565): Remove once the data is available.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoCentralWebCacheLimitControl);

// If enabled, IME updates are computed at the end of a lifecycle update rather
// than the beginning.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRunTextInputUpdatePostLifecycle);

// If set, HTMLDocumentParser processes data immediately rather than after a
// delay. This is further controlled by the feature params starting with the
// same name. Also note that this only applies to uses that are normally
// deferred (for example, when HTMLDocumentParser is created for inner-html it
// is not deferred).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kProcessHtmlDataImmediately);

// If set, the first chunk of data available for html processing is processed
// immediately.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelyFirstChunk;

// If set, subsequent chunks of data available for html processing are processed
// immediately.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelySubsequentChunks;

// If enabled, some paint property updates (e.g., transform changes) will be
// applied directly instead of using the property tree builder.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFastPathPaintPropertyUpdates);

// If enabled, wildcard subdomains are supported in permissions policies.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWildcardSubdomainsInPermissionsPolicy);

// If enabled, reads and decodes navigation body data off the main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedBodyLoader);

// If enabled, will cache for each node's EventPath::NodePath in document.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDocumentEventNodePathCaching);

// When an application calls getDisplayMedia(), a media-picker is displayed
// to the user, allowing them to share a tab, a window or a screen.
// * If this flag is enabled, the order is - tabs, windows, screens.
// * If this flag is disabled, the order is - screens, windows, tabs.
//
// If {preferCurrentTab: true} is specified, the order is unaffected.
//
// When the new order is used, the default value of selfBrowserSurface
// is "exclude", unless {preferCurrentTab: true} is specified, in which
// case the default value is "include".
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNewGetDisplayMediaPickerOrder);

// Parameter for tuning max entries allowed in EventNodePathCache, which will be
// used to do LRU eviction in document.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kDocumentMaxEventNodePathCachedEntries;

// Whether same-origin different-partition post messages are currently blocked.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageDifferentPartitionSameOriginBlocked);

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
