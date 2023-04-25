// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include <string>

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
BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillDetectRemovedFormControls);
BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill);
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
    kBackForwardCacheDWCOnJavaScriptExecution);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheWithKeepaliveRequest);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundResourceFetch);
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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForOverlayPopupDetection);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForLargeStickyAdDetection);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDisplayLocking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEditingNG);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNavigationPredictor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnchorElementInteraction);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPlzDedicatedWorker);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPortalsCrossOrigin);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFrames);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFullUserAgent);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPath2DPaintCache);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIs);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDefaultViewportIsDeviceWidth);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApi);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInSharedStorage;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInFledge;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiFledgeExtensionsEnabled;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPrivateAggregationApiMaxBudgetPerScope;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPrivateAggregationApiFledgeExtensionsLocalTestingOverride);

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
// SharedStorageManager first checks for any stale entries, purging any that it
// finds.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageStalePurgeInitialInterval;
// Second and subsequent intervals from service startup after
// which SharedStorageManager checks for any stale entries, purging any that it
// finds.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageStalePurgeRecurringInterval;
// Length of time between last key write access and key expiration. When an
// entry's data is older than this threshold, it will be auto-purged.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSharedStorageStalenessThreshold;
// Maximum depth of fenced frame where sharedStorage.selectURL() is allowed to
// be invoked. The depth of a fenced frame is the number of the fenced frame
// boundaries above that frame (i.e. the outermost main frame's frame tree has
// fenced frame depth 0, a topmost fenced frame tree embedded in the outermost
// main frame has fenced frame depth 1, etc).
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageMaxAllowedFencedFrameDepthForSelectURL;

// If enabled, limits the number of times per origin per pageload that
// `sharedStorage.selectURL()` is allowed to be invoked.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageSelectURLLimit);
// Maximum number of bits of entropy per pageload that are allowed to leak via
// `sharedStorage.selectURL()`, if `kSharedStorageSelectURLLimit` is enabled.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageSelectURLBitBudgetPerPageLoad;
// Maximum number of bits of entropy per origin per pageload that are allowed to
// leak via `sharedStorage.selectURL()`, if `kSharedStorageSelectURLLimit` is
// enabled.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageSelectURLBitBudgetPerOriginPerPageLoad;

// Enables the multiple prerendering in a sequential way:
// https://crbug.com/1355151
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2SequentialPrerendering);
// Enables the same-origin main frame navigation in a prerendered page.
// See https://crbug.com/1239281.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2MainFrameNavigation);
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
// Enables to run prerendering for new tabs (e.g., target="_blank").
// See https://crbug.com/1350676.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2InNewTab);

// Fenced Frames:
BLINK_COMMON_EXPORT bool IsFencedFramesEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPreviewsResourceLoadingHintsSpecificResourceTypes);
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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDroppedTouchSequenceIncludesTouchEnd);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFileHandlingIcons);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowSyncXHRInPageDismissal);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchPrivacyChanges);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeJpeg420ImagesToYUV);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeLossyWebPImagesToYUV);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebFontsCacheAwareTimeoutAdaption);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioSinkSelection);

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
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkImageClassifier>
    kForceDarkImageClassifierParam;

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2DHibernation);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvasCompressHibernatedImage);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvasFreeMemoryWhenHidden);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCreateImageBitmapOrientationNone);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheCodeOnIdle);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kCacheCodeOnIdleDelayParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kCacheCodeOnIdleDelayServiceWorkerOnlyParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kProduceCompileHints);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kProduceCompileHintsOnIdleDelayParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kProduceCompileHintsNoiseLevel;

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

// Enables passing of mailbox backed Accelerated bitmap images to be passed
// cross-process as mailbox references instead of serialized bitmaps in
// shared memory.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAcceleratedStaticBitmapImageSerialization);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableScopeExtensions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppManifestLockScreen);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppBorderless);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLoadingTasksUnfreezable);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTargetBlankImpliesNoOpener);

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSendCnameAliasesToSubresourceFilterFromRenderer);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScopeMemoryCachePerContext);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnablePenetratingImageSelection);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundTracingPerformanceMark);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

