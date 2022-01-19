// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature
    kAutomaticLazyFrameLoadingToEmbeds;
BLINK_COMMON_EXPORT extern const base::Feature kBackForwardCacheDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kCOEPForSharedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kCOLRV1Fonts;
BLINK_COMMON_EXPORT extern const base::Feature kCSSContainerQueries;
BLINK_COMMON_EXPORT extern const base::Feature kConversionMeasurement;
BLINK_COMMON_EXPORT extern const base::Feature kGMSCoreEmoji;
BLINK_COMMON_EXPORT extern const base::Feature
    kHandwritingRecognitionWebPlatformApiFinch;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHolding;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHoldingCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kConsumeCodeCacheOffThread;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kReduceUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForOverlayPopupDetection;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForLargeStickyAdDetection;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature kJSONModules;
BLINK_COMMON_EXPORT extern const base::Feature kForceSynchronousHTMLParsing;
BLINK_COMMON_EXPORT extern const base::Feature kEditingNG;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature kPortalsCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature kFencedFrames;
BLINK_COMMON_EXPORT extern const base::Feature kUserAgentClientHint;
BLINK_COMMON_EXPORT extern const base::Feature
    kUserAgentClientHintFullVersionList;
BLINK_COMMON_EXPORT extern const base::Feature
    kPrefersColorSchemeClientHintHeader;
BLINK_COMMON_EXPORT extern const base::Feature kViewportHeightClientHintHeader;

