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

// -----------------------------------------------------------------------------
// Feature declarations and associated constants (feature params, et cetera)
//
// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When declaring feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// Enables passing of mailbox backed Accelerated bitmap images to be passed
// cross-process as mailbox references instead of serialized bitmaps in
// shared memory.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAcceleratedStaticBitmapImageSerialization);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdAuctionReportingWithMacroApi);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdAuctionSignals);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kAdAuctionSignalsMaxSizeBytes;

// Runtime flag that changes default Permissions Policy for features
// join-ad-interest-group and run-ad-auction to a more restricted EnableForSelf.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAdInterestGroupAPIRestrictedPolicyByDefault);

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

// Allows running DevTools main thread debugger even when a renderer process
// hosts multiple main frames.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowDevToolsMainThreadDebuggerForMultipleMainFrames);

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowDropAlphaForMediaStream);

// Enables rate obfuscation mitigation in compute pressure, to prevent
// cross-channel attacks.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kComputePressureRateObfuscationMitigation);

// Feature for allowing page with open IDB connection to be
// stored in back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowPageWithIDBConnectionInBFCache);
// Feature for allowing page with open IDB transaction to be stored in
// back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowPageWithIDBTransactionInBFCache);

// If enabled, allows MediaStreamVideoSource objects to be restarted by a
// successful source switch. Normally, switching the source would only allowed
// on streams that are in started state. However, changing the source also first
// stops the stream before performing the switch and sometimes it can be useful
// to do a change directly on a paused stream.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowSourceSwitchOnPausedVideoMediaStream);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowSyncXHRInPageDismissal);

// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowURNsInIframes);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnchorElementInteraction);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnchorElementMouseMotionEstimator);

// Extended physical keyboard shortcuts for Android.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidExtendedKeyboardShortcuts);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePriority);

BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill);

BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillUseDomNodeIdForRendererId);

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

// https://crbug.com/1472970
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutoSpeculationRules);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kAutoSpeculationRulesConfig;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kAutoSpeculationRulesHoldback;

// Switch to enabling rendering of gainmap-based AVIF HDR images.
// For this feature to work, kGainmapHdrImages must also be enabled.
// Tracker: https://crbug.com/1451889
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAvifGainmapHdrImages);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBackForwardCacheDWCOnJavaScriptExecution);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheWithKeepaliveRequest);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundResourceFetch);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundTracingPerformanceMark);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

// Debug reporting runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBiddingAndScoringDebugReportingAPI);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapCompaction);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapConcurrentMarking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapConcurrentSweeping);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapIncrementalMarking);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlinkHeapIncrementalMarkingStress);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBlockingDownloadsInAdFrameWithoutUserActivation);

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostImagePriority);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityImageCount;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityImageSize;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBoostImagePriorityTightMediumLimit;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopics);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBrowsingTopicsBypassIPIsPubliclyRoutableCheck);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopicsDocumentAPI);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopicsParameters);
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
    kBrowsingTopicsTaxonomyVersion;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBrowsingTopicsDisabledTopicsList;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBrowsingTopicsPrioritizedTopicsList;
constexpr int kBrowsingTopicsTaxonomyVersionDefault = 2;

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCORSErrorsIssueOnly);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheStorageCodeCacheHintHeader);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheCodeOnIdle);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kCacheCodeOnIdleDelayParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kCacheCodeOnIdleDelayServiceWorkerOnlyParam;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2DHibernation);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvasCompressHibernatedImage);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvasFreeMemoryWhenHidden);

// If enabled, the HTMLDocumentParser will only check its budget after parsing a
// commonly slow token or for one out of 10 fast tokens. Note that this feature
// is a no-op if kTimedHTMLParserBudget is disabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCheckHTMLParserBudgetLessOften);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsFormFactor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kClientHintsPrefersReducedTransparency);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsSaveData);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsXRFormFactor);