// Main controls for ad serving API features.
//
// Backend storage + kill switch for Interest Group API origin trials.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInterestGroupStorage);
//
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOwners;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxStoragePerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOpsBeforeMaintenance;
// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledge);

// Configures FLEDGE to consider k-anononymity. If both
// kFledgeConsiderKAnonymity and kFledgeEnforceKAnonymity are on it will be
// enforced; if only kFledgeConsiderKAnonymity is on it will be simulated.
//
// Turning on kFledgeEnforceKAnonymity alone does nothing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeConsiderKAnonymity);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeEnforceKAnonymity);

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
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kBrowsingTopicsMaxEpochIntroductionDelay;
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
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBrowsingTopicsDisabledTopicsList;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBrowsingTopicsBypassIPIsPubliclyRoutableCheck);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopicsXHR);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePageViewportInLCP);

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowDropAlphaForMediaStream);

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCORSErrorsIssueOnly);

// If enabled, the ResourceLoadScheculer will take the current network state
// into consideration, when it plans to delay a low-priority throttleable
// requests in the tight mode. The factors include:
//  - The total number of the in-flight multiplexed connections (e.g.,
//    H2/SPDY/QUIC).
//  - HTTP RTT estimate.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDelayLowPriorityRequestsAccordingToNetworkState);

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
BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMaxUnthrottledTimeoutNestingLevel);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxUnthrottledTimeoutNestingLevelParam;
BLINK_COMMON_EXPORT bool IsMaxUnthrottledTimeoutNestingLevelEnabled();
BLINK_COMMON_EXPORT int GetMaxUnthrottledTimeoutNestingLevel();

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPAnimatedImagesReporting);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPVideoFirstFrame);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEventTimingMatchPresentationIndex);

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultEnabled);

// This flag enables a console warning in cases where document.domain is set
// without origin agent clustering being explicitly disabled.
// (This is a transitory behaviour on the road to perma-enabling
// kOriginAgentClusterDefaultEnabled above.)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultWarning);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSystemColorChooser);

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoForcedFrameUpdatesForWebTests);

// If enabled, the client hints cache will be loaded on browser restarts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDurableClientHintsCache);

// A parameter for kReduceUserAgentMinorVersion;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kUserAgentFrozenBuildVersion;

// Parameters for kReduceUserAgentPlatformOsCpu;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kAllExceptLegacyWindowsPlatform;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLegacyWindowsPlatform;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kRegisterJSSourceLocationBlockingBFCache);

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

// If enabled, a fix for image loading prioritization based on visibility is
// applied. See https://crbug.com/1369823.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kImageLoadingPrioritizationFix);

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostImagePriority);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityImageCount;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityImageSize;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityTightMediumLimit;

// If enabled, allows MediaStreamVideoSource objects to be restarted by a
// successful source switch. Normally, switching the source would only allowed
// on streams that are in started state. However, changing the source also first
// stops the stream before performing the switch and sometimes it can be useful
// to do a change directly on a paused stream.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowSourceSwitchOnPausedVideoMediaStream);

// If enabled, expose non-standard stats in the WebRTC getStats API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcExposeNonStandardStats);

// Whether the pending beacon API is enabled or not.
// https://github.com/WICG/pending-beacon/blob/main/README.md
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
// Details in https://github.com/WICG/pending-beacon/issues/30
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPendingBeaconAPIForcesSendingOnNavigation;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// If enabled, font lookup tables will be prefetched on renderer startup.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchFontLookupTables);
#endif

// If enabled, inline scripts will be stream compiled using a background HTML
// scanner.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrecompileInlineScripts);

