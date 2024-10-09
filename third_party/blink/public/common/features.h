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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdAuctionReportingWithMacroApi);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdAuctionSignals);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kAdAuctionSignalsMaxSizeBytes);

// Runtime flag that changes default Permissions Policy for features
// join-ad-interest-group and run-ad-auction to a more restricted EnableForSelf.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAdInterestGroupAPIRestrictedPolicyByDefault);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAlwaysAllowFledgeDeprecatedRenderURLReplacements);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowerHighResolutionTimerThreshold);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAlignFontDisplayAutoTimeoutWithLCPGoal);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam);
enum class AlignFontDisplayAutoTimeoutWithLCPGoalMode {
  kToFailurePeriod,
  kToSwapPeriod
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    AlignFontDisplayAutoTimeoutWithLCPGoalMode,
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam);

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

// Feature for allowing page into back/forward cache when datapipe has been
// drained as bytes consumer for fetch requests.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowDatapipeDrainedAsBytesConsumerInBFCache);

// If enabled, allows MediaStreamVideoSource objects to be restarted by a
// successful source switch. Normally, switching the source would only allowed
// on streams that are in started state. However, changing the source also first
// stops the stream before performing the switch and sometimes it can be useful
// to do a change directly on a paused stream.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowSourceSwitchOnPausedVideoMediaStream);

// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowURNsInIframes);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisplayWarningDeprecateURNIframesUseFencedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAnchorElementInteraction);

// Extended physical keyboard shortcuts for Android.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidExtendedKeyboardShortcuts);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePriority);

#if BUILDFLAG(IS_APPLE)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePeriodMac);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadPool);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAutofillIncludeFormElementsInShadowDom);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAutofillIncludeShadowDomInUnassociatedListedElements);

BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill);

// https://crbug.com/1472970
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutoSpeculationRules);
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kAutoSpeculationRulesHoldback;

// Switch to enabling rendering of gainmap-based AVIF HDR images.
// Tracker: https://crbug.com/1451889
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAvifGainmapHdrImages);

// When synchronousy loading the initial empty document, perform the layout
// asynchronously.
// TODO(http://crbug.com/324572951): Remove flag after Finch shows that this is
// safe and has positive impact.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAvoidForcedLayoutOnInitialEmptyDocumentInSubframe);

// If enabled, open broadcast channels do not block back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBFCacheOpenBroadcastChannel);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBackForwardCacheDWCOnJavaScriptExecution);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheWithKeepaliveRequest);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundResourceFetch);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBackgroundFontResponseProcessor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBackgroundScriptResponseProcessor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBackgroundCodeCacheDecoderStart);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBakedGamutMapping);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackgroundTracingPerformanceMark);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList;

// Debug reporting runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBiddingAndScoringDebugReportingAPI);

// If enabled, the blink scheduler uses the responsiveness metrics definition of
// discrete input rather than isInputPending's definition.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBlinkSchedulerDiscreteInputMatchesResponsivenessMetrics);

// Finch flag for preventing rendering starvation during threaded scrolling.
// With this feature enabled, the compositor task queue priority remains low
// during compositor gestures, e.g. scrolling, but main thread compositor tasks
// are prioritized if a frame has not been produced recently (a configurable
// duration), until the next BeginMainFrame.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kThreadedScrollPreventRenderingStarvation);

// Block all MIDI access with the MIDI_SYSEX permission
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlockMidiByDefault);

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostImagePriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kBoostImagePriorityImageCount);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kBoostImagePriorityImageSize);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBoostImagePriorityTightMediumLimit);

// Boost the priority of certain loading tasks (https://crbug.com/1470003).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostImageSetLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostFontLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostVideoLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBoostRenderBlockingStyleLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBoostNonRenderBlockingStyleLoadingTaskPriority);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopics);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBrowsingTopicsBypassIPIsPubliclyRoutableCheck);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopicsDocumentAPI);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBrowsingTopicsParameters);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsTimePeriodPerEpoch);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsNumberOfEpochsToExpose);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsNumberOfTopTopicsPerEpoch);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsUseRandomTopicProbabilityPercent);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsMaxEpochIntroductionDelay);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsEpochRetentionDuration);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsMaxEpochPhaseOutTimeOffset);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kBrowsingTopicsTaxonomyVersion);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBrowsingTopicsDisabledTopicsList;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBrowsingTopicsPrioritizedTopicsList;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsFirstTimeoutRetryDelay);
constexpr int kBrowsingTopicsTaxonomyVersionDefault = 2;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheStorageCodeCacheHintHeader);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheCodeOnIdle);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCacheCodeOnIdleDelayParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kCacheCodeOnIdleDelayServiceWorkerOnlyParam);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Enables camera preview in permission bubble and site settings.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCameraMicPreview);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2DHibernation);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kCanvas2DHibernationReleaseTransferMemory);