// Allows reading unsanitized content from the clipboard. Currently,
// it is only applicable to HTML format. See crbug.com/1268679.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClipboardUnsanitizedContent);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCompressParkableStrings);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxDiskDataAllocatorCapacityMB;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConsumeCodeCacheOffThread);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kContentCaptureConstantStreaming);

// Enable the correction testing for float extension for webgl version 1.
// This is simply a killswitch in case we need to restore original behavior.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCorrectFloatExtensionTestForWebGL);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCreateImageBitmapOrientationNone);

// If enabled, DOMContentLoaded will be fired after all async scripts are
// executed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDOMContentLoadedWaitForAsyncScript);

// If enabled, script source text will be decoded and hashed off the main
// thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDecodeScriptSourceOffThread);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDefaultViewportIsDeviceWidth);

// If enabled, async script execution will be delayed than usual.
// See https://crbug.com/1340837.
//
// As of 2023/11, this experiment enables kLCPCriticalPathPredictor.
// See third_party/blink/common/loader/lcp_critical_path_predictor_util.cc
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelayAsyncScriptExecution);
enum class DelayAsyncScriptDelayType {
  kFinishedParsing,
  kFirstPaintOrFinishedParsing,
  kEachLcpCandidate,
  kEachPaint,
  kTillFirstLcpCandidate,
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
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kDelayAsyncScriptExecutionWhenLcpFoundInHtml;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kDelayAsyncScriptExecutionDelayByDefaultParam;

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

// Enables input IPC to directly target the renderer's compositor thread without
// hopping through the IO thread first.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDirectCompositorThreadIpc);

// TODO(https://crbug.com/1201109): temporary flag to disable new ArrayBuffer
// size limits, so that tests can be written against code receiving these
// buffers. Remove when the bindings code instituting these limits is removed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableArrayBufferSizeLimitsForTesting);

// Kill-switch for a deprecation trial that unpartitions storage in third-party
// contexts under the registered top-level site. If
// `kDisableThirdPartyStoragePartitioningDeprecationTrial` is enabled, the
// deprecation trial information can be sent to and enabled in the browser
// process (i.e. when the base::Feature is enabled, the deprecation trial is
// enabled in the browser process too).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableThirdPartyStoragePartitioningDeprecationTrial);

// These values are used to implement a browser intervention: if a cross-origin
// iframe has moved more than {param:distance} device independent pixels
// (manhattan distance) within its embedding page's viewport within the last
// {param:time_ms} milliseconds, most input events targeting the iframe will be
// quietly discarded.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDiscardInputEventsToRecentlyMovedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDispatchBeforeUnloadOnFreeze);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDisplayLocking);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDropInputEventsBeforeFirstPaint);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDroppedTouchSequenceIncludesTouchEnd);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEagerCacheStorageSetupForServiceWorkers);

// Early exit when the style or class attribute of an element is set to the same
// value as before.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEarlyExitOnNoopClassOrStyleChange);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEditingNG);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnablePenetratingImageSelection);

// Enables establishing the GPU channel asnchronously when requesting a new
// layer tree frame sink.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEstablishGpuChannelAsync);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEventTimingMatchPresentationIndex);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeprecateUnload);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeprecateUnloadByUserAndOrigin);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kDeprecateUnloadPercent;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int> kDeprecateUnloadBucket;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kDeprecateUnloadAllowlist;

// This feature (EventTimingReportAllEarlyEntriesOnPaintedPresentation) is
// having an effect only when EventTimingMatchPresentationIndex is turned on.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEventTimingReportAllEarlyEntriesOnPaintedPresentation);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExcludeLowEntropyImagesFromLCP);
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kMinimumEntropyForLCP;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFramesM120FeaturesPart1);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFramesM120FeaturesPart2);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesCrossOriginAutomaticBeacons);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesReportingAttestationsChanges);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesAutomaticBeaconCredentials);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFileHandlingIcons);