// TODO(accessibility): This flag is set to accommodate JAWS on Windows so they
// can adjust to us not simulating click events on a focus action. It is in the
// process of being removed completely and is currently disabled by default on
// all platforms. We want to allow users to manually re-enable this behavior for
// the next few months in case their users discover issues they still have to
// fix. It should be removed by 9/17/2023.
//
// See https://crbug.com/1326622 for more info.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSimulateClickOnAXFocus);

// When enabled, the serialization of accessibility information for the browser
// process will be done during LocalFrameView::RunPostLifecycleSteps, rather
// than from a stand-alone task.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSerializeAccessibilityPostLifecycle);

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedPreloadScanner);

// If enabled, allows the use of WebSQL in non-secure contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLNonSecureContextAccess);

// Enables the Web Machine Learning Neural Network Service to access hardware
// acceleration out of renderer process. Explainer:
// https://github.com/webmachinelearning/webnn/blob/main/explainer.md
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEnableMachineLearningNeuralNetworkService);

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

// Stylus gestures for editable web content.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusRichGestures);

// Stylus handwriting recognition to text input feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusWritingToInput);

// Extended physical keyboard shortcuts for Android.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidExtendedKeyboardShortcuts);

// Apply touch adjustment for stylus pointer events. This feature allows
// enabling functions like writing into a nearby input element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusPointerAdjustment);

// TODO(https://crbug.com/1201109): temporary flag to disable new ArrayBuffer
// size limits, so that tests can be written against code receiving these
// buffers. Remove when the bindings code instituting these limits is removed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableArrayBufferSizeLimitsForTesting);

// If enabled, the HTMLDocumentParser will use a budget based on elapsed time
// rather than token count.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTimedHTMLParserBudget);

// If enabled, the HTMLDocumentParser will only check its budget after parsing a
// commonly slow token or for one out of 10 fast tokens. Note that this feature
// is a no-op if kTimedHTMLParserBudget is disabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCheckHTMLParserBudgetLessOften);

// Allows reading/writing unsanitized content from/to the clipboard. Currently,
// it is only applicable to HTML format. See crbug.com/1268679.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClipboardUnsanitizedContent);

// Make RTCVideoEncoder::Encode() asynchronous.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcEncoderAsyncEncode);

// If enabled, the WebRTC_* threads in peerconnection module will use
// kResourceEfficient thread type.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebRtcThreadsUseResourceEfficientType);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcMetronome);

// If enabled, IME updates are computed at the end of a lifecycle update rather
// than the beginning.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRunTextInputUpdatePostLifecycle);

// If set, HTMLDocumentParser processes data immediately rather than after a
// delay. This is further controlled by the feature params starting with the
// same name. Also note that this only applies to uses that are normally
// deferred (for example, when HTMLDocumentParser is created for inner-html it
// is not deferred).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kProcessHtmlDataImmediately);

// If set, kProcessHtmlDataImmediately impacts child frames. If not set,
// kProcessHtmlDataImmediately does not apply to child frames.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelyChildFrame;

// If set, the first chunk of data available for html processing is processed
// immediately.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelyFirstChunk;

// If set, kProcessHtmlDataImmediately impacts the main frame. If not set,
// kProcessHtmlDataImmediately does not apply to the main frame.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelyMainFrame;

// If set, subsequent chunks of data available for html processing are processed
// immediately.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kProcessHtmlDataImmediatelySubsequentChunks;

// If enabled, some paint property updates (e.g., transform changes) will be
// applied directly instead of using the property tree builder.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFastPathPaintPropertyUpdates);

// If enabled, SVG images will suspend animations when all instances of the
// image are outside of the viewport.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleOffscreenAnimatingSvgImages);

// If enabled, reads and decodes navigation body data off the main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedBodyLoader);

// Whether first-party to third-party different-bucket same-origin post messages
// are blocked.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlocked);

// Whether first-party to third-party different-bucket same-origin post messages
// are blocked when storage partitioning is enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned);

// Whether third-party to first-party different-bucket same-origin post messages
// are blocked.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlocked);

// Whether third-party to first-party different-bucket same-origin post messages
// are blocked when storage partitioning is enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned);