// Enable the Shared Bitmap to Shared Image conversion for Canvas.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvasSharedBitmapToSharedImage);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCaptureJSExecutionLocation);

// If enabled, the HTMLDocumentParser will only check its budget after parsing a
// commonly slow token or for one out of 10 fast tokens. Note that this feature
// is a no-op if kTimedHTMLParserBudget is disabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCheckHTMLParserBudgetLessOften);

// We do intend to deprecate these when possible, do not remove the feature
// until they can be disabled by default.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth_DEPRECATED);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsXRFormFactor);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCompressParkableStrings);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxDiskDataAllocatorCapacityMB;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLessAggressiveParkableString);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConsumeCodeCacheOffThread);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kContentCaptureConstantStreaming);

// Enable the correction testing for float extension for webgl version 1.
// This is simply a killswitch in case we need to restore original behavior.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCorrectFloatExtensionTestForWebGL);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCreateImageBitmapOrientationNone);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDefaultViewportIsDeviceWidth);

// If enabled, some task queues are disabled between a discrete input event and
// the subsequent frame. Which task types are deferrable depends on the
// `TaskDeferralPolicy`.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeferRendererTasksAfterInput);
enum class TaskDeferralPolicy {
  // A minimal set of task types are deferrable, including DOM Manipulation
  // tasks (for popover) and low priority tasks.
  kMinimalTypes,
  // Existing "deferrable" task types are deferrable, excluding user-blocking
  // web scheduling tasks.
  kNonUserBlockingDeferrableTypes,
  // All per-frame task types are deferrable, excluding user-blocking web
  // scheduling tasks.
  kNonUserBlockingTypes,
  // All existing "deferrable" task types are deferrable.
  kAllDeferrableTypes,
  // All per-frame task types are deferrable.
  kAllTypes,
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(TaskDeferralPolicy,
                                               kTaskDeferralPolicyParam);
// Constants to expose the policy in about:flags.
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputPolicyParamName[];
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputMinimalTypesPolicyName[];
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName[];
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName[];
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName[];
BLINK_COMMON_EXPORT extern const char
    kDeferRendererTasksAfterInputAllTypesPolicyName[];

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    DelayAsyncScriptDelayType,
    kDelayAsyncScriptExecutionDelayParam);
enum class DelayAsyncScriptTarget {
  kAll,
  kCrossSiteOnly,
  // Unlike other options (that are more like scheduling changes within the
  // spec),  kCrossSiteWithAllowList and kCrossSiteWithAllowListReportOnly are
  // used only for the ForceInOrder intervention.
  // TODO(crbug.com/40231912): Remove these values when the ForceInOrder
  // experiment is cleaned up.
  kCrossSiteWithAllowList,
  kCrossSiteWithAllowListReportOnly,
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(DelayAsyncScriptTarget,
                                               kDelayAsyncScriptTargetParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDelayAsyncScriptExecutionDelayLimitParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDelayAsyncScriptExecutionFeatureLimitParam);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kDelayAsyncScriptAllowList;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionMainFrameOnlyParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionWhenLcpFoundInHtml);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionDelayByDefaultParam);
enum class AsyncScriptExperimentalSchedulingTarget {
  kAds,
  kNonAds,
  kBoth,
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    AsyncScriptExperimentalSchedulingTarget,
    kDelayAsyncScriptExecutionTargetParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutLowFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutAutoFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutHighFetchPriorityHintParam);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kHttpRttThreshold);
// The cost reduction for the multiplexed requests when
// `kDelayLowPriorityRequestsAccordingToNetworkState` is enabled.
BLINK_COMMON_EXPORT
extern const base::FeatureParam<double> kCostReductionOfMultiplexedRequests;

// Enables the use of CrabbyAvif for decoding AVIF images.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCrabbyAvif);

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
// `kDisableThirdPartyStoragePartitioningDeprecationTrial2` is enabled, the
// deprecation trial information can be sent to and enabled in the browser
// process (i.e. when the base::Feature is enabled, the deprecation trial
// extension is enabled in the browser process too).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableThirdPartyStoragePartitioningDeprecationTrial2);

// These values are used to implement a browser intervention: if a cross-origin
// iframe has moved more than {param:distance} device independent pixels
// (manhattan distance) within its embedding page's viewport within the last
// {param:time_ms} milliseconds, most input events targeting the iframe will be
// quietly discarded.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDiscardInputEventsToRecentlyMovedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawingBufferWithoutGpuMemoryBuffer);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDropInputEventsBeforeFirstPaint);

// Enables establishing the GPU channel asnchronously when requesting a new
// layer tree frame sink.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEstablishGpuChannelAsync);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeprecateUnload);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeprecateUnloadByAllowList);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kDeprecateUnloadPercent;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int> kDeprecateUnloadBucket;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kDeprecateUnloadAllowlist;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEventTimingHandleOrphanPointerup);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExcludeLowEntropyImagesFromLCP);
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kMinimumEntropyForLCP;