// Switch to temporary turn back on file system url navigation.
// TODO(https://crbug.com/1332598): Remove this feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFileSystemUrlNavigation);
// TODO(https://crbug.com/1360512): this feature creates a carveout for
// enabling filesystem: URL navigation within Chrome Apps regardless of whether
// kFileSystemUrlNavigation is enabled or not.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFileSystemUrlNavigationForChromeAppsOnly);

// Enables filtering of predicted scroll events on compositor thread.
// Uses the kFilterName* values in ui_base_features.h as the 'filter' feature
// param.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFilteringScrollPrediction);

// (b/283408783): When enabled, first gesture scrolls on web pages that have
// touch handlers registered will go through the normal queueing process if
// while gesture scrolls that hit a touch handlers will be queued instantly.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFixGestureScrollQueuingBug);

// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledge);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeBiddingAndAuctionServer);
// Public key URL to use for the default bidding and auction Coordinator.
// Overrides the JSON config for the default coordinator if both are specified.
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kFledgeBiddingAndAuctionKeyURL;
// JSON config specifying supported coordinator origins and their public key
// URLs.
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kFledgeBiddingAndAuctionKeyConfig;
// Configures FLEDGE to consider k-anononymity. If both
// kFledgeConsiderKAnonymity and kFledgeEnforceKAnonymity are on it will be
// enforced; if only kFledgeConsiderKAnonymity is on it will be simulated.
//
// Turning on kFledgeEnforceKAnonymity alone does nothing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeConsiderKAnonymity);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeEnforceKAnonymity);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgePassKAnonStatusToReportWin);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgePassRecencyToGenerateBid);

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

// If enabled, parser-blocking scripts are force-deferred.
// https://crbug.com/1339112
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceDeferScriptIntervention);

// Forces the attribute powerPreference to be set to "high-performance" for
// WebGL contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceHighPerformanceGPUForWebGL);

// If enabled, parser-blocking scripts are loaded asynchronously but the
// execution order is respected. See https://crbug.com/1344772
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceInOrderScript);

// If enabled, the major version number returned by Chrome will be locked at
// 99. The minor version number returned by Chrome will be forced to the
// value of the major version number. The purpose of this
// feature is a back up plan for if the major version moving from
// two to three digits breaks unexpected things.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kForceMajorVersionInMinorPositionInUserAgent);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForLargeStickyAdDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForOverlayPopupDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGMSCoreEmoji);

// Switch to enabling rendering of gainmap-based HDR images.
// Tracker: https://crbug.com/1404000
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGainmapHdrImages);

// Record the bounds of a selection even when there is no selection handle.
// This allows providing more information to the IME, but was disabled because
// of https://crbug.com/1441243.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kHiddenSelectionBounds);

// If enabled, a fix for image loading prioritization based on visibility is
// applied. See https://crbug.com/1369823.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kImageLoadingPrioritizationFix);

// Use Snappy to compress values for IndexedDB before wiring them to the
// browser.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIndexedDBCompressValuesWithSnappy);

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gathering accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInputPredictorTypeChoice);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIntensiveWakeUpThrottling);
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[];

// Backend storage + kill switch for Interest Group API origin trials.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInterestGroupStorage);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOwners;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxStoragePerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxNegativeGroupsPerOwner;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInterestGroupStorageMaxOpsBeforeMaintenance;

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

// Kill-switch for the fetch keepalive request infra migration.
// If enabled, all keepalive requests will be proxied via the browser process.
// Design Doc: https://bit.ly/chromium-keepalive-migration
// Tracker: https://crbug.com/1356128
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKeepAliveInBrowserMigration);

// Enables attribution reporting to be proxied via the browser process using the
// same path as other fetch keepalive requests.
// See https://crbug.com/1374121.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAttributionReportingInBrowserMigration);

// Enables changing the influence of acceleration based on change of direction.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKalmanHeuristics);
// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKalmanDirectionCutOff);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPAnimatedImagesReporting);