// Whether third-party to third-party different-bucket same-origin post messages
// are blocked.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlocked);

// Whether third-party to third-party different-bucket same-origin post messages
// are blocked when storage partitioning is enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned);

// Combine WebRTC Network and Worker threads. More info at crbug.com/1373439.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIsolateSandboxedIframes);
enum class IsolateSandboxedIframesGrouping {
  // In this grouping, all isolated sandboxed iframes whose URLs share the same
  // site in a given BrowsingInstance will share a process.
  kPerSite,
  // In this grouping, all isolated sandboxed iframes from a given
  // BrowsingInstance whose URLs share the same origin will be isolated in an
  // origin-keyed process.
  kPerOrigin,
  // Unlike the other two modes, which group sandboxed frames per-site or
  // per-origin, this one doesn't do any grouping at all and uses one process
  // per document.
  kPerDocument,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    IsolateSandboxedIframesGrouping>
    kIsolateSandboxedIframesGroupingParam;

// Flag to control whether about:blank and srcdoc iframes use newly proposed
// base url inheritance behavior from https://crbug.com/1356658.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNewBaseUrlInheritanceBehavior);

// This function checks both kNewBaseUrlInheritanceBehavior and
// kIsolateSandboxedIframes and returns true if either is enabled.
BLINK_COMMON_EXPORT bool IsNewBaseUrlInheritanceBehaviorEnabled();

// These control a major serialization change to include information about
// exposed interfaces in trailer data, to allow emergency fixes.
// Regardless, data which might have been serialized to disk must continue to be
// deserializable. These should be removed after a couple milestones.
//
// See https://crbug.com/1341844.
//
// `kSSVTrailerWriteNewVersion`
//   If disabled, Blink will revert to writing a pre-trailer format.
//   This will become impractical once any other incompatible wire format
//   changes are made.
// `kSSVTrailerWriteExposureAssertion`
//   If enabled, Blink will include assertions about which interfaces are
//   exposed in trailers to serialized messages. Has no effect if
//   kSSVTrailerWriteNewVersion is disabled.
// `kSSVTrailerEnforceExposureAssertion`
//   If enabled, Blink will reject messages which cannot be deserialized in the
//   current realm. Otherwise, all interfaces will be treated as exposed in all
//   contexts for the purposes of serialization.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSSVTrailerWriteNewVersion);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSSVTrailerWriteExposureAssertion);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSSVTrailerEnforceExposureAssertion);

// Forces the attribute powerPreference to be set to "high-performance" for
// WebGL contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceHighPerformanceGPUForWebGL);

// Process device and display capture requests on different queues.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSplitUserMediaQueues);

// Use TextCodecCJK for encoding/decoding CJK except for Big5.
// If the flag is disabled TextCodecICU would be used instead.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTextCodecCJKEnabled);

// Make the browser decide when to turn on the capture indicator (red button)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kStartMediaStreamCaptureIndicatorInBrowser);

// Causes MediaStreamVideoSource video frames to be transported on a
// SequencedTaskRunner backed by the threadpool instead of the normal IO thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseThreadPoolForMediaStreamVideoTaskRunner);

// Forces same-process display:none cross-origin iframes to be throttled in the
// same manner that OOPIFs are.
// Note: this feature should never be accessed directly. Instead, use
// IsThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesEnabled defined
// below.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes);

// Use to determine if iframe throttling is enabled via the feature
// kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes and not disabled
// via enterprise policy.
BLINK_COMMON_EXPORT bool
IsThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesEnabled();

// Allows certain origin trials to be enabled using third-party tokens
// associated with the origin of external speculation rules.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSpeculationRulesHeaderEnableThirdPartyOriginTrial);

// Controls whether the SpeculationRulesPrefetchFuture origin trial can be
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculationRulesPrefetchFuture);

// Feature for allowing page with open IDB connection to be
// stored in back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowPageWithIDBConnectionInBFCache);

// Feature for allowing page with open IDB transaction to be stored in
// back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowPageWithIDBTransactionInBFCache);

// Checks both of kAllowPageWithIDBConnectionInBFCache and
// kAllowPageWithIDBTransactionInBFCache are turned on when determining if a
// page with IndexedDB transaction is eligible for BFCache.
BLINK_COMMON_EXPORT bool
IsAllowPageWithIDBConnectionAndTransactionInBFCacheEnabled();

// Kill switch for using a custom task runner in the blink scheduler that makes
// DeleteSoon/ReleaseSoon less prone to memory leaks.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseBlinkSchedulerTaskRunnerWithCustomDeleter);

// Extend ScriptResource's lifetime to match its payload's lifetime.
// See https://crbug.com/1393246.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExtendScriptResourceLifetime);

// Use WebIDL instead of iteration to populate RTCStatsReport.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcStatsReportIdl);

// Makes preloaded fonts render-blocking up to the limits below.
// See https://crbug.com/1412861
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderBlockingFonts);

// Max milliseconds from navigation start that fonts can block rendering.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxBlockingTimeMsForRenderBlockingFonts;

// Max milliseconds that font are allowed to delay of FCP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxFCPDelayMsForRenderBlockingFonts;

// Whether Sec-CH-UA headers on subresource fetches that contain an empty
// string should be quoted (`""`) as they are for navigation fetches. See
// https://crbug.com/1416925.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kQuoteEmptySecChUaStringHeadersConsistently);

// The number of "automatic" implicit storage access grants per third-party
// origin that can be granted.
//
// Note that if `kStorageAccessAPIAutoGrantInFPS` and
// `kStorageAccessAPIAutoDenyOutsideFPS` are both true, then this parameter has
// no effect.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kStorageAccessAPIImplicitGrantLimit;
// Whether to auto-grant storage access requests when the top level origin and
// the requesting origin are in the same First-Party Set.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIAutoGrantInFPS;
// Whether to auto-deny storage access requests when the top level origin and
// the requesting origin are not in the same First-Party Set.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIAutoDenyOutsideFPS;

// Kill-switch for a deprecation trial that unpartitions storage in third-party
// contexts under the registered top-level site. If
// `kDisableThirdPartyStoragePartitioningDeprecationTrial` is enabled, the
// deprecation trial information can be sent to and enabled in the browser
// process (i.e. when the base::Feature is enabled, the deprecation trial is
// enabled in the browser process too).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableThirdPartyStoragePartitioningDeprecationTrial);

// Kill-switch for any calls to the  mojo interface
// RuntimeFeatureStateController in the RuntimeFeatureStateOverrideContext
// class. If `kRuntimeFeatureStateControllerApplyFeatureDiff` is disabled,
// origin/deprecation trial token information is not sent to the browser
// process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kRuntimeFeatureStateControllerApplyFeatureDiff);

// Disallow setting URL ports with a value that will overflow.
// See https://crbug.com/1416017
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kURLSetPortCheckOverflow);

// Keep strong references in the blink memory cache to maximize resource reuse.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryCacheStrongReference);

// Save only one unloaded page's resources in the memory cache.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceSingleUnload);

// Exclude images from the saved strong references for resources.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceFilterImages);

// Exclude scripts from the saved strong references for resources.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceFilterScripts);

// If enabled, renderers look for cached resources from another renderer
// that has the same process isolation policies. Note that renderers don't
// use cached resources in other rendereres yet, just record histograms.
// See https://crbug.com/1414262
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRemoteResourceCache);

// Kill-switch for the fetch keepalive request infra migration.
// If enabled, all keepalive requests will be proxied via the browser process.
// Design Doc: https://bit.ly/chromium-keepalive-migration
// Tracker: https://crbug.com/1356128
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKeepAliveInBrowserMigration);

// Switch to enabling rendering of gainmap-based HDR images.
// Tracker: https://crbug.com/1404000
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGainmapHdrImages);

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