// Controls if the file loaded via the Speculation-Rules header
// is exempt from CSP checks. See crbug.com/371595744 for context.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExemptSpeculationRulesHeaderFromCSP);

// Number of pixels to expand in root layout coordinates for cull rect under
// scroll translation or other composited transform:
//   kCullRectPixelDistanceToExpand *
//   (1 + (device_pixel_ratio - 1) * kCullRectExpansionDPRCoef)
// If kCullRectExpansionDPRCoef equals 0 (the default), the expansion will be
// kCullRectPixelDistanceToExpand in local coordinates.
// If kCullRectExpansionDPRCoef equals 1, the expansion will be
// kCullRectPixelDistanceToExpand in device coordinates.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kExpandCompositedCullRect);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kCullRectPixelDistanceToExpand);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                               kCullRectExpansionDPRCoef);
// If this is enabled, a non-root scroller with an area below a threshold will
// use a minimal cull rect expansion instead of the above expansion.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kSmallScrollersUseMinCullRect);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesCrossOriginEventReportingUnlabeledTraffic);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesCrossOriginEventReportingAllTraffic);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesAutomaticBeaconCredentials);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesLocalUnpartitionedDataAccess);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFramesReportEventHeaderChanges);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kExemptUrlFromNetworkRevocationForTesting);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFramesSrcPermissionsPolicy);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFetchDestinationJsonCssModules);

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
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kFilteringScrollPredictionFilterParam;

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
// Configures FLEDGE to consider k-anonymity. If both
// kFledgeConsiderKAnonymity and kFledgeEnforceKAnonymity are on it will be
// enforced; if only kFledgeConsiderKAnonymity is on it will be simulated.
//
// Turning on kFledgeEnforceKAnonymity alone does nothing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeConsiderKAnonymity);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeEnforceKAnonymity);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgePassKAnonStatusToReportWin);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgePassRecencyToGenerateBid);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeSampleDebugReports);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeSplitTrustedSignalsFetchingURL);

BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFledgeDebugReportLockout;
// Prevent ad techs who accidentally call the API repeatedly for all users,
// from locking themselves out of sending any more debug reports for years.
// This is accomplished by most of the time putting that ad tech in a shorter
// cooldown period, and only some time (e.g., 10% of the time) putting it in a
// restricted cooldown period.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFledgeDebugReportRestrictedCooldown;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFledgeDebugReportShortCooldown;
// Gives a 1/(kFledgeDebugReportSamplingRandomMax+1) chance of allowing sending
// forDebuggingOnly reports.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeDebugReportSamplingRandomMax;
// Gives a 1/(kFledgeDebugReportSamplingRestrictedCooldownRandomMax+1) chance of
// putting an ad tech in a restricted cooldown period.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeDebugReportSamplingRestrictedCooldownRandomMax;
// Sets the time when to enable filtering debug reports. It's the time delta
// since windows epoch. Lockout and cooldown collected before this time will be
// ignored. This avoids locking out ad techs who used forDebuggingOnly API
// before filtering was enabled. Set to zero to disable filtering debug reports.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFledgeEnableFilteringDebugReportStartingFrom;

// If kFledgeCustomMaxAuctionAdComponents is enabled, the limit on number of
// component ads will be taken from `kFledgeCustomMaxAuctionAdComponentsValue`
// (up to kMaxAdAuctionAdComponentsConfigLimit) rather than default.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeCustomMaxAuctionAdComponentsValue;

// If kFledgeNumberBidderWorkletGroupByOriginContextsToKeep is enabled,
// kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue sets the number of
// previously-used group-by-origin contexts to keep in case they can be reused
// in a bidder worklet. Defaulted to 1. A non-default value will only
// be used if kCookieDeprecationFacilitatedTesting is not enabled or if
// kFledgeNumberBidderWorkletContextsIncludeFacilitedTesting is enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeNumberBidderWorkletGroupByOriginContextsToKeep);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kFledgeNumberBidderWorkletContextsIncludeFacilitedTesting;

// Reuse a single V8 context to generate all bids in a bidder worklet.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeAlwaysReuseBidderContext);
// Reuse a single V8 context to score all ads in a seller worklet.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeAlwaysReuseSellerContext);

// Feature params for feature kFledgeRealTimeReporting.
// Epsilon of FLEDGE real time reporting's Rappor noise algorithm.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kFledgeRealTimeReportingEpsilon;
// Total number of buckets supported for FLEDGE real time reporting. Supported
// buckets will be [0, kFledgeRealTimeReportingNumBuckets). Platform
// contribution buckets will start from kFledgeRealTimeReportingNumBuckets.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeRealTimeReportingNumBuckets;
// The priorityWeight of FLEDGE real time reporting's platform contributions.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kFledgeRealTimeReportingPlatformContributionPriority;
// The number of FLEDGE real time reports (`kFledgeRealTimeReportingMaxReports`)
// allowed to be sent per reporting origin per page per
// `kFledgeRealTimeReportingWindow`.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFledgeRealTimeReportingWindow;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFledgeRealTimeReportingMaxReports;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeEnforcePermissionPolicyContributeOnEvent);

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