// When enabled, LCP critical path predictor will optimize the subsequent visits
// to websites using performance hints collected in the past page loads.
// It enables boosting a loading priority of the possible LCP element.
// TODO(crbug.com/1419756): rename to represent this is for possible LCP entry
// boost.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPCriticalPathPredictor);

// If true, LCP critical path predictor mechanism doesn't change the fetch
// priority but still the rest will work.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPCriticalPathPredictorDryRun;

// The maximum element locator length for LCPP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPCriticalPathPredictorMaxElementLocatorLength;

// The type of LCP elements recorded by LCPP.
enum class LcppRecordedLcpElementTypes {
  kAll,
  kImageOnly,
};

BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppRecordedLcpElementTypes>
    kLCPCriticalPathPredictorRecordedLcpElementTypes;

// TODO(crbug.com/1419756): We should merge this to ResourceLoadPriority.
enum class LcppResourceLoadPriority {
  kMedium,
  kHigh,
  kVeryHigh,
};

// The ResourceLoadPriority for images that are expected to be LCP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppResourceLoadPriority>
    kLCPCriticalPathPredictorImageLoadPriority;

// The ResourceLoadPriority for scripts that are expected to be LCP influencers.
BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppResourceLoadPriority>
    kLCPCriticalPathPredictorInfluencerScriptLoadPriority;

// Enables LCPP ElementLocator performance improvements
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPCriticalPathPredictorEnableElementLocatorPerformanceImprovements;

// If enabled, script execution is observed to determine script dependencies of
// the LCP element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPScriptObserver);

// The maximum URL count for LCPP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPScriptObserverMaxUrlCountPerOrigin;

// The maximum URL length allowed for LCPP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPScriptObserverMaxUrlLength;

// If enabled, fetched font URLs are observed to predict font usage in the
// future navigation.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPFontURLPredictor);

// The maximum URL length for LCPP font URL predictor.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPPFontURLPredictorMaxUrlLength;

// The maximum URL count allowed for LCPP font URL predictor.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPPFontURLPredictorMaxUrlCountPerOrigin;

// Fonts are preloaded if frequencies are above this threshold.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kLCPPFontURLPredictorFrequencyThreshold;

// The maximum number of Fonts to be sent for preload.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPPFontURLPredictorMaxPreloadCount;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPLazyLoadImagePreload);

// The type of preloading for LCP images which are loaded lazily.
// crbug.com/1498777 for more details.
enum class LcppPreloadLazyLoadImageType {
  kNone,
  // Browser-level lazy loading via loading attributes.
  // https://html.spec.whatwg.org/#lazy-loading-attributes
  kNativeLazyLoading,
  // Lazy loading via JavaScript, which is usually provided with the dynamic src
  // URL insersion via IntersectionObserver. Image URLs are typically located
  // in data-src attributes, and this option creates preload requests to that
  // URLs.
  kCustomLazyLoading,
  kAll,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    LcppPreloadLazyLoadImageType>
    kLCPCriticalPathPredictorPreloadLazyLoadImageType;

// Enables prefetch using the LCPP font URL predictor.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPPFontURLPredictorEnablePrefetch;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPVideoFirstFrame);

// Kill-switch for new parsing behaviour of the X-Content-Type-Options header.
// (Should be removed after the new behaviour has been launched.)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLegacyParsingOfXContentTypeOptions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLightweightNoStatePrefetch);

// Enables the Link Preview.
// Tracking bug: go/launch/4269184
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLinkPreview);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLoadingTasksUnfreezable);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyCanvas2dImageChromium);

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
// kLowPriorityAsyncScriptExecution will be excluded for async scripts that
// influence LCP element. Requires the following features enabled as a
// pre-requisite: kLCPCriticalPathPredictor, kLCPScriptObserver and
// kLowPriorityAsyncScriptExecution.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam;
// kLowPriorityAsyncScriptExecution will be disabled when LCP element is
// not detected in Html. Requires kLCPCriticalPathPredictor experiment to be
// enabled for this to work.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam;

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

