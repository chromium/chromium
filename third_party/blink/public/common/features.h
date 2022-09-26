// Copyright 2018 The Chromium Authors. All rights reserved.
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

BLINK_COMMON_EXPORT extern const base::Feature kAnonymousIframeOriginTrial;
BLINK_COMMON_EXPORT extern const base::Feature kAutomaticLazyFrameLoadingToAds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kTimeoutMillisForLazyAds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSkipFrameCountForLazyAds;
BLINK_COMMON_EXPORT extern const base::Feature
    kAutomaticLazyFrameLoadingToEmbeds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kTimeoutMillisForLazyEmbeds;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSkipFrameCountForLazyEmbeds;
BLINK_COMMON_EXPORT extern const base::Feature
    kAutomaticLazyFrameLoadingToEmbedUrls;
enum class AutomaticLazyFrameLoadingToEmbedLoadingStrategy {
  kAllowList,
  kNonAds,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AutomaticLazyFrameLoadingToEmbedLoadingStrategy>
    kAutomaticLazyFrameLoadingToEmbedLoadingStrategyParam;
BLINK_COMMON_EXPORT extern const base::Feature kBackForwardCacheDedicatedWorker;

// - kBackForwardCacheSendNotRestoredReasons = {true: {"requires_origin_trial":
// false}} to enable the features globally.
// - kBackForwardCacheSendNotRestoredReasons = {true: {"requires_origin_trial":
// true}} to enable the features only for execution context with OT token.
BLINK_COMMON_EXPORT extern const base::Feature
    kBackForwardCacheSendNotRestoredReasons;
// If true, the execution context from client request needs to have OT token in
// it, in addition to `kBackForwardCacheSendNotRestoredReasons` being set to
// true, such that the API can be enabled. If false, setting
// `kBackForwardCacheSendNotRestoredReasons` to true enable the API both in
// Chromium & in Blink.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kBackForwardCacheSendNotRestoredReasonsRequiresOriginTrial;

BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kConversionMeasurement;
BLINK_COMMON_EXPORT extern const base::Feature kExcludeLowEntropyImagesFromLCP;
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kMinimumEntropyForLCP;
BLINK_COMMON_EXPORT extern const base::Feature kGMSCoreEmoji;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHolding;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHoldingCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kConsumeCodeCacheOffThread;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForOverlayPopupDetection;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForLargeStickyAdDetection;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature kEditingNG;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNGBlockInInline;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature kAnchorElementInteraction;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortalsCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature kFencedFrames;
BLINK_COMMON_EXPORT extern const base::Feature kFullUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature kPath2DPaintCache;
BLINK_COMMON_EXPORT extern const base::Feature kPrivacySandboxAdsAPIs;

enum class FencedFramesImplementationType {
  kShadowDOM,
  kMPArch,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    FencedFramesImplementationType>
    kFencedFramesImplementationTypeParam;

BLINK_COMMON_EXPORT extern const base::Feature kSharedStorageAPI;
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

// Enables the multiple prerendering in a sequential way:
// https://crbug.com/1355151
BLINK_COMMON_EXPORT extern const base::Feature
    kPrerender2SequentialPrerendering;

// The number of prerenderings that can run concurrently. This only applies for
// prerenderings triggered by speculation rules.
BLINK_COMMON_EXPORT extern const char
    kPrerender2MaxNumOfRunningSpeculationRules[];
// Enables restrictions on how much memory is required on a device to use
// Prerender2. This is a separate feature from kPrerender2 so that the
// restrictions can be disabled entirely to allow bots to run the tests without
// needing to explicitly enable Prerender2, which some tests do not want to do
// because they want to test the default behavior.
BLINK_COMMON_EXPORT extern const base::Feature kPrerender2MemoryControls;
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
BLINK_COMMON_EXPORT extern const base::Feature
    kSameSiteCrossOriginForSpeculationRulesPrerender;
// Enables to keep prerenderings alive in the background when their visibility
// state changes to HIDDEN.
BLINK_COMMON_EXPORT extern const base::Feature kPrerender2InBackground;
// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsPrerender2Enabled();
// Returns true when the same-site cross origin Prerender2 feature is
// enabled.
BLINK_COMMON_EXPORT bool
IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled();

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
BLINK_COMMON_EXPORT extern const base::Feature kInitialNavigationEntry;
BLINK_COMMON_EXPORT bool IsInitialNavigationEntryEnabled();

BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGpuCodecSupportWaiter;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kRTCGpuCodecSupportWaiterTimeoutParam;
BLINK_COMMON_EXPORT extern const base::Feature kV8OptimizeWorkersForPerformance;
BLINK_COMMON_EXPORT extern const base::Feature
    kWebMeasureMemoryViaPerformanceManager;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcHideLocalIpsWithMdns;
BLINK_COMMON_EXPORT extern const base::Feature
    kWebRtcIgnoreUnspecifiedColorSpace;

BLINK_COMMON_EXPORT extern const base::Feature kIntensiveWakeUpThrottling;
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[];
BLINK_COMMON_EXPORT extern const base::Feature kThrottleForegroundTimers;

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcH264WithOpenH264FFmpeg;
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kDropInputEventsBeforeFirstPaint;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingIcons;
BLINK_COMMON_EXPORT extern const base::Feature kAllowSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchPrivacyChanges;

BLINK_COMMON_EXPORT extern const base::Feature kDecodeJpeg420ImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;

BLINK_COMMON_EXPORT extern const base::Feature
    kAudioWorkletThreadRealtimePriority;

BLINK_COMMON_EXPORT extern const base::Feature kLightweightNoStatePrefetch;

BLINK_COMMON_EXPORT extern const base::Feature kSaveDataImgSrcset;

BLINK_COMMON_EXPORT extern const base::Feature kForceWebContentsDarkMode;
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

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcUseMinMaxVEADimensions;

// Blink garbage collection.
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapCompaction;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentMarking;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentSweeping;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapIncrementalMarking;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkHeapIncrementalMarkingStress;

BLINK_COMMON_EXPORT extern const base::Feature kSetLowPriorityForBeacon;

BLINK_COMMON_EXPORT extern const base::Feature kCacheStorageCodeCacheHintHeader;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT extern const base::Feature kDispatchBeforeUnloadOnFreeze;

BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dImageChromium;

BLINK_COMMON_EXPORT extern const base::Feature kDawn2dCanvas;

BLINK_COMMON_EXPORT extern const base::Feature kWebviewAccelerateSmallCanvases;

BLINK_COMMON_EXPORT extern const base::Feature kCanvas2dStaysGPUOnReadback;

BLINK_COMMON_EXPORT extern const base::Feature kDiscardCodeCacheAfterFirstUse;

BLINK_COMMON_EXPORT extern const base::Feature kCacheCodeOnIdle;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kCacheCodeOnIdleDelayParam;

BLINK_COMMON_EXPORT extern const base::Feature
    kAlignFontDisplayAutoTimeoutWithLCPGoal;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam;
enum class AlignFontDisplayAutoTimeoutWithLCPGoalMode {
  kToFailurePeriod,
  kToSwapPeriod
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam;

BLINK_COMMON_EXPORT extern const base::Feature kThrottleInstallingServiceWorker;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInstallingServiceWorkerOutstandingThrottledLimit;

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gatherig accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
BLINK_COMMON_EXPORT extern const base::Feature kInputPredictorTypeChoice;

// Enables resampling input events on main thread.
BLINK_COMMON_EXPORT extern const base::Feature kResamplingInputEvents;

// Elevates the InputTargetClient mojo interface to input, since its input
// blocking.
BLINK_COMMON_EXPORT extern const base::Feature kInputTargetClientHighPriority;

// Enables resampling GestureScroll events on compositor thread.
// Uses the kPredictorName* values in ui_base_features.h as the 'predictor'
// feature param.
BLINK_COMMON_EXPORT extern const base::Feature kResamplingScrollEvents;

// Enables filtering of predicted scroll events on compositor thread.
// Uses the kFilterName* values in ui_base_features.h as the 'filter' feature
// param.
BLINK_COMMON_EXPORT extern const base::Feature kFilteringScrollPrediction;

// Enables changing the influence of acceleration based on change of direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanHeuristics;

// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanDirectionCutOff;

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

BLINK_COMMON_EXPORT extern const base::Feature kCompressParkableStrings;
BLINK_COMMON_EXPORT bool ParkableStringsUseSnappy();
BLINK_COMMON_EXPORT extern const base::Feature kUseSnappyForParkableStrings;
BLINK_COMMON_EXPORT bool IsParkableStringsToDiskEnabled();
BLINK_COMMON_EXPORT extern const base::Feature kDelayFirstParkingOfStrings;

BLINK_COMMON_EXPORT extern const base::Feature kReducedReferrerGranularity;

BLINK_COMMON_EXPORT extern const base::Feature kContentCaptureConstantStreaming;

BLINK_COMMON_EXPORT extern const base::Feature kCheckOfflineCapability;
enum class CheckOfflineCapabilityMode {
  kWarnOnly,
  kEnforce,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<CheckOfflineCapabilityMode>
    kCheckOfflineCapabilityParam;

BLINK_COMMON_EXPORT extern const base::Feature
    kBackForwardCacheABExperimentControl;
BLINK_COMMON_EXPORT
extern const char kBackForwardCacheABExperimentGroup[];

BLINK_COMMON_EXPORT extern const base::Feature kPreferCompositingToLCDText;

BLINK_COMMON_EXPORT extern const base::Feature
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableUrlHandlers;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppManifestLockScreen;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppBorderless;

BLINK_COMMON_EXPORT extern const base::Feature kLoadingTasksUnfreezable;

BLINK_COMMON_EXPORT extern const base::Feature kTargetBlankImpliesNoOpener;

BLINK_COMMON_EXPORT extern const base::Feature
    kMediaStreamTrackUseConfigMaxFrameRate;

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT extern const base::Feature
    kSendCnameAliasesToSubresourceFilterFromRenderer;

BLINK_COMMON_EXPORT extern const base::Feature kDisableDocumentDomainByDefault;

BLINK_COMMON_EXPORT extern const base::Feature kScopeMemoryCachePerContext;

BLINK_COMMON_EXPORT extern const base::Feature kEnablePenetratingImageSelection;

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT extern const base::Feature
    kBackgroundTracingPerformanceMark;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

BLINK_COMMON_EXPORT extern const base::Feature kSanitizerAPINamespaces;

// Kill switch for the blocking of the navigation of top from a cross origin
// iframe to a different scheme. TODO(https://crbug.com/1151507): Remove in
// M92.
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockCrossOriginTopNavigationToDiffentScheme;

BLINK_COMMON_EXPORT extern const base::Feature kJXL;

// Main controls for ad serving API features.
//
// Backend storage + kill switch for Interest Group API origin trials.
BLINK_COMMON_EXPORT extern const base::Feature kInterestGroupStorage;
//
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOwners;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOpsBeforeMaintenance;
// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT extern const base::Feature kFledge;
// Runtime flag that changes default Permissions Policy for features
// join-ad-interest-group and run-ad-auction to a more restricted EnableForSelf.
BLINK_COMMON_EXPORT extern const base::Feature
    kAdInterestGroupAPIRestrictedPolicyByDefault;
// Debug reporting runtime flag/JS API.
BLINK_COMMON_EXPORT extern const base::Feature
    kBiddingAndScoringDebugReportingAPI;
// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT extern const base::Feature kAllowURNsInIframes;

// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled();

BLINK_COMMON_EXPORT extern const base::Feature kBrowsingTopics;
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
BLINK_COMMON_EXPORT extern const base::Feature
    kBrowsingTopicsBypassIPIsPubliclyRoutableCheck;

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT extern const base::Feature kUsePageViewportInLCP;

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BLINK_COMMON_EXPORT extern const base::Feature kAllowDropAlphaForMediaStream;

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BLINK_COMMON_EXPORT extern const base::Feature kCORSErrorsIssueOnly;

// Makes Persistent quota the same as Temporary quota.
BLINK_COMMON_EXPORT
extern const base::Feature kPersistentQuotaIsTemporaryQuota;
BLINK_COMMON_EXPORT bool IsPersistentQuotaIsTemporaryQuota();

// If enabled, the ResourceLoadScheculer will take the current network state
// into consideration, when it plans to delay a low-priority throttleable
// requests in the tight mode. The factors include:
//  - The total number of the in-flight multiplexed connections (e.g.,
//    H2/SPDY/QUIC).
//  - HTTP RTT estimate.
BLINK_COMMON_EXPORT extern const base::Feature
    kDelayLowPriorityRequestsAccordingToNetworkState;

// When enabled, this turns off an LCP calculation optimization that's ignoring
// initially invisible images, and resulting in LCP correctness issues. See
// https://crbug.com/1249622
BLINK_COMMON_EXPORT extern const base::Feature
    kIncludeInitiallyInvisibleImagesInLCP;

// When enabled, this includes SVG background images in LCP calculation.
BLINK_COMMON_EXPORT extern const base::Feature kIncludeBackgroundSVGInLCP;

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
BLINK_COMMON_EXPORT extern const base::Feature
    kForceMajorVersionInMinorPositionInUserAgent;

BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDeviceMemory;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDPR;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsResourceWidth;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsViewportWidth;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsDeviceMemory_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDPR_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsResourceWidth_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsViewportWidth_DEPRECATED;

BLINK_COMMON_EXPORT extern const base::Feature kSetTimeoutWithoutClamp;
// window.setTimeout() has a feature to remove 1ms clamp to improve performance
// and battery life. Enterprise policy can override this to control the feature.
// Normally, the result of this feature calculation is cached; allow tests
// to clear the cache to recompute the feature value.
BLINK_COMMON_EXPORT void
ClearSetTimeoutWithout1MsClampPolicyOverrideCacheForTesting();
BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

BLINK_COMMON_EXPORT extern const base::Feature
    kMaxUnthrottledTimeoutNestingLevel;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxUnthrottledTimeoutNestingLevelParam;
BLINK_COMMON_EXPORT void ClearUnthrottledNestedTimeoutOverrideCacheForTesting();
BLINK_COMMON_EXPORT bool IsMaxUnthrottledTimeoutNestingLevelEnabled();
BLINK_COMMON_EXPORT int GetMaxUnthrottledTimeoutNestingLevel();

// If enabled, ContentToVisibleTimeReporter logs
// Browser.Tabs.TotalSwitchDuration2.* instead of
// Browser.Tabs.TotalSwitchDuration.*.
BLINK_COMMON_EXPORT extern const base::Feature kTabSwitchMetrics2;

BLINK_COMMON_EXPORT extern const base::Feature kLCPAnimatedImagesReporting;

BLINK_COMMON_EXPORT extern const base::Feature kEarlyBodyLoad;

BLINK_COMMON_EXPORT extern const base::Feature kEarlyCodeCache;

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT extern const base::Feature
    kOriginAgentClusterDefaultEnabled;

// This flag enables a console warning in cases where document.domain is set
// without origin agent clustering being explicitly disabled.
// (This is a transitory behaviour on the road to perma-enabling
// kOriginAgentClusterDefaultEnabled above.)
BLINK_COMMON_EXPORT extern const base::Feature
    kOriginAgentClusterDefaultWarning;

#if BUILDFLAG(IS_ANDROID)
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchAndroidFonts;
#endif

// Allows pages that support App Install Banners to stay eligible for the
// back/forward cache.
BLINK_COMMON_EXPORT extern const base::Feature kBackForwardCacheAppBanner;

// Enables back/forward cache for non-plugin embeds.
// TODO(crbug.com/1325192): Remove once the bug is resolved.
BLINK_COMMON_EXPORT
extern const base::Feature kBackForwardCacheEnabledForNonPluginEmbed;

BLINK_COMMON_EXPORT extern const base::Feature kDefaultStyleSheetsEarlyInit;

BLINK_COMMON_EXPORT extern const base::Feature kSystemColorChooser;

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT extern const base::Feature kNoForcedFrameUpdatesForWebTests;

// If enabled, the client hints cache will be loaded on browser restarts.
BLINK_COMMON_EXPORT extern const base::Feature kDurableClientHintsCache;

// Gates Multi-Screen Window Placement additional enhancements.
BLINK_COMMON_EXPORT extern const base::Feature
    kWindowPlacementFullscreenCompanionWindow;

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
BLINK_COMMON_EXPORT extern const base::Feature kReportFCPOnlyOnSuccessfulCommit;

// If enabled, the `CropTarget.fromElement()` method will allow for the use
// of additional element tag tyeps, instead of just <div> and <iframe>.
BLINK_COMMON_EXPORT extern const base::Feature
    kRegionCaptureExperimentalSubtypes;

// Experiment for measuring how often an overridden User-Agent string is made by
// appending or prepending to the original User-Agent string.
BLINK_COMMON_EXPORT extern const base::Feature kUserAgentOverrideExperiment;

BLINK_COMMON_EXPORT extern const base::Feature kWebSQLAccess;

BLINK_COMMON_EXPORT extern const base::Feature kUACHOverrideBlank;

#if BUILDFLAG(IS_WIN)
// Enables prewarming the default font families.
BLINK_COMMON_EXPORT extern const base::Feature kPrewarmDefaultFontFamilies;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmStandard;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmFixed;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmSerif;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmSansSerif;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmCursive;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool> kPrewarmFantasy;
#endif

BLINK_COMMON_EXPORT extern const base::Feature kClientHintsSaveData;

// Enables establishing the GPU channel asnchronously when requesting a new
// layer tree frame sink.
BLINK_COMMON_EXPORT extern const base::Feature kEstablishGpuChannelAsync;

// If enabled, script source text will be decoded and hashed off the main
// thread.
BLINK_COMMON_EXPORT extern const base::Feature kDecodeScriptSourceOffThread;

// If enabled, async script execution will be delayed than usual.
// See https://crbug.com/1340837.
BLINK_COMMON_EXPORT extern const base::Feature kDelayAsyncScriptExecution;
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
BLINK_COMMON_EXPORT extern const base::Feature kDelayAsyncScriptUrls;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kDelayAsyncScriptAllowList;

// If enabled, async scripts will be run on a lower priority task queue.
// See https://crbug.com/1348467.
BLINK_COMMON_EXPORT extern const base::Feature kLowPriorityAsyncScriptExecution;
// The timeout value for kLowPriorityAsyncScriptExecution. Async scripts run on
// lower priority queue until this timeout elapsed.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTimeoutForLowPriorityAsyncScriptExecution;
// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |feature_limit|. Zero value means no limit.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kLowPriorityAsyncScriptExecutionFeatureLimitParam;
// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam;

// If enabled, DOMContentLoaded will be fired after all async scripts are
// executed.
BLINK_COMMON_EXPORT extern const base::Feature
    kDOMContentLoadedWaitForAsyncScript;

// If enabled, parser-blocking scripts are force-deferred.
// https://crbug.com/1339112
BLINK_COMMON_EXPORT extern const base::Feature kForceDeferScriptIntervention;

// If enabled, parser-blocking scripts are loaded asynchronously but the
// execution order is respected. See https://crbug.com/1344772
BLINK_COMMON_EXPORT extern const base::Feature kForceInOrderScript;

// If enabled, parser-blocking scripts are loaded asynchronously. The target
// scripts are selectively applied via the allowlist provided from the feature
// param. See https://crbug.com/1356396
BLINK_COMMON_EXPORT extern const base::Feature kSelectiveInOrderScript;
BLINK_COMMON_EXPORT extern const base::Feature kSelectiveInOrderScriptTarget;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kSelectiveInOrderScriptAllowList;

// If enabled, allows MediaStreamVideoSource objects to be restarted by a
// successful source switch. Normally, switching the source would only allowed
// on streams that are in started state. However, changing the source also first
// stops the stream before performing the switch and sometimes it can be useful
// to do a change directly on a paused stream.
BLINK_COMMON_EXPORT extern const base::Feature
    kAllowSourceSwitchOnPausedVideoMediaStream;

// If enabled, expose non-standard stats in the WebRTC getStats API.
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcExposeNonStandardStats;

// If enabled, CSS rulesets with many different rules on the same attribute
// will be attempted accelerated with a substring set tree.
BLINK_COMMON_EXPORT extern const base::Feature
    kSubstringSetTreeForAttributeBuckets;

// If enabled, CSS parsing will attempt to use an arena for temporary
// allocations of certain structures when parsing selectors.
BLINK_COMMON_EXPORT extern const base::Feature kCSSParserSelectorArena;

// Whether the pending beacon API is enabled or not.
// https://github.com/WICG/unload-beacon/blob/main/README.md
// - kPendingBeaconAPI = {true: {"requires_origin_trial": false}} to enable the
//   features globally.
// - kPendingBeaconAPI = {true: {"requires_origin_trial": true}} to enable the
//   features only for execution context with OT token.
BLINK_COMMON_EXPORT extern const base::Feature kPendingBeaconAPI;
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
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchFontLookupTables;
#endif

// If enabled, inline scripts will be stream compiled using a background HTML
// scanner.
BLINK_COMMON_EXPORT extern const base::Feature kPrecompileInlineScripts;

// If enabled, CSS will be tokenized in a background thread when possible.
BLINK_COMMON_EXPORT extern const base::Feature kPretokenizeCSS;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPretokenizeInlineSheets;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPretokenizeExternalSheets;

// TODO(accessibility): This flag is set to accommodate JAWS on Windows so they
// can adjust to us not simulating click events on a focus action. It should be
// disabled by default (and removed) before 5/17/2023.
// See https://crbug.com/1326622 for more info.
BLINK_COMMON_EXPORT extern const base::Feature kSimulateClickOnAXFocus;

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT extern const base::Feature kThreadedPreloadScanner;

// If enabled, allows the use of WebSQL in non-secure contexts.
BLINK_COMMON_EXPORT extern const base::Feature kWebSQLNonSecureContextAccess;

// Switch to temporary turn back on file system url navigation.
// TODO(https://crbug.com/1332598): Remove this feature.
BLINK_COMMON_EXPORT extern const base::Feature kFileSystemUrlNavigation;

// Early exit when the style or class attribute of an element is set to the same
// value as before.
BLINK_COMMON_EXPORT extern const base::Feature
    kEarlyExitOnNoopClassOrStyleChange;

// Stylus handwriting recognition to text input feature.
BLINK_COMMON_EXPORT extern const base::Feature kStylusWritingToInput;

// TODO(https://crbug.com/1201109): temporary flag to disable new ArrayBuffer
// size limits, so that tests can be written against code receiving these
// buffers. Remove when the bindings code instituting these limits is removed.
BLINK_COMMON_EXPORT extern const base::Feature
    kDisableArrayBufferSizeLimitsForTesting;

// If enabled, the HTMLDocumentParser will use a budget based on elapsed time
// rather than token count.
BLINK_COMMON_EXPORT extern const base::Feature kTimedHTMLParserBudget;

// Allows reading/writing unsanitized content from/to the clipboard. Currently,
// it is only applicable to HTML format. See crbug.com/1268679.
BLINK_COMMON_EXPORT extern const base::Feature kClipboardUnsanitizedContent;

// If set, HTMLTokenizer is run on a background thread.
BLINK_COMMON_EXPORT extern const base::Feature kThreadedHtmlTokenizer;

// The maximum number of tokens the background thread will generate before
// NextParseResults() is called.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kThreadedHtmlTokenizerTokenMaxCount;

// If enabled, the WebRTC_* threads in peerconnection module will use
// kResourceEfficient thread type.
BLINK_COMMON_EXPORT extern const base::Feature
    kWebRtcThreadsUseResourceEfficientType;

// If enabled, fine-grained UMA metrics for IntersectionObserver will only be
// collected on 10% of animation frames.
BLINK_COMMON_EXPORT extern const base::Feature kThrottleIntersectionObserverUMA;

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcMetronome;

// If enabled, all of FileSystemAccessSyncAccessHandle methods are synchronous.
BLINK_COMMON_EXPORT extern const base::Feature kSyncAccessHandleAllSyncSurface;

// If enabled, some paint property updates (e.g., transform changes) will be
// applied directly instead of using the property tree builder.
BLINK_COMMON_EXPORT extern const base::Feature kFastPathPaintPropertyUpdates;

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