// Forces the attribute powerPreference to be set to "high-performance" for
// WebGL contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceHighPerformanceGPUForWebGL);

// If enabled, parser-blocking scripts are loaded asynchronously but the
// execution order is respected. See https://crbug.com/1344772
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceInOrderScript);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForLargeStickyAdDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForOverlayPopupDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGMSCoreEmoji);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Defers device selection until after permission is granted.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kGetUserMediaDeferredDeviceSettingsSelection);
#endif
// Record the bounds of a selection even when there is no selection handle.
// This allows providing more information to the IME, but was disabled because
// of https://crbug.com/1441243.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kHiddenSelectionBounds);

// If enabled, a fix for image loading prioritization based on visibility is
// applied. See https://crbug.com/1369823.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kImageLoadingPrioritizationFix);

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gathering accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInputPredictorTypeChoice);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIntensiveWakeUpThrottling);
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[];

// Don't require FCP for the page to turn interactive. Useful for testing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInteractiveDetectorIgnoreFcp);

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

// When enabled, LCP critical path predictor will optimize the subsequent visits
// to websites using performance hints collected in the past page loads.
// It enables boosting a loading priority of the possible LCP element.
// TODO(crbug.com/1419756): rename to represent this is for possible LCP entry
// boost.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPCriticalPathPredictor);

// If false, LCP critical path predictor mechanism doesn't change the fetch
// priority but still the rest will work.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPCriticalPathAdjustImageLoadPriority;

// The maximum element locator length for LCPP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kLCPCriticalPathPredictorMaxElementLocatorLength);

// If true, LCP critical path predictor mechanism overrides the first N image
// prioritization when there is LCP hint.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost;

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

// Enable ResourceLoadPriority changes for all HTMLImageElement loaded images.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement;

// Size of LRU caches for the host data for LCP critical path predictor (LCPP).
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPCriticalPathPredictorMaxHostsToTrack;

// The virtual sliding window size for LCP critical path predictor (LCPP)
// histogram.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPCriticalPathPredictorHistogramSlidingWindowSize;

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPCriticalPathPredictorMaxHistogramBuckets;

// If enabled, script execution is observed to determine script dependencies of
// the LCP element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPScriptObserver);

// The ResourceLoadPriority for scripts that are expected to be LCP influencers.
BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppResourceLoadPriority>
    kLCPScriptObserverScriptLoadPriority;

// The ResourceLoadPriority for images that are expected to LCP elements.
BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppResourceLoadPriority>
    kLCPScriptObserverImageLoadPriority;

// The maximum URL count for LCPP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPScriptObserverMaxUrlCountPerOrigin;

// The maximum URL length allowed for LCPP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPScriptObserverMaxUrlLength;

// Enable ResourceLoadPriority changes for all HTMLImageElement loaded images.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPScriptObserverAdjustImageLoadPriority;

// If enabled, Prerender2 by Speculation Rules API is delayed until
// LCP is finished.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPTimingPredictorPrerender2);

// If enabled, LCP image origin is predicted and preconnected automatically.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPAutoPreconnectLcpOrigin);

// Origins are automatically preconnected if frequencies are above this
// threshold.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kLCPPAutoPreconnectFrequencyThreshold;

// The maximum number of origins to be preconnected
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kkLCPPAutoPreconnectMaxPreconnectOriginsCount;

// If enabled, unused preload requests are deferred to the timing on LCP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPDeferUnusedPreload);