// If enabled, image loading tasks on visible pages have high priority.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMainThreadHighPriorityImageLoading);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMaxUnthrottledTimeoutNestingLevel);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxUnthrottledTimeoutNestingLevelParam;

// Keep strong references in the blink memory cache to maximize resource reuse.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryCacheStrongReference);
// The threshold for the total decoded size of resources that keep strong
// references.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMemoryCacheStrongReferenceTotalSizeThresholdParam;
// The threshold for the decoded size of a resource that can keep a strong
// reference.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMemoryCacheStrongReferenceResourceSizeThresholdParam;
// Exclude images from the saved strong references for resources.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceFilterImages);
// Exclude scripts from the saved strong references for resources.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceFilterScripts);
// Exclude cross origin scripts from the saved strong references for resources.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceFilterCrossOriginScripts);
// Save only one unloaded page's resources in the memory cache.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kMemoryCacheStrongReferenceSingleUnload);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNavigationPredictor);

// Flag to control whether about:blank and srcdoc iframes use newly proposed
// base url inheritance behavior from https://crbug.com/1356658.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNewBaseUrlInheritanceBehavior);

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoForcedFrameUpdatesForWebTests);

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultEnabled);
// This flag enables a console warning in cases where document.domain is set
// without origin agent clustering being explicitly disabled.
// (This is a transitory behaviour on the road to perma-enabling
// kOriginAgentClusterDefaultEnabled above.)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultWarning);

// Kill-switch for any calls to the mojo interface OriginTrialStateHost
// in the RuntimeFeatureStateOverrideContext class. If
// `kOriginTrialStateHostApplyFeatureDiff` is disabled,
// origin/deprecation trial token information is not sent to the browser
// process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginTrialStateHostApplyFeatureDiff);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPath2DPaintCache);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPaintHolding);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPaintHoldingCrossOrigin);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kParkableImagesToDisk);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPartialLowEndModeExcludeCanvasFontCache;
#endif

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPlzDedicatedWorker);

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

// If enabled, inline scripts will be stream compiled using a background HTML
// scanner.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrecompileInlineScripts);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreferCompositingToLCDText);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// If enabled, font lookup tables will be prefetched on renderer startup.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchFontLookupTables);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchPrivacyChanges);

// If enabled, the machine learning model will be employed to predict the next
// click for speculation-rule based pre-loadings.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadingHeuristicsMLModel);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPreviewsResourceLoadingHintsSpecificResourceTypes);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIs);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApi);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInSharedStorage;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInProtectedAudience;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiProtectedAudienceExtensionsEnabled;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPrivateAggregationApiDebugModeEnabledAtAll;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPrivateAggregationApiMultipleCloudProviders);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessNullIpAddress);

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

// Data producer side for the V8 Crowdsourced Compile hints feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kProduceCompileHints2);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kProduceCompileHintsOnIdleDelayParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kProduceCompileHintsNoiseLevel;
// The proportion of the clients producing data.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kProduceCompileHintsDataProductionLevel;
// For forcing producing compile hints independent of the platform and
// kProduceCompileHintsDataProductionLevel.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceProduceCompileHints);

// Load V8_COMPILE_HINTS optimization data from OptimizationGuide and
// transmit it to V8. See `ProduceCompileHints` for the data producer side of
// this feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConsumeCompileHints);

// Cache information about which functions are compiled and use it for eager-
// compiling those functions when the same script is loaded again.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLocalCompileHints);

// When enabled, gesture scroll updates that hit a JS touch handlers
// will be queued normally on CC, enabling coalescing and consistent
// input handling.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kQueueBlockingGestureScrolls);