enum class FencedFramesImplementationType {
  kShadowDOM,
  kMPArch,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    FencedFramesImplementationType>
    kFencedFramesImplementationTypeParam;

BLINK_COMMON_EXPORT extern const base::Feature kSharedStorageAPI;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit;

// Prerender2:
// Enables the Prerender2 feature: https://crbug.com/1126305
// But see comments in the .cc file also.
BLINK_COMMON_EXPORT extern const base::Feature kPrerender2;
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
// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsPrerender2Enabled();

// Fenced Frames:
BLINK_COMMON_EXPORT bool IsFencedFramesEnabled();
BLINK_COMMON_EXPORT bool IsFencedFramesMPArchBased();

BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kWindowOpenNewPopupBehavior;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature
    kRTCDisallowPlanBOutsideDeprecationTrial;
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
BLINK_COMMON_EXPORT extern const base::Feature kSpeculationRulesPrefetchProxy;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStorageAccessAPI;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kCssSelectorFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kFontAccess;
BLINK_COMMON_EXPORT extern const base::Feature kComputePressure;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingAPI;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingIcons;
BLINK_COMMON_EXPORT extern const base::Feature kAllowSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchPrivacyChanges;

BLINK_COMMON_EXPORT extern const base::Feature kDecodeJpeg420ImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingFocusWithoutUserActivation;

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
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    ForceDarkIncreaseTextContrast>
    kForceDarkIncreaseTextContrastParam;

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

BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkCompositorUseDisplayThreadPriority;

BLINK_COMMON_EXPORT extern const base::Feature kBackfaceVisibilityInterop;

BLINK_COMMON_EXPORT extern const base::Feature kSetLowPriorityForBeacon;

BLINK_COMMON_EXPORT extern const base::Feature kCacheStorageCodeCacheHintHeader;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT extern const base::Feature kDispatchBeforeUnloadOnFreeze;

BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dImageChromium;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dSwapChain;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyWebGLSwapChain;

BLINK_COMMON_EXPORT extern const base::Feature kDawn2dCanvas;

BLINK_COMMON_EXPORT extern const base::Feature kWebviewAccelerateSmallCanvases;

BLINK_COMMON_EXPORT extern const base::Feature kSoftwareCanvas2DOnReadback;

BLINK_COMMON_EXPORT extern const base::Feature kDiscardCodeCacheAfterFirstUse;

BLINK_COMMON_EXPORT extern const base::Feature kCacheCodeOnIdle;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kCacheCodeOnIdleDelayParam;

// TODO(crbug.com/920069): Remove OffsetParentNewSpecBehavior after the feature
// is in stable with no issues.
BLINK_COMMON_EXPORT extern const base::Feature kOffsetParentNewSpecBehavior;

BLINK_COMMON_EXPORT extern const base::Feature kKeepScriptResourceAlive;

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

// Enables the device-memory, resource-width, viewport-width and DPR client
// hints to be sent to third-party origins if the first-party has opted in to
// receiving client hints, regardless of Permissions Policy.
BLINK_COMMON_EXPORT extern const base::Feature kAllowClientHintsToThirdParty;

// Enables filtering of predicted scroll events on compositor thread.
// Uses the kFilterName* values in ui_base_features.h as the 'filter' feature
// param.
BLINK_COMMON_EXPORT extern const base::Feature kFilteringScrollPrediction;

// Enables changing the influence of acceleration based on change of direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanHeuristics;

// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanDirectionCutOff;

// Skips the browser touch event filter, ensuring that events that reach the
// queue and would otherwise be filtered out will instead be passed onto the
// renderer compositor process as long as the page hasn't timed out. If
// skip_filtering_process is browser_and_renderer, also skip the renderer cc
// touch event filter, ensuring that events will be passed onto the renderer
// main thread. Which event types will be always forwarded is controlled by the
// "type" FeatureParam, which can be either "discrete" (default) or "all".
BLINK_COMMON_EXPORT
extern const base::Feature kSkipTouchEventFilter;
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

BLINK_COMMON_EXPORT extern const base::Feature kCLSScrollAnchoring;

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

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableDarkMode;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableHandleLinks;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableIsolatedStorage;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableLaunchHandler;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableLinkCapturing;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableManifestId;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableTranslations;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableUrlHandlers;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableProtocolHandlers;

BLINK_COMMON_EXPORT extern const base::Feature kLoadingTasksUnfreezable;

BLINK_COMMON_EXPORT extern const base::Feature
    kNetworkRequestUsesOnlyPerProcessBufferLimit;

BLINK_COMMON_EXPORT extern const base::Feature kTargetBlankImpliesNoOpener;

BLINK_COMMON_EXPORT extern const base::Feature
    kMediaStreamTrackUseConfigMaxFrameRate;

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT extern const base::Feature
    kSendCnameAliasesToSubresourceFilterFromRenderer;

BLINK_COMMON_EXPORT extern const base::Feature kInterestCohortAPIOriginTrial;

BLINK_COMMON_EXPORT extern const base::Feature kInterestCohortFeaturePolicy;

BLINK_COMMON_EXPORT extern const base::Feature kDisableDocumentDomainByDefault;

BLINK_COMMON_EXPORT extern const base::Feature kScopeMemoryCachePerContext;

BLINK_COMMON_EXPORT extern const base::Feature kEnablePenetratingImageSelection;

BLINK_COMMON_EXPORT extern const base::Feature kDocumentTransition;
BLINK_COMMON_EXPORT extern const base::Feature kDocumentTransitionRenderer;

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT extern const base::Feature
    kBackgroundTracingPerformanceMark;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

BLINK_COMMON_EXPORT extern const base::Feature kSanitizerAPI;
BLINK_COMMON_EXPORT extern const base::Feature kManagedConfiguration;

// Kill switch for the blocking of the navigation of top from a cross origin
// iframe to a different scheme. TODO(https://crbug.com/1151507): Remove in
// M92.
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockCrossOriginTopNavigationToDiffentScheme;

BLINK_COMMON_EXPORT extern const base::Feature kJXL;

// Forces same-process display:none cross-origin iframes to be throttled in the
// same manner that OOPIFs are.
BLINK_COMMON_EXPORT extern const base::Feature
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes;

// Main controls for ad serving API features.
//
// Backend storage + kill switch for Interest Group API  origin trials.
BLINK_COMMON_EXPORT extern const base::Feature kInterestGroupStorage;
//
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOwners;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOpsBeforeMaintenance;
// Interest Group JS API/runtimeflag.
BLINK_COMMON_EXPORT extern const base::Feature kAdInterestGroupAPI;
// PARAKEET ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT extern const base::Feature kParakeet;
// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT extern const base::Feature kFledge;
// Changes default Permissions Policy for features join-ad-interest-group and
// run-ad-auction to a more restricted EnableForSelf.
BLINK_COMMON_EXPORT extern const base::Feature
    kAdInterestGroupAPIRestrictedPolicyByDefault;
// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT extern const base::Feature kAllowURNsInIframes;

// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled();

// Control switch for minimizing processing in the WebRTC APM when all audio
// tracks are disabled.
BLINK_COMMON_EXPORT extern const base::Feature
    kMinimizeAudioProcessingForUnusedOutput;

// When <dialog>s are closed, this focuses the "previously focused" element
// which had focus when the <dialog> was first opened.
// TODO(crbug.com/649162): Remove DialogFocusNewSpecBehavior after
// the feature is in stable with no issues.
BLINK_COMMON_EXPORT extern const base::Feature kDialogFocusNewSpecBehavior;

// Makes autofill look across shadow boundaries when collecting form controls to
// fill.
BLINK_COMMON_EXPORT extern const base::Feature kAutofillShadowDOM;

// Allows read/write of custom formats with unsanitized clipboard content. See
// crbug.com/106449.
BLINK_COMMON_EXPORT extern const base::Feature kClipboardCustomFormats;

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT extern const base::Feature kUsePageViewportInLCP;

// Send all user interaction latency data from renderer to the browser process.
BLINK_COMMON_EXPORT extern const base::Feature kSendAllUserInteractionLatencies;

// Enable "Sec-CH-UA-Platform" client hint and request header for all requests
BLINK_COMMON_EXPORT extern const base::Feature kUACHPlatformEnabledByDefault;

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BLINK_COMMON_EXPORT extern const base::Feature kAllowDropAlphaForMediaStream;

BLINK_COMMON_EXPORT extern const base::Feature kThirdPartyStoragePartitioning;

BLINK_COMMON_EXPORT extern const base::Feature kDesktopPWAsSubApps;

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BLINK_COMMON_EXPORT extern const base::Feature kCORSErrorsIssueOnly;

// Makes Persistent quota the same as Temporary quota.
BLINK_COMMON_EXPORT
extern const base::Feature kPersistentQuotaIsTemporaryQuota;

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

// If enabled, the minor version number returned by Chrome will be forced to
// 100.  This feature is only applicable for M96-M99 and will be removed after
// M99.  The purpose of this feature is to allow testing of mitigation
// strategies for conveying the major version number in the minor version string
// if we decide to freeze the major version at 99.
BLINK_COMMON_EXPORT extern const base::Feature kForceMinorVersion100InUserAgent;

// If enabled, the major version number returned by Chrome will be forced to
// 100.  This feature is only applicable for M96-M99 and will be removed after
// M99.  The purpose of this feature is to allow users to test and proactively
// fix any issues as we approach a 3-digit major version number.
BLINK_COMMON_EXPORT extern const base::Feature kForceMajorVersion100InUserAgent;

BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDeviceMemory;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDPR;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsResourceWidth;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsViewportWidth;
BLINK_COMMON_EXPORT extern const base::Feature
    kWebSQLInThirdPartyContextEnabled;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsDeviceMemory_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature kClientHintsDPR_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsResourceWidth_DEPRECATED;
BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsViewportWidth_DEPRECATED;

// https://drafts.csswg.org/css-cascade-5/#layering
BLINK_COMMON_EXPORT extern const base::Feature kCSSCascadeLayers;

BLINK_COMMON_EXPORT extern const base::Feature kSetTimeoutWithoutClamp;
BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

// If enabled, ContentToVisibleTimeReporter logs
// Browser.Tabs.TotalSwitchDuration2.* instead of
// Browser.Tabs.TotalSwitchDuration.*.
BLINK_COMMON_EXPORT extern const base::Feature kTabSwitchMetrics2;

BLINK_COMMON_EXPORT extern const base::Feature kLCPAnimatedImagesReporting;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebSQLInThirdPartyContextThrowsWhenDisabled;

// https://blog.whatwg.org/newline-normalizations-in-form-submission
BLINK_COMMON_EXPORT extern const base::Feature kLateFormNewlineNormalization;

// TODO(crbug.com/1185950): Remove this flag when the feature is fully launched
// and released to stable with no issues.
BLINK_COMMON_EXPORT extern const base::Feature kAutoExpandDetailsElement;

BLINK_COMMON_EXPORT extern const base::Feature kEarlyCodeCache;

BLINK_COMMON_EXPORT extern const base::Feature
    kClientHintsMetaHTTPEquivAcceptCH;

BLINK_COMMON_EXPORT extern const base::Feature kClientHintsMetaNameAcceptCH;

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

BLINK_COMMON_EXPORT extern const base::Feature kClientHintThirdPartyDelegation;

#if defined(OS_ANDROID)
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchAndroidFonts;
#endif

BLINK_COMMON_EXPORT extern const base::Feature
    kLazyInitializeTimeZoneController;

BLINK_COMMON_EXPORT extern const base::Feature kCompositedCaret;

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