// The resource type which is excluded from the DeferUnusedPreload target.
enum class LcppDeferUnusedPreloadExcludedResourceType {
  kNone,
  kStyleSheet,
  kScript,
  kMock,  // Only for testing.
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    LcppDeferUnusedPreloadExcludedResourceType>
    kLcppDeferUnusedPreloadExcludedResourceType;

// Unused preload requests are deferred if frequencies are above this threshold.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kLCPPDeferUnusedPreloadFrequencyThreshold;

// The type of preload for potentially unused preload resources.
enum class LcppDeferUnusedPreloadPreloadedReason {
  // No limitation. All preload requests, regardless of the developer created or
  // the browser created, can be deferred.
  kAll,
  // Limit the deferred preload to the requests which are made via <link
  // rel="preload">.
  kLinkPreloadOnly,
  // Limit the deferred preload to the requests which are speculatively
  // preloaded via the browse mechanism e.g. preload scanner, LCPP
  // optimizations.
  //
  // This option strictly respects the developer signals by excluding the
  // preloads via <link rel="preload">
  kBrowserSpeculativePreloadOnly,
};

BLINK_COMMON_EXPORT extern const base::FeatureParam<
    LcppDeferUnusedPreloadPreloadedReason>
    kLcppDeferUnusedPreloadPreloadedReason;

// The type of load timing for potentially unused preload resources.
enum class LcppDeferUnusedPreloadTiming {
  // Start loading via PostTask.
  kPostTask,
  // Start loading after the LCPP timing. crbug.com/40285771 for more details.
  kLcpTimingPredictor,
  // LCPTimingPredictor + PostTask.
  kLcpTimingPredictorWithPostTask,
};

BLINK_COMMON_EXPORT extern const base::FeatureParam<
    LcppDeferUnusedPreloadTiming>
    kLcppDeferUnusedPreloadTiming;

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

// Enables prefetch using the LCPP font URL predictor.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kLCPPFontURLPredictorEnablePrefetch;

// Enables prefetch/preload if upper limit bandwidth for the network is
// larger than this value.
// The value <=0 is used for disabling the feature.
BLINK_COMMON_EXPORT extern const base::FeatureParam<double>
    kLCPPFontURLPredictorThresholdInMbps;

// A list of hosts to be excluded from the LCPPFontURLPredictor feature.
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kLCPPFontURLPredictorExcludedHosts;

// Enables cross site font prefetch/preload.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPPCrossSiteFontPredictionAllowed);

// If enabled, LCPP learns with a navigation-initiator origin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPInitiatorOrigin);

// The virtual sliding window size for LCP critical path predictor (LCPP)
// histogram for kLCPPInitiatorOrigin option.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLcppInitiatorOriginHistogramSlidingWindowSize;

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPPInitiatorOrigin option.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLcppInitiatorOriginMaxHistogramBuckets;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPLazyLoadImagePreload);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kLCPPLazyLoadImagePreloadDryRun);

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

// If enabled, some system fonts are preloaded.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadSystemFonts);

BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kPreloadSystemFontsTargets;

BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPreloadSystemFontsRequiredMemoryGB;

// If enabled, LCPP learns with additional first-level-path key to origin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPMultipleKey);

BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLCPPMultipleKeyMaxPathLength;

// The type of LCPP Multiple Key Database.
enum class LcppMultipleKeyTypes {
  kDefault,
  kLcppKeyStat,
};

BLINK_COMMON_EXPORT extern const base::FeatureParam<LcppMultipleKeyTypes>
    kLcppMultipleKeyType;

// The virtual sliding window size for LCP critical path predictor (LCPP)
// histogram for LcppMultipleKeyTypes::kLcppKeyStat option.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLcppMultipleKeyHistogramSlidingWindowSize;

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for LcppMultipleKeyTypes::kLcppKeyStat option.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kLcppMultipleKeyMaxHistogramBuckets;

// If enabled, prewarm HTTP disk cache based on the previous navigation.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kHttpDiskCachePrewarming);

BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kHttpDiskCachePrewarmingMaxUrlLength;

BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kHttpDiskCachePrewarmingHistorySize;

BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kHttpDiskCachePrewarmingReprewarmPeriod;

BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingTriggerOnNavigation;

BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover;

// This feature needs to be used in combination with the
// network::kSimpleURLLoaderUseReadAndDiscardBodyOption feature in order to
// discard the response body efficiently inside the network service.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption;

// If true, avoid prewarming HttpDiskCache during the browser startup.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingSkipDuringBrowserStartup;

// Kill-switch for new parsing behaviour of the X-Content-Type-Options header.
// (Should be removed after the new behaviour has been launched.)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLegacyParsingOfXContentTypeOptions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLightweightNoStatePrefetch);

// Enables the Link Preview.
// Tracking bug: go/launch/4269184
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLinkPreview);

enum class LinkPreviewTriggerType {
  // Alt + left click
  kAltClick,
  // Alt + mousehover
  kAltHover,
  // Long left click down
  kLongPress,
};

BLINK_COMMON_EXPORT extern const base::FeatureParam<LinkPreviewTriggerType>
    kLinkPreviewTriggerType;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLoadingTasksUnfreezable);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyCanvas2dImageChromium);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyWebGLImageChromium);