// Whether Sec-CH-UA headers on subresource fetches that contain an empty
// string should be quoted (`""`) as they are for navigation fetches. See
// https://crbug.com/1416925.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kQuoteEmptySecChUaStringHeadersConsistently);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRTCOfferExtmapAllowMixed);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRTCGpuCodecSupportWaiter);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kRTCGpuCodecSupportWaiterTimeoutParam;

// A parameter for kReduceUserAgentMinorVersion;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kUserAgentFrozenBuildVersion;

// Parameters for kReduceUserAgentPlatformOsCpu;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kAllExceptLegacyWindowsPlatform;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLegacyWindowsPlatform;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kReducedReferrerGranularity);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kRegisterJSSourceLocationBlockingBFCache);

// If enabled, renderers look for cached resources from another renderer
// that has the same process isolation policies. Note that renderers don't
// use cached resources in other rendereres yet, just record histograms.
// See https://crbug.com/1414262
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRemoteResourceCache);

// Makes preloaded fonts render-blocking up to the limits below.
// See https://crbug.com/1412861
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderBlockingFonts);
// Max milliseconds from navigation start that fonts can block rendering.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxBlockingTimeMsForRenderBlockingFonts;
// Max milliseconds that font are allowed to delay of FCP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxFCPDelayMsForRenderBlockingFonts;

// Report rectangles around lines of text in the currently focused editable
// element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kReportVisibleLineBounds);
// Enables resampling input events on main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingInputEvents);
// Enables resampling GestureScroll events on compositor thread.
// Uses the kPredictorName* values in ui_base_features.h as the 'predictor'
// feature param.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingScrollEvents);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResourceLoadViaDataPipe);

// If enabled, IME updates are computed at the end of a lifecycle update rather
// than the beginning.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRunTextInputUpdatePostLifecycle);

// When enabled, it adds FTP / FTPS / SFTP to the safe list for
// registerProtocolHandler. This feature is enabled by default and meant to
// be used as a killswitch.
// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSafelistFTPToRegisterProtocolHandler);

// When enabled, it adds Payto URI Scheme to the safe list for
// registerProtocolHandler. This feature is disabled by default
// Payto URI Scheme explanation https://datatracker.ietf.org/doc/html/rfc8905
// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSafelistPaytoToRegisterProtocolHandler);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSaveDataImgSrcset);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScopeMemoryCachePerContext);

// When enabled, only pages that belong to a certain browsing context group are
// paused instead of all pages.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPausePagesPerBrowsingContextGroup);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreaming);

// If enabled, parser-blocking scripts are loaded asynchronously. The target
// scripts are selectively applied via the allowlist provided from the feature
// param. See https://crbug.com/1356396
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSelectiveInOrderScript);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSelectiveInOrderScriptTarget);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kSelectiveInOrderScriptAllowList;

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSendCnameAliasesToSubresourceFilterFromRenderer);

// When enabled, the serialization of accessibility information for the browser
// process will be done during LocalFrameView::RunPostLifecycleSteps, rather
// than from a stand-alone task.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSerializeAccessibilityPostLifecycle);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerUpdateDelay);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetLowPriorityForBeacon);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetTimeoutWithoutClamp);

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
// Maximum number of bits of entropy per site per pageload that are allowed to
// leak via `sharedStorage.selectURL()`, if `kSharedStorageSelectURLLimit` is
// enabled.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageSelectURLBitBudgetPerSitePerPageLoad;

// Additional Shared Storage API features shipped in M118.
// TODO(crbug.com/1218540): Merge this flag with `kSharedStorageAPI` once
// shipped.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPIM118);

// TODO(accessibility): This flag is set to accommodate JAWS on Windows so they
// can adjust to us not simulating click events on a focus action. It is in the
// process of being removed completely and is currently disabled by default on
// all platforms. We want to allow users to manually re-enable this behavior for
// the next few months in case their users discover issues they still have to
// fix. It should be removed by 9/17/2023.
//
// See https://crbug.com/1326622 for more info.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSimulateClickOnAXFocus);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSmallScriptStreaming);

// Allows certain origin trials to be enabled using third-party tokens
// associated with the origin of external speculation rules.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSpeculationRulesHeaderEnableThirdPartyOriginTrial);
// Controls whether the SpeculationRulesPrefetchFuture origin trial can be
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculationRulesPrefetchFuture);

// TODO(crbug/1431792): Speculatively warm-up service worker.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerWarmUp);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpDryRun;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpWaitForLoad;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpBatchTimer;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpFirstBatchTimer;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpBatchSize;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpMaxCount;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpRequestQueueLength;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpRequestLimit;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpDuration;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpIntersectionObserver;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpIntersectionObserverDelay;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnVisible;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnInsertedIntoDom;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerover;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerdown;

// Make the browser decide when to turn on the capture indicator (red button)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kStartMediaStreamCaptureIndicatorInBrowser);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStopInBackground);

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
// Whether to renew Storage Access API permission grants after user interaction
// in the relevant contexts.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIRefreshGrantsOnUserInteraction;
// How far back to look when requiring top-level user interaction on the
// requesting site for Storage Access API permission grants. If this value is an
// empty duration (e.g. "0s"), then no top-level user interaction is required.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPITopLevelUserInteractionBound;
// How long a Related Website Sets Storage Access API permission
// grant/denial should last (not taking renewals into account).
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPIRelatedWebsiteSetsLifetime;
// How long an implicit Storage Access API permission grant/denial should last
// (not taking renewals into account).
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPIImplicitPermissionLifetime;
// How long an explicit Storage Access API permission grant/denial should last
// (not taking renewals into account).
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPIExplicitPermissionLifetime;

// Stylus gestures for editable web content.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusRichGestures);
// Stylus handwriting recognition to text input feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusWritingToInput);
// Apply touch adjustment for stylus pointer events. This feature allows
// enabling functions like writing into a nearby input element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusPointerAdjustment);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSystemColorChooser);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTargetBlankImpliesNoOpener);

// Use TextCodecCJK for encoding/decoding CJK except for Big5.
// If the flag is disabled TextCodecICU would be used instead.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTextCodecCJKEnabled);

// Use Gb18030-2022 for encoding/decoding GB18030.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGb18030_2022Enabled);

// If enabled, reads and decodes navigation body data off the main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedBodyLoader);

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedPreloadScanner);

// Forces same-process display:none cross-origin iframes to be throttled in the
// same manner that OOPIFs are.
// Note: this feature should never be accessed directly. Instead, use
// IsThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesEnabled defined
// below.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleForegroundTimers);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleInstallingServiceWorker);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInstallingServiceWorkerOutstandingThrottledLimit;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleUnimportantFrameTimers);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kUnimportantFrameTimersThrottledWakeUpIntervalMills;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLargeFrameSizePercentThreshold;

// If enabled, the HTMLDocumentParser will use a budget based on elapsed time
// rather than token count.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTimedHTMLParserBudget);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUACHOverrideBlank);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEmulateLoadStartedForInspectorOncePerResource);

// Kill switch for using a custom task runner in the blink scheduler that makes
// DeleteSoon/ReleaseSoon less prone to memory leaks.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseBlinkSchedulerTaskRunnerWithCustomDeleter);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseImageInsteadOfStorageForStagingBuffer);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePageViewportInLCP);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSnappyForParkableStrings);

// Causes MediaStreamVideoSource video frames to be transported on a
// SequencedTaskRunner backed by the threadpool instead of the normal IO thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseThreadPoolForMediaStreamVideoTaskRunner);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUserLevelMemoryPressureSignal);

// If enabled, file backed blobs are registered by using the
// FileBackedBlobFactory interface. This interface allows to capture the URL
// from which these blobs are accessed. Access from certain URLs may be disabled
// for managed users according to Data Leak Prevention policies.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableFileBackedBlobFactory);