// If enabled, async scripts will be run on a lower priority task queue.
// See https://crbug.com/1348467.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowPriorityAsyncScriptExecution);
// The timeout value for kLowPriorityAsyncScriptExecution. Async scripts run on
// lower priority queue until this timeout elapsed.
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTimeoutForLowPriorityAsyncScriptExecution;
// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |low_pri_async_exec_feature_limit|. Zero value means no limit.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kLowPriorityAsyncScriptExecutionFeatureLimitParam);
// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam);
// kLowPriorityAsyncScriptExecution will be applied only for main frame's
// scripts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionMainFrameOnlyParam);
// kLowPriorityAsyncScriptExecution will be excluded for async scripts that
// influence LCP element. Requires the following features enabled as a
// pre-requisite: kLCPCriticalPathPredictor, kLCPScriptObserver and
// kLowPriorityAsyncScriptExecution.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam);
// kLowPriorityAsyncScriptExecution will be disabled when LCP element is
// not detected in Html. Requires kLCPCriticalPathPredictor experiment to be
// enabled for this to work.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam);
enum class AsyncScriptPrioritisationType {
  kHigh,
  kLow,
  kBestEffort,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AsyncScriptPrioritisationType>
    kLowPriorityAsyncScriptExecutionLowerTaskPriorityParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AsyncScriptExperimentalSchedulingTarget>
    kLowPriorityAsyncScriptExecutionTargetParam;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionExcludeNonParserInsertedParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionExcludeDocumentWriteParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutLowFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutAutoFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutHighFetchPriorityHintParam);

// If enabled, async scripts will be loaded with a lower fetch priority.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowPriorityScriptLoading);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityScriptLoadingCrossSiteOnlyParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kLowPriorityScriptLoadingFeatureLimitParam);
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kLowPriorityScriptLoadingDenyListParam;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLowPriorityScriptLoadingMainFrameOnlyParam);

// Keep strong references in the blink memory cache to maximize resource reuse.
// See https://crbug.com/1409349.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryCacheStrongReference);
// The threshold for the total decoded size of resources that keep strong
// references.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kMemoryCacheStrongReferenceTotalSizeThresholdParam);
// The threshold for the decoded size of a resource that can keep a strong
// reference.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kMemoryCacheStrongReferenceResourceSizeThresholdParam);
// Whether the ResourceFetcher should store strong references too.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kResourceFetcherStoresStrongReferences);

// Improvements to MHTML for more accurate snapshots.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMHTML_Improvements);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNavigationPredictor);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPredictorTrafficClientEnabledPercent;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kNavigationPredictorNewViewportFeatures);

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoForcedFrameUpdatesForWebTests);

// Don't throttle frames that are same-agent with with a visible frame.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoThrottlingVisibleAgent);

// Optimize loading data: URLs.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOptimizeLoadingDataUrls);

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultEnabled);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPartitionVisitedLinkDatabase);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPartitionVisitedLinkDatabaseWithSelfLinks);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPlzDedicatedWorker);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDedicatedWorkerAblationStudyEnabled);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kDedicatedWorkerStartDelayInMs;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseAncestorRenderFrameForWorker);

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

// If enabled, instantiate Pages and Frames beforehand to speed up SVGImage.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreInitializePageAndFrameForSVGImage);

// The max count of Pages and Frames that will be prepared.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxCountOfPreInitializePageAndFrameForSVGImage;

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
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPreloadingModelTimerStartDelay;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPreloadingModelTimerInterval;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPreloadingModelOneExecutionPerHover;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kPreloadingModelMaxHoverTime;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kPreloadingModelEnactCandidates;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPreloadingModelPrefetchModerateThreshold;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kPreloadingModelPrerenderModerateThreshold;

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

// Enables the prerendering page to perform prepaint document lifecycle updates
// before activation. See https://crbug.com/336963892.
// TODO( https://crbug.com/336963892): Make the expected DocumentLifecycle
// status a feature parameter.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kPrerender2EarlyDocumentLifecycleUpdate);

// Prerender2 support for No-Vary-Search header. Enables prerender matching
// at navigation time using non-exact URL matching based on the prerender
// No-Vary-Search header.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2NoVarySearch);

// Enables to warm up compositor on certain loading event of prerender initial
// navigation. The feature `kWarmUpCompositor` in cc is required to enable this
// feature. Please see crbug.com/41496019 for more details.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositor);
enum class Prerender2WarmUpCompositorTriggerPoint {
  kDidCommitLoad,
  kDidDispatchDOMContentLoadedEvent,
  kDidFinishLoad,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    Prerender2WarmUpCompositorTriggerPoint>
    kPrerender2WarmUpCompositorTriggerPoint;

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
    kPrivateAggregationApiProtectedAudienceAdditionalExtensions);

// If set, HTMLDocumentParser processes data immediately rather than after a
// delay. This is further controlled by the feature params starting with the
// same name. Also note that this only applies to uses that are normally
// deferred (for example, when HTMLDocumentParser is created for inner-html it
// is not deferred).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kProcessHtmlDataImmediately);
// If set, kProcessHtmlDataImmediately impacts child frames. If not set,
// kProcessHtmlDataImmediately does not apply to child frames.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kProcessHtmlDataImmediatelyChildFrame);
// If set, the first chunk of data available for html processing is processed
// immediately.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kProcessHtmlDataImmediatelyFirstChunk);
// If set, kProcessHtmlDataImmediately impacts the main frame. If not set,
// kProcessHtmlDataImmediately does not apply to the main frame.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kProcessHtmlDataImmediatelyMainFrame);
// If set, subsequent chunks of data available for html processing are processed
// immediately.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kProcessHtmlDataImmediatelySubsequentChunks);

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