// Feature flag for driving the Metronome by VSyncs instead of by timer.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kVSyncDecoding);
// Feature parameter controlling WebRTC VSyncDecoding tick durations during
// occluded tabs.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kVSyncDecodingHiddenOccludedTickDuration;

// Feature flag for making use of VideoFrameMetadata::capture_begin_time
// if set, instead of relating incoming media timestamps to local time in the
// WebRTC track source.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseCaptureBeginTimestamp);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppBorderless);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableScopeExtensions);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableUrlHandlers);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppManifestLockScreen);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioSinkSelection);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioBypassOutputBuffering);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebFontsCacheAwareTimeoutAdaption);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebMeasureMemoryViaPerformanceManager);

// Combine WebRTC Network and Worker threads. More info at crbug.com/1373439.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread);
// If enabled, expose non-standard stats in the WebRTC getStats API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcExposeNonStandardStats);
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcHideLocalIpsWithMdns);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace);
// Initialize VideoEncodeAccelerator on the first encode.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcInitializeEncoderOnFirstFrame);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcMetronome);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcMultiplexCodec);
// Feature flag for batching sending of WebRTC RTP UDP packets.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcSendPacketBatch);
// If enabled, the WebRTC_* threads in peerconnection module will use
// kResourceEfficient thread type.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebRtcThreadsUseResourceEfficientType);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseMinMaxVEADimensions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLAccess);
// If enabled, allows the use of WebSQL in non-secure contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLNonSecureContextAccess);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebviewAccelerateSmallCanvases);

// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When declaring feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// ----------------------------------------------------------------------------
// Helper functions for querying feature status. Please declare any features or
// constants for features in the section above.

BLINK_COMMON_EXPORT int GetMaxUnthrottledTimeoutNestingLevel();

// Checks both of kAllowPageWithIDBConnectionInBFCache and
// kAllowPageWithIDBTransactionInBFCache are turned on when determining if a
// page with IndexedDB transaction is eligible for BFCache.
BLINK_COMMON_EXPORT bool
IsAllowPageWithIDBConnectionAndTransactionInBFCacheEnabled();

BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled();

BLINK_COMMON_EXPORT bool IsFencedFramesEnabled();

BLINK_COMMON_EXPORT bool IsMaxUnthrottledTimeoutNestingLevelEnabled();

// This function checks both kNewBaseUrlInheritanceBehavior and
// kIsolateSandboxedIframes and returns true if either is enabled.
BLINK_COMMON_EXPORT bool IsNewBaseUrlInheritanceBehaviorEnabled();

BLINK_COMMON_EXPORT bool IsParkableStringsToDiskEnabled();

BLINK_COMMON_EXPORT bool IsParkableImagesToDiskEnabled();

BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

// Use to determine if iframe throttling is enabled via the feature
// kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes and not disabled
// via enterprise policy.
BLINK_COMMON_EXPORT bool
IsThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesEnabled();

BLINK_COMMON_EXPORT bool ParkableStringsUseSnappy();

// Returns true if the in-browser KeepAliveURLLoaderService should be enabled by
// verifying either kKeepAliveInBrowserMigration or kFetchLaterAPI is true.
// Note that as the service is shared by two different features, code path
// specific to one of them should not rely on this function.
BLINK_COMMON_EXPORT bool IsKeepAliveURLLoaderServiceEnabled();

// Kill-switch for removing Authorization header upon cross origin redirects.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kRemoveAuthroizationOnCrossOriginRedirect);

// Number of pixels to expand in root coordinates for cull rect under
// scroll translation or other composited transform.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExpandCompositedCullRect);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int> kPixelDistanceToExpand;

// Treat HTTP header `Expires: "0"` as expired value according section 5.3 on
// RFC 9111.
// TODO(https://crbug.com/853508): Remove after the bug fix will go well for a
// while on stable channels.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kTreatHTTPExpiresHeaderValueZeroAsExpiredInBlink);

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