// Whether Sec-CH-UA headers on subresource fetches that contain an empty
// string should be quoted (`""`) as they are for navigation fetches. See
// https://crbug.com/1416925.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kQuoteEmptySecChUaStringHeadersConsistently);

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

// Kill-switch for removing Authorization header upon cross origin redirects.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kRemoveAuthroizationOnCrossOriginRedirect);

// Makes preloaded fonts render-blocking up to the limits below.
// See https://crbug.com/1412861
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderBlockingFonts);
// Max milliseconds from navigation start that fonts can block rendering.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxBlockingTimeMsForRenderBlockingFonts;
// Max milliseconds that font are allowed to delay of FCP.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxFCPDelayMsForRenderBlockingFonts;

// Enable the optional renderSize field in the browserSignals parameter of
// scoreAd function of Protected Audience API.
// See explainer:
// https://github.com/WICG/turtledove/blob/main/FLEDGE.md#23-scoring-bids
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderSizeInScoreAdBrowserSignals);

// Enables resampling input events on main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingInputEvents);
// Enables resampling GestureScroll events on compositor thread.
// Uses the kPredictorName* values in ui_base_features.h as the 'predictor'
// feature param.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kResamplingScrollEvents);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSafelistPaytoToRegisterProtocolHandler);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSaveDataImgSrcset);

// When enabled, only pages that belong to a certain browsing context group are
// paused instead of all pages.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPausePagesPerBrowsingContextGroup);

// Whether the HUD display is shown for paused pages.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kShowHudDisplayForPausedPages);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreaming);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreamingForNonHTTP);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerUpdateDelay);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerClientIdAlignedWithSpec);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterNotConditionEnabled);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetLowPriorityForBeacon);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetTimeoutWithoutClamp);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPI);

// Maximum number of URLs allowed to be included in the input parameter for
// runURLSelectionOperation().
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit;
// Maximum number of total bytes in database entries at a time that any single
// origin is permitted.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kMaxSharedStorageBytesPerOrigin;
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
// If enabled, sends additional details in the error message for the
// rejected promise when shared storage is disabled, for local troubleshooting
// and use in testing.
//
// NOTE: To preserve user privacy, this feature param MUST remain false by
// default.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSharedStorageExposeDebugMessageForSettingsStatus;

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

// If enabled, the shared storage worklet threads (on the same renderer process)
// will share the same backing thread; otherwise, each will own a dedicated
// backing thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSharedStorageWorkletSharedBackingThreadImplementation);

// Additional Shared Storage API features shipped in M118.
// TODO(crbug.com/1218540): Merge this flag with `kSharedStorageAPI` once
// shipped.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPIM118);

// Additional Shared Storage API features shipped in M125.
// TODO(crbug.com/1218540): Merge this flag with `kSharedStorageAPI` once
// shipped.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPIM125);

// For the Shared Storage API, allows cross-origin script in `addModule()`.
// TODO(crbug.com/40185706): Merge this flag with `kSharedStorageAPI` once
// shipped.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageCrossOriginScript);

// Aligns `createWorklet()`'s default data origin with `addModule()`'s to use
// the invoking context's origin. Also adds the manual `dataOrigin` option to
// that can be passed in the options dictionary for `createWorklet()` to use the
// script's origin as the data origin instead.
// TODO(crbug.com/40185706): Merge this flag with `kSharedStorageAPI` once
// shipped.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSharedStorageCreateWorkletUseContextOriginByDefault);

// Enables WAL (write-ahead-logging) mode for the Shared Storage API SQLite
// database backend.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPIEnableWALForDatabase);

// Optimize loading 1x1 transparent placeholder images.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSimplifyLoadingTransparentPlaceholderImage);

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

// Controls whether the SpeculationRulesPrefetchFuture origin trial can be
// enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculationRulesPrefetchFuture);

// TODO(crbug/1431792): Speculatively warm-up service worker.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerWarmUp);
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpMaxCount;
BLINK_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpDuration;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerover;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerdown;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnIdleTimeout;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStorageSuppressPostTask);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostRenderProcessForLoading);

BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kBoostRenderProcessForLoadingTargetUrls;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBoostRenderProcessForLoadingPrioritizePrerendering);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBoostRenderProcessForLoadingPrioritizePrerenderingOnly);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStopInBackground);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStreamlineRendererInit);

// Subsample a very chatty UKM metric.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSubSampleWindowProxyUsageMetrics);

// Stylus gestures for editable web content.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusRichGestures);
// Apply touch adjustment for stylus pointer events. This feature allows
// enabling functions like writing into a nearby input element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStylusPointerAdjustment);

// If enabled, regex match on script source to detect third party technologies.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThirdPartyScriptDetection);

// If enabled, reads and decodes navigation body data off the main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedBodyLoader);

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedPreloadScanner);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleInstallingServiceWorker);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kInstallingServiceWorkerOutstandingThrottledLimit);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThrottleUnimportantFrameTimers);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kUnimportantFrameTimersThrottledWakeUpIntervalMills);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kLargeFrameSizePercentThreshold);

// If enabled, the HTMLDocumentParser will use a budget based on elapsed time
// rather than token count.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kTimedHTMLParserBudget);

// Treat HTTP header `Expires: "0"` as expired value according section 5.3 on
// RFC 9111.
// TODO(https://crbug.com/853508): Remove after the bug fix will go well for a
// while on stable channels.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kTreatHTTPExpiresHeaderValueZeroAsExpiredInBlink);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUACHOverrideBlank);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEmulateLoadStartedForInspectorOncePerResource);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUnloadBlocklisted);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseImageInsteadOfStorageForStagingBuffer);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePageViewportInLCP);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSnappyForParkableStrings);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseZstdForParkableStrings);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kZstdCompressionLevel);

// Causes MediaStreamVideoSource video frames to be transported on a
// SequencedTaskRunner backed by the threadpool instead of the normal IO thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kUseThreadPoolForMediaStreamVideoTaskRunner);

// If enabled, file backed blobs are registered by using the
// FileBackedBlobFactory interface. This interface allows to capture the URL
// from which these blobs are accessed. Access from certain URLs may be disabled
// for managed users according to Data Leak Prevention policies.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableFileBackedBlobFactory);

// Feature flag for driving decoding with the Metronome by VSyncs instead of by
// timer.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kVSyncDecoding);
// Feature parameter controlling WebRTC VSyncDecoding tick durations during
// occluded tabs.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kVSyncDecodingHiddenOccludedTickDuration);

// Feature flag for driving encoding with the Metronome by VSyncs.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kVSyncEncoding);

// Feature flag for making use of VideoFrameMetadata::capture_begin_time
// if set, instead of relating incoming media timestamps to local time in the
// WebRTC track source.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseCaptureBeginTimestamp);

// Feature to make WebRtcAudioSink use TimestampAligner to align absolute
// capture timestamps. This is disabled by default.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcAudioSinkUseTimestampAligner);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppBorderless);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableScopeExtensions);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableUrlHandlers);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppManifestLockScreen);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioBypassOutputBuffering);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebFontsCacheAwareTimeoutAdaption);

// Combine WebRTC Network and Worker threads. More info at crbug.com/1373439.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread);
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcHideLocalIpsWithMdns);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace);
// If enabled, the WebRTC_* threads in peerconnection module will use
// kResourceEfficient thread type.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebRtcThreadsUseResourceEfficientType);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseMinMaxVEADimensions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebSQLAccess);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebUSBTransferSizeLimit);

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

BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled();

BLINK_COMMON_EXPORT bool IsCanvas2DHibernationEnabled();

BLINK_COMMON_EXPORT bool DisplayWarningDeprecateURNIframesUseFencedFrames();

BLINK_COMMON_EXPORT bool IsFencedFramesEnabled();

BLINK_COMMON_EXPORT bool IsParkableStringsToDiskEnabled();

BLINK_COMMON_EXPORT bool IsParkableImagesToDiskEnabled();

BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT bool IsSetTimeoutWithoutClampEnabled();

// Returns if unload handlers are considered as a blocklisted reason for
// back/forward cache.
BLINK_COMMON_EXPORT bool IsUnloadBlocklisted();

BLINK_COMMON_EXPORT bool ParkableStringsUseSnappy();

// Returns true if the in-browser KeepAliveURLLoaderService should be enabled by
// verifying either kKeepAliveInBrowserMigration or kFetchLaterAPI is true.
// Note that as the service is shared by two different features, code path
// specific to one of them should not rely on this function.
BLINK_COMMON_EXPORT bool IsKeepAliveURLLoaderServiceEnabled();

// Returns true if Link Preview and the given trigger type is enabled.
BLINK_COMMON_EXPORT bool IsLinkPreviewTriggerTypeEnabled(
    LinkPreviewTriggerType type);

BLINK_COMMON_EXPORT bool IsCanvasSharedBitmapConversionEnabled();

// DO NOT ADD NEW FEATURES HERE.
//
// The section above is for helper functions for querying feature status. The
// section below should have nothing. Please add new features in the giant block
// of features that already exist in this file, trying to keep newly-added
// features in sorted order.
//
// DO NOT ADD NEW FEATURES HERE.

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
