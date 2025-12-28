// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include <stdint.h>

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/features_generated.h"

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

// Controls whether to include information about the page's open popup in
// AIPageContent.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAIPageContentIncludePopupWindows);

// Controls whether a missing subframe while generating the APC proto is
// silently dropped. If false, the entire APC is considered failed when this
// happens. When true, the subframe is simply skipped but the rest of APC
// generation is unaffected.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAIPageContentMissingSubframesFailSilently);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdAuctionSignals);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kAdAuctionSignalsMaxSizeBytes);

// Avoids copying ResourceRequest::TrustedParams when possible.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAvoidTrustedParamsCopies);

// Sends touch moves async once the scroll has already started. This means the
// generation of GestureScrollUpdate is not blocked on touch moves being handled
// by RendererMain.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAsyncTouchMovesImmediatelyAfterScroll);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowerHighResolutionTimerThreshold);

// Allows running DevTools main thread debugger even when a renderer process
// hosts multiple main frames.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowDevToolsMainThreadDebuggerForMultipleMainFrames);

// Enables rate obfuscation mitigation in compute pressure, to prevent
// cross-channel attacks.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kComputePressureRateObfuscationMitigation);

// Enables more context data to crash reports reported via the Crash Reporting
// API. See https://crbug.com/400432195.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCrashReportingAPIMoreContextData);

// Enables crash reports to be sent to the `crash-endpoint` group as specified
// in the `Reporting-Endpoints` response header.
// See https://github.com/WICG/crash-reporting/issues/24 for more details.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOverrideCrashReportingEndpoint);

// Feature for allowing page into back/forward cache when datapipe has been
// drained as bytes consumer for fetch requests.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowDatapipeDrainedAsBytesConsumerInBFCache);

// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowURNsInIframes);

#if BUILDFLAG(IS_ANDROID)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidDesktopWebPrefsLargeDisplays);

// If enabled, renders styling similar to Android native UI for spelling and
// grammar errors in textboxes.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidSpellcheckNativeUi);

// If enabled, provides API support for custom spell check menus that are
// rendered by Android applications.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidSpellcheckFullApiBlink);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisplayWarningDeprecateURNIframesUseFencedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePriority);

#if BUILDFLAG(IS_APPLE)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadRealtimePeriodMac);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAudioWorkletThreadPool);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAutofillEnableSyntheticSelectMetricsLogging);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAutofillFixFieldsAssociatedWithNestedFormsByParser);

BLINK_COMMON_EXPORT
BASE_DECLARE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill);

// https://crbug.com/1472970
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kAutoSpeculationRules);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kAutoSpeculationRulesHoldback);

// If enabled, open broadcast channels do not block back/forward cache.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBFCacheOpenBroadcastChannel);

// If enabled, back/forward cache is enabled for pages using shared worker, and
// the page will be evicted from BFCache if it receives a message from the
// shared worker while cached.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBFCacheWithSharedWorker);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBackForwardCacheDWCOnJavaScriptExecution);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackForwardCachePauseMicrotasks);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kBackgroundTracingPerformanceMark_AllowList);

// Block all MIDI access with the MIDI_SYSEX permission
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlockMidiByDefault);

// Boost the priority of certain loading tasks (https://crbug.com/1470003).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostImageSetLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostFontLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostVideoLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBoostRenderBlockingStyleLoadingTaskPriority);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kBoostNonRenderBlockingStyleLoadingTaskPriority);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kBrowsingTopicsDisabledTopicsList);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kBrowsingTopicsPrioritizedTopicsList);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kBrowsingTopicsFirstTimeoutRetryDelay);
constexpr int kBrowsingTopicsTaxonomyVersionDefault = 2;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCacheStorageCodeCacheHintHeader);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kCacheStorageCodeCacheHintHeaderName);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2DHibernation);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kCanvas2DHibernationReleaseTransferMemory);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanvas2DHibernationNoSmallCanvas);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCaptureJSExecutionLocation);

// If enabled, the Clear-Site-Data header will handle "prefetchCache" and
// "prerenderCache" to clear the Prefetch and Prerender caches respectively.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClearSiteDataPrefetchPrerenderCache);

// Fix for CSS font comparison logic.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCSSFontComparisonFix);

// We do intend to deprecate these when possible, do not remove the feature
// until they can be disabled by default.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDeviceMemory_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsDPR_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsResourceWidth_DEPRECATED);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kClientHintsViewportWidth_DEPRECATED);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCompressParkableStrings);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kMaxDiskDataAllocatorCapacityMB);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLessAggressiveParkableString);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCombineNewWindowIPCs);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kConsumeCodeCacheOffThread);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kContentCaptureConstantStreaming);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kCreateImageBitmapOrientationNone);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDeclarativeCSSModulesUseDataURI);

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
  kTillFirstLcpCandidate,
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    DelayAsyncScriptDelayType,
    kDelayAsyncScriptExecutionDelayParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionCrossSiteOnlyParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDelayAsyncScriptExecutionDelayLimitParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDelayAsyncScriptExecutionFeatureLimitParam);
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
    kDelayAsyncExecExcludeNonParserInsertedParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncExecExcludeDocumentWriteParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutLowFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutAutoFetchPriorityHintParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDelayAsyncScriptExecutionOptOutHighFetchPriorityHintParam);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDelayLayerTreeViewDeletionOnLocalSwap);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam);

// Improves the signal-to-noise ratio of network error related messages in the
// DevTools Console.
// See http://crbug.com/40788570.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDevToolsImprovedNetworkError);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDevToolsAllowPopoverForcing);

// Enables input IPC to directly target the renderer's compositor thread without
// hopping through the IO thread first.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDirectCompositorThreadIpc);

// TODO(https://crbug.com/1201109): temporary flag to disable new ArrayBuffer
// size limits, so that tests can be written against code receiving these
// buffers. Remove when the bindings code instituting these limits is removed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDisableArrayBufferSizeLimitsForTesting);

// These values are used to implement a browser intervention: if a cross-origin
// iframe has moved more than {param:distance} device independent pixels
// (manhattan distance) within its embedding page's viewport within the last
// {param:time_ms} milliseconds, most input events targeting the iframe will be
// quietly discarded.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kDiscardInputEventsToRecentlyMovedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDropInputEventsWhilePaintHolding);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEnableDevtoolsDeepLinkViaExtensibilityApi);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnforceNoopenerOnBlobURLNavigation);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableLazyLoadImageForInvisiblePage);
enum class EnableLazyLoadImageForInvisiblePageType {
  kAllInvisiblePage,
  kPrerenderPage,
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    EnableLazyLoadImageForInvisiblePageType,
    kEnableLazyLoadImageForInvisiblePageTypeParam);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource);

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
// Parameter for ChangedEnoughMinimumDistance() in cull_rect.cc.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kCullRectChangedEnoughDistance);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFadeInScrollbarWhenMouseWheelMayBegin);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFencedFrames);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesCrossOriginEventReporting);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesAutomaticBeaconCredentials);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFencedFramesCrossOriginAutomaticBeaconData);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kFilteringScrollPredictionFilterParam);

// FLEDGE ad serving runtime flag/JS API.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledge);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeBiddingAndAuctionServer);
// Public key URL to use for the default bidding and auction Coordinator.
// Overrides the JSON config for the default coordinator if both are specified.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string,
                                               kFledgeBiddingAndAuctionKeyURL);
// JSON config specifying supported coordinator origins and their public key
// URLs.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kFledgeBiddingAndAuctionKeyConfig);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeOriginScopedKeys);
// JSON config specifying supported coordinator origins and their public key
// URLs.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string,
                                               kFledgeOriginScopedKeyConfig);

// Configures FLEDGE to consider k-anonymity. If both
// kFledgeConsiderKAnonymity and kFledgeEnforceKAnonymity are on it will be
// enforced; if only kFledgeConsiderKAnonymity is on it will be simulated.
//
// Turning on kFledgeEnforceKAnonymity alone does nothing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeConsiderKAnonymity);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeEnforceKAnonymity);

// Configures a limit on the number of `selectableBuyerAndSellerReportingIds`
// per ad. This is implemented with two parameters, a hard limit and a soft
// limit, intended to ensure that interest groups joined or updated before a
// reduction in the limit don't immediately become unusable. The hard limit is
// enforced both when an interest group is joined or updated, and also anytime
// the interest group is deserialized. The soft limit is only enforced when an
// interest group is joined or updated. To execute a reduction in the limit, the
// soft limit would first be changed to reflect the new value, and only after
// the "maximum interest group TTL" amount of time has passed, the hard limit
// would be lowered to match. Except during such a reduction, the hard limit and
// soft limits should always match. Increases in the limit can be applied to
// both the hard and soft limits simultaneously. The hard limit should always be
// greater than or equal to the soft limit. For each parameter, a negative value
// indicates that that no limit should be enforced.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeLimitSelectableBuyerAndSellerReportingIds);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsSoftLimit);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsHardLimit);

// Controls the max interest group lifetime in milliseconds. If
// kFledgeMaxGroupLifetimeFeature is enabled, then the max interest group
// lifetime will be kFledgeMaxGroupLifetimeFeature; otherwise, it will be 30
// days.
//
// *WARNING*: Some UMA histograms assume a particular max lifetime for the
// purposes of choosing appropriate bucketing, such as
// Ads.InterestGroup.Auction.GroupFreshness*. As bucketing should not be changed
// for an existing metric, consider adding a new metric with new bucketing if
// increasing this value.
//
// NOTE: This value is only enforced when interest groups are joined -- the
// lifetimes of groups already in the database will not be affected.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeMaxGroupLifetimeFeature);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kFledgeMaxGroupLifetime);
// The join/bid/win metadata storage max history length is controlled by a
// separate feature param. This is because metadata is included in B&A requests
// sent to servers, so increasing the length of metadata history sent increases
// the number of bytes sent over the network, necessitating special care.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kFledgeMaxGroupLifetimeForMetadata);

// Decide whether to enable forDebuggingOnly report sampling based on user's
// third party cookie setting.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeEnableSampleDebugReportOnCookieSetting);

// Run sampling of forDebuggingOnly reports and let generateBid() and scoreAd()
// know fDO's lockout/cooldown status through their browser signals, to allow
// ad techs experimenting with and adapting to the algorithm.
// But whether sending all or only sampled forDebuggingOnly reports depends on
// flag kFledgeEnableFilteringDebugReportStartingFrom.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeSampleDebugReports);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kFledgeDebugReportLockout);
// Prevent ad techs who accidentally call the API repeatedly for all users,
// from locking themselves out of sending any more debug reports for years.
// This is accomplished by most of the time putting that ad tech in a shorter
// cooldown period, and only some time (e.g., 10% of the time) putting it in a
// restricted cooldown period.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kFledgeDebugReportRestrictedCooldown);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kFledgeDebugReportShortCooldown);
// Gives a 1/(kFledgeDebugReportSamplingRandomMax+1) chance of allowing sending
// forDebuggingOnly reports.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeDebugReportSamplingRandomMax);
// Gives a 1/(kFledgeDebugReportSamplingRestrictedCooldownRandomMax+1) chance of
// putting an ad tech in a restricted cooldown period.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeDebugReportSamplingRestrictedCooldownRandomMax);
// Sets the time when to enable filtering debug reports. It's the time delta
// since windows epoch. Lockout and cooldown collected before this time will be
// ignored. This avoids locking out ad techs who used forDebuggingOnly API
// before filtering was enabled. Set to zero to disable filtering debug reports.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kFledgeEnableFilteringDebugReportStartingFrom);

// If kFledgeCustomMaxAuctionAdComponents is enabled, the limit on number of
// component ads will be taken from `kFledgeCustomMaxAuctionAdComponentsValue`
// (up to kMaxAdAuctionAdComponentsConfigLimit) rather than default.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeCustomMaxAuctionAdComponentsValue);

// Feature params for feature kFledgeRealTimeReporting.
// Epsilon of FLEDGE real time reporting's Rappor noise algorithm.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                               kFledgeRealTimeReportingEpsilon);
// Total number of buckets supported for FLEDGE real time reporting. Supported
// buckets will be [0, kFledgeRealTimeReportingNumBuckets). Platform
// contribution buckets will start from kFledgeRealTimeReportingNumBuckets.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeRealTimeReportingNumBuckets);
// The priorityWeight of FLEDGE real time reporting's platform contributions.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kFledgeRealTimeReportingPlatformContributionPriority);
// The number of FLEDGE real time reports (`kFledgeRealTimeReportingMaxReports`)
// allowed to be sent per reporting origin per page per
// `kFledgeRealTimeReportingWindow`.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kFledgeRealTimeReportingWindow);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeRealTimeReportingMaxReports);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeEnforcePermissionPolicyContributeOnEvent);

// Feature flag to disable locally hosted ad auction.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFledgeDisableLocalAdsAuctions);

// Feature flag to limit on the number of
// `selectableBuyerAndSellerReportingIds` for which the browser fetches k-anon
// keys.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit);

// Feature flag to truncate the set of `selectableBuyerAndSellerReportingIds`
// to only those for which k-anon status was fetched, as limited by the
// kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit parameter
// defined above.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFledgeTruncateSelectableBuyerAndSellerReportingIdsToKAnonLimit);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceWebContentsDarkMode);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kForceDarkForegroundLightnessThresholdParam);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kForceDarkBackgroundLightnessThresholdParam);

// Forces the attribute powerPreference to be set to "high-performance" for
// WebGL contexts.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceHighPerformanceGPUForWebGL);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceOffTextAutosizing);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kFrameMetadataObserver);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForLargeStickyAdDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kFrequencyCappingForOverlayPopupDetection);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kGMSCoreEmoji);

// If enabled, then display audio track permission failures are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kGetDisplayMediaIgnoreAudioPermissionFailures);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kHTMLParserYieldEventNameForPause);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kHTMLParserYieldEventNameForResume);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                               kHTMLParserYieldTimeoutInMs);

// When enabled all input arriving will be ignored, and the dispatcher will be
// notified that the event was not consumed. With the exception of when there
// is an attached Dev Tools session, during which input will be dispatched even
// if we are hidden.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIgnoreInputWhileHidden);

// If enabled, a fix for image loading prioritization based on visibility is
// applied. See https://crbug.com/1369823.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kImageLoadingPrioritizationFix);

#if !BUILDFLAG(IS_ANDROID)
// If enabled, the initial WebUI will not interact with extensions. This feature
// intends to optimize performance by reducting extension related tasks.
// See crbug.com/450192387.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kInitialWebUIWithoutExtensions);
#endif  // !BUILDFLAG(IS_ANDROID)

// Use Snappy to compress values for IndexedDB before wiring them to the
// browser.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kIndexedDBCompressValuesWithSnappy);
// Defines the minimum uncompressed size to merit a compression attempt.
// Values less than 0 will use the minimum threshold for value blob-wrapping.
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kIndexedDBCompressValuesWithSnappyCompressionThreshold;

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    IsolateSandboxedIframesGrouping,
    kIsolateSandboxedIframesGroupingParam);

#if BUILDFLAG(ENABLE_JXL_DECODER)
// Flag to enable JXL (JPEG XL) image format support.
// If disabled, JXL images will not be decoded even if the decoder is built.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kJXLImageFormat);
#endif

// Kill-switch for the fetch keepalive request infra migration.
// If enabled, all keepalive requests will be proxied via the browser process.
// Design Doc: https://bit.ly/chromium-keepalive-migration
// Tracker: https://crbug.com/1356128
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kKeepAliveInBrowserMigration);

// When enabled, PaintArtifactCompositor::Layerizer will limit the distance
// (in number of non-mergeable intermediate PendingLayers) between merged
// layers to kLayerMergeDistanceLimit, to reduce the cost of layerization.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLimitLayerMergeDistance);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                               kLayerMergeDistanceLimit);

// When enabled, LCP critical path predictor will optimize the subsequent visits
// to websites using performance hints collected in the past page loads.
// It enables boosting a loading priority of the possible LCP element.
// TODO(crbug.com/1419756): rename to represent this is for possible LCP entry
// boost.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPCriticalPathPredictor);

// If false, LCP critical path predictor mechanism doesn't change the fetch
// priority but still the rest will work.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPCriticalPathAdjustImageLoadPriority);

// The maximum element locator length for LCPP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kLCPCriticalPathPredictorMaxElementLocatorLength);

// If true, LCP critical path predictor mechanism overrides the first N image
// prioritization when there is LCP hint.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost);

// The value must be between 0 and 1 inclusive. The prediction that is below
// this threshold will be ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kLcppAdjustImageLoadPriorityConfidenceThreshold);

// The type of LCP elements recorded by LCPP.
enum class LcppRecordedLcpElementTypes {
  kAll,
  kImageOnly,
};

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppRecordedLcpElementTypes,
    kLCPCriticalPathPredictorRecordedLcpElementTypes);

// TODO(crbug.com/1419756): We should merge this to ResourceLoadPriority.
enum class LcppResourceLoadPriority {
  kMedium,
  kHigh,
  kVeryHigh,
};

// The ResourceLoadPriority for images that are expected to be LCP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppResourceLoadPriority,
    kLCPCriticalPathPredictorImageLoadPriority);

// Enable ResourceLoadPriority changes for all HTMLImageElement loaded images.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement);

// Size of LRU caches for the host data for LCP critical path predictor (LCPP).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPCriticalPathPredictorMaxHostsToTrack);

// The virtual sliding window size for LCP critical path predictor (LCPP).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPCriticalPathPredictorSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPCriticalPathPredictorMaxHistogramBuckets);

// If enabled, script execution is observed to determine script dependencies of
// the LCP element.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPScriptObserver);

// The ResourceLoadPriority for scripts that are expected to be LCP influencers.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppResourceLoadPriority,
    kLCPScriptObserverScriptLoadPriority);

// The ResourceLoadPriority for images that are expected to LCP elements.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppResourceLoadPriority,
    kLCPScriptObserverImageLoadPriority);

// The maximum URL count for LCPP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kLCPScriptObserverMaxUrlCountPerOrigin);

// The maximum URL length allowed for LCPP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                               kLCPScriptObserverMaxUrlLength);

// Enable ResourceLoadPriority changes for all HTMLImageElement loaded images.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPScriptObserverAdjustImageLoadPriority);

// The virtual sliding window size for kLCPScriptObserver.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPScriptObserverSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPScriptObserver.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPScriptObserverMaxHistogramBuckets);

// If enabled, Prerender2 by Speculation Rules API is delayed until
// LCP is finished.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPTimingPredictorPrerender2);

// The virtual sliding window size for kLCPTimingPredictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPTimingPredictorSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPTimingPredictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPTimingPredictorMaxHistogramBuckets);

// If enabled, LCP image origin is predicted and preconnected automatically.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPAutoPreconnectLcpOrigin);

// Origins are automatically preconnected if frequencies are above this
// threshold.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kLCPPAutoPreconnectFrequencyThreshold);

// The maximum number of origins to be preconnected
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kkLCPPAutoPreconnectMaxPreconnectOriginsCount);

// The virtual sliding window size for kLCPPAutoPreconnectLcpOrigin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPAutoPreconnectSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPPAutoPreconnectLcpOrigin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPAutoPreconnectMaxHistogramBuckets);

// If enabled, kLCPPAutoPreconnectLcpOrigin stores all origins instead of the
// final LCP image element's.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPPAutoPreconnectRecordAllOrigins);

// If enabled, unused preload requests are deferred to the timing on LCP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPDeferUnusedPreload);

// The resource type which is excluded from the DeferUnusedPreload target.
enum class LcppDeferUnusedPreloadExcludedResourceType {
  kNone,
  kStyleSheet,
  kScript,
  kMock,  // Only for testing.
};
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppDeferUnusedPreloadExcludedResourceType,
    kLcppDeferUnusedPreloadExcludedResourceType);

// Unused preload requests are deferred if frequencies are above this threshold.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kLCPPDeferUnusedPreloadFrequencyThreshold);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppDeferUnusedPreloadPreloadedReason,
    kLcppDeferUnusedPreloadPreloadedReason);

// The type of load timing for potentially unused preload resources.
enum class LcppDeferUnusedPreloadTiming {
  // Start loading via PostTask.
  kPostTask,
  // Start loading after the LCPP timing. crbug.com/40285771 for more details.
  kLcpTimingPredictor,
  // LCPTimingPredictor + PostTask.
  kLcpTimingPredictorWithPostTask,
};

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(LcppDeferUnusedPreloadTiming,
                                               kLcppDeferUnusedPreloadTiming);

// The virtual sliding window size for kLCPPDeferUnusedPreload.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPDeferUnusedPreloadSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPPDeferUnusedPreload.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPDeferUnusedPreloadMaxHistogramBuckets);

// If enabled, fetched font URLs are observed to predict font usage in the
// future navigation.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPFontURLPredictor);

// The maximum URL length for LCPP font URL predictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kLCPPFontURLPredictorMaxUrlLength);

// The maximum URL count allowed for LCPP font URL predictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kLCPPFontURLPredictorMaxUrlCountPerOrigin);

// Fonts are preloaded if frequencies are above this threshold.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kLCPPFontURLPredictorFrequencyThreshold);

// The maximum number of Fonts to be sent for preload.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPFontURLPredictorMaxPreloadCount);

// Enables prefetch using the LCPP font URL predictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPPFontURLPredictorEnablePrefetch);

// Enables prefetch/preload if upper limit bandwidth for the network is
// larger than this value.
// The value <=0 is used for disabling the feature.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kLCPPFontURLPredictorThresholdInMbps);

// A list of hosts to be excluded from the LCPPFontURLPredictor feature.
// Note: declared without BASE_DECLARE_FEATURE_PARAM because the production code
// gets this value only once.
// Note: declared without BASE_DECLARE_FEATURE_PARAM because the production code
// gets this value only once to construct static local instance.
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kLCPPFontURLPredictorExcludedHosts;

// Enables cross site font prefetch/preload.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kLCPPCrossSiteFontPredictionAllowed);

// The virtual sliding window size for kLCPPFontURLPredictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPFontURLPredictorSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPPFontURLPredictor.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLCPPFontURLPredictorMaxHistogramBuckets);

// If enabled, LCPP learns with a navigation-initiator origin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPInitiatorOrigin);

// The virtual sliding window size for LCP critical path predictor (LCPP)
// histogram for kLCPPInitiatorOrigin option.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLcppInitiatorOriginHistogramSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPPInitiatorOrigin option.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLcppInitiatorOriginMaxHistogramBuckets);

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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    LcppPreloadLazyLoadImageType,
    kLCPCriticalPathPredictorPreloadLazyLoadImageType);

// If enabled, some system fonts are preloaded.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadSystemFonts);

// Note: Declared without BASE_DECLARE_FEATURE_PARAM because the production code
// gets this value only once.
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kPreloadSystemFontsTargets;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kPreloadSystemFontsRequiredMemoryGB);

// If enabled, LCPP learns with additional first-level-path key to origin.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPMultipleKey);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                               kLCPPMultipleKeyMaxPathLength);

// The type of LCPP Multiple Key Database.
enum class LcppMultipleKeyTypes {
  kDefault,
  kLcppKeyStat,
};

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(LcppMultipleKeyTypes,
                                               kLcppMultipleKeyType);

// The virtual sliding window size for LCP critical path predictor (LCPP)
// histogram for LcppMultipleKeyTypes::kLcppKeyStat option.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLcppMultipleKeyHistogramSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for LcppMultipleKeyTypes::kLcppKeyStat option.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kLcppMultipleKeyMaxHistogramBuckets);

// If enabled, LCPP prefetches the subresources based on LCP prewarmed HTTP disk
// cache data.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPPrefetchSubresource);

// If enabled, doing prefetch task async after main resource fetching.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLCPPPrefetchSubresourceAsync);

// If enabled, prewarm HTTP disk cache based on the previous navigation.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kHttpDiskCachePrewarming);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kHttpDiskCachePrewarmingMaxUrlLength);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kHttpDiskCachePrewarmingHistorySize);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kHttpDiskCachePrewarmingReprewarmPeriod);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingTriggerOnNavigation);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover);

// This feature needs to be used in combination with the
// network::kSimpleURLLoaderUseReadAndDiscardBodyOption feature in order to
// discard the response body efficiently inside the network service.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption);

// If true, avoid prewarming HttpDiskCache during the browser startup.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingSkipDuringBrowserStartup);

// The virtual sliding window size for kLCPScriptObserver.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kHttpDiskCachePrewarmingSlidingWindowSize);

// The max histogram bucket count that can be stored in the LCP critical path
// predictor (LCPP) database for kLCPScriptObserver.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kHttpDiskCachePrewarmingMaxHistogramBuckets);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(LinkPreviewTriggerType,
                                               kLinkPreviewTriggerType);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLoadingTasksUnfreezable);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyCanvas2dImageChromium);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowLatencyWebGLImageChromium);

// If enabled, async scripts will be run on a lower priority task queue.
// See https://crbug.com/1348467.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLowPriorityAsyncScriptExecution);
// The minimum amount of the physical memory (GB) to use
// kLowPriorityAsyncScriptExecution.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kMinimumPhysicalMemoryForLowPriorityAsyncScriptExecution);
// The timeout value for kLowPriorityAsyncScriptExecution. Async scripts run on
// lower priority queue until this timeout elapsed.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kTimeoutForLowPriorityAsyncScriptExecution);
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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    AsyncScriptExperimentalSchedulingTarget,
    kLowPriorityAsyncScriptExecutionTargetParam);
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

// A feature to enable the new value-based, multi-tiered pruning strategy
// for the MemoryCache's strong references.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryCacheIntelligentPruning);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kMemoryCacheIntelligentPruningFreqWeight);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kMemoryCacheIntelligentPruningCostWeight);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kMemoryCacheIntelligentPruningTypeWeight);

// Enables extending the list of resource types for strong references
// in MemoryCache via boolean FeatureParams.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryCacheStrongReferenceExtensions);

// --- High Priority ---
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kMemoryCacheStrongRefXSLStyleSheet);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kMemoryCacheStrongRefRaw);

// --- Medium Priority ---
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefImage);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kMemoryCacheStrongRefSVGDocument);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefManifest);

// --- Low Priority ---
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefAudio);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefVideo);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefTextTrack);

// --- Lowest Priority ---
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kMemoryCacheStrongRefLinkPrefetch);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kMemoryCacheStrongRefSpeculationRules);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kMemoryCacheStrongRefDictionary);

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

// Purge memory when a frame is frozen in a renderer. See
// `kMemoryPurgeOnFreezeLimit` to do this only once per backgrounded session.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryPurgeOnFreeze);

// Limits the number of memory purges on page freezing to 1 per background
// session. Without this, memory purge is performed every time a page becomes
// frozen, which can be too much with periodic freezing/unfreezing.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemoryPurgeOnFreezeLimit);

// Enables v8 memory saver mode on low memory thresholds.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMemorySaverModeRenderTuning);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kAvailableMemoryThresholdParamMb);

// Improvements to MHTML for more accurate snapshots.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMHTML_Improvements);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kMixedContentAutoupgrade);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNavigationPredictor);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kPredictorTrafficClientEnabledPercent);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kNavigationPredictorNewViewportFeatures);

// Disables forced frame updates for web tests. Used by web test runner only.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoForcedFrameUpdatesForWebTests);

// Don't throttle frames that are same-agent with with a visible frame.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoThrottlingVisibleAgent);

// Don't throw exception from worker constructor when the worker url is blocked
// by CSP, fire error event asynchronously instead.
// See https://crbug.com/41285169.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoThrowForCSPBlockedWorker);

// Fix for https://crbug.com/40927333.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOpenAllUrlsOrFilesOnDrop);

// Optimize URL copies and resolutions by reducing unnecessary URL construction,
// and caching the URLs in Document.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOptimizeHTMLElementUrls);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kDocumentURLCacheSize);

// If enabled, an absent Origin-Agent-Cluster: header is interpreted as
// requesting an origin agent cluster, but in the same process.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kOriginAgentClusterDefaultEnabled);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPath2DPaintCache);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPaintHolding);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kPartialLowEndModeExcludeCanvasFontCache);
#endif

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kDedicatedWorkerAblationStudyEnabled);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kDedicatedWorkerStartDelayInMs);

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

// If enabled, inline scripts will be stream compiled using a background HTML
// scanner.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrecompileInlineScripts);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreferCompositingToLCDText);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// If enabled, font lookup tables will be prefetched on renderer startup.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrefetchFontLookupTables);
#endif

// If enabled, "eager" eagerness will use different heuristics than "immediate"
// eagerness:
// * Hover heuristics (for Desktop), it will use a short hover delay.
// * Viewport heuristics (for Mobile), it will use the presence of a link in the
// viewport (with no restrictions of the sort used by
// kPreloadingModerateViewportHeuristics).
//
// Both are less eager than the "immediate" behavior and more eager than the
// "moderate" behavior on the respective platforms.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadingEagerHoverHeuristics);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kPreloadingEagerHoverHeuristicsDwellTime);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadingEagerViewportHeuristics);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kPreloadingEagerViewportHeuristicsPresentTime);

// If enabled, the machine learning model will be employed to predict the next
// click for speculation-rule based pre-loadings.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadingHeuristicsMLModel);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kPreloadingModelTimerStartDelay);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                               kPreloadingModelTimerInterval);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                               kPreloadingModelMaxHoverTime);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                               kPreloadingModelEnactCandidates);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kPreloadingModelPrefetchModerateThreshold);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kPreloadingModelPrerenderModerateThreshold);

// If enabled, a viewport based heuristic will be used to predict the next click
// for speculation-rule based preloading.
// Note: To work correctly, this also needs kNavigationPredictor enabled with
// "random_anchor_sampling_period" set to 1, and
// kNavigationPredictorNewViewportFeatures.
// Note: The prediction will only be preloaded if the "enact_candidates" param
// is set to true (false by default), otherwise it is only logged for metrics
// purposes.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPreloadingModerateViewportHeuristics);

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

// Firing pagehide events for intended prerender cancellation. See
// crbug.com/353628449 for more details.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPageHideEventForPrerender2);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApi);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kPrivateAggregationApiEnabledInSharedStorage);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kPrivateAggregationApiEnabledInProtectedAudience);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kPrivateAggregationApiDebugModeEnabledAtAll);
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
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                               kProduceCompileHintsNoiseLevel);
// The proportion of the clients producing data.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kProduceCompileHintsDataProductionLevel);
// For forcing producing compile hints independent of the platform and
// kProduceCompileHintsDataProductionLevel.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kForceProduceCompileHints);

// Cache information about which functions are compiled and use it for eager-
// compiling those functions when the same script is loaded again.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kLocalCompileHints);

// Whether Sec-CH-UA headers on subresource fetches that contain an empty
// string should be quoted (`""`) as they are for navigation fetches. See
// https://crbug.com/1416925.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kQuoteEmptySecChUaStringHeadersConsistently);

// A parameter for kReduceUserAgentMinorVersion;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string,
                                               kUserAgentFrozenBuildVersion);

// Parameters for kReduceUserAgentPlatformOsCpu;
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kReducedReferrerGranularity);

// Refactor CompositorThreadEventQueue to separate event queuing and coalescing.
// When disabled, CompositorThreadEventQueue coalesces input events in
// CompositorThreadEventQueue::Queue itself.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRefactorCompositorThreadEventQueue);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kReleaseResourceDecodedDataOnMemoryPressure);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kReleaseResourceStrongReferencesOnMemoryPressure);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRemoveCommitRedirectUrlsArray);

// If enabled, prefetches and prerenders will not include a Purpose: prefetch
// header.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRemovePurposeHeaderForPrefetch);

// Makes preloaded fonts render-blocking up to the limits below.
// See https://crbug.com/1412861
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderBlockingFonts);
// Max milliseconds from navigation start that fonts can block rendering.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kMaxBlockingTimeMsForRenderBlockingFonts);
// Max milliseconds that font are allowed to delay of FCP.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kMaxFCPDelayMsForRenderBlockingFonts);

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

// This experiment evaluates various restrictions on the application of
// spelling/grammar highlights to prevent user dictionary leaks.
// For more see:
// https://explainers-by-googlers.github.io/user-dictionary-leaks/
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRestrictSpellingAndGrammarHighlights);

// If true, this disables spelling/grammar highlights performed on script
// edit (requiring user input to invoke).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedContents);

// If true, this disables spelling/grammar highlights performed on script
// enablement (requiring contents or selection change).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedEnablement);

// If true, this disables spelling/grammar highlights performed on script
// focus (requiring user gesture to invoke).
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictSpellingAndGrammarHighlightsChangedSelection);

// Aggregated flag for the restriction on HTTP Link headers on subresource
// responses. See crbug.com/417529151 for details.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kRestrictLinkHeaderOnSubresource);
// Disables only "rel=compression-dictionary".
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictLinkHeaderOnSubresourceCompressionDictionary);
// Disables all types of chained-preloads from cross-origin subresource
// responses.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictLinkHeaderOnSubresourceCrossOrigin);
// Disables "rel=dns-prefetch" and "rel=preconnect".
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictLinkHeaderOnSubresourceNetworkHint);
// Disables "rel=preload", "rel=modulepreload", and "rel=prefetch".
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRestrictLinkHeaderOnSubresourceResourceLoad);

// When enabled, it adds Payto URI Scheme to the safe list for
// registerProtocolHandler. This feature is disabled by default
// Payto URI Scheme explanation https://datatracker.ietf.org/doc/html/rfc8905
// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSafelistPaytoToRegisterProtocolHandler);

// When enabled, only pages that belong to a certain browsing context group are
// paused instead of all pages.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kPausePagesPerBrowsingContextGroup);

// Whether the HUD display is shown for paused pages.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kShowHudDisplayForPausedPages);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreaming);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kScriptStreamingForNonHTTP);

// If enabled, prefetches from NoStatePrefetchURLLoaderThrottle will be sent
// with the Sec-Purpose: "prefetch" header.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSecPurposePrefetchHeaderNoStatePrefetch);

// If enabled, prefetches from rel="prefetch" will be sent with the
// Sec-Purpose: "prefetch" header.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSecPurposePrefetchHeaderRelPrefetch);

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSendCnameAliasesToSubresourceFilterFromRenderer);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSetIntervalWithoutClamp);

// If enabled, the shared storage worklet threads (on the same renderer process)
// will share the same backing thread; otherwise, each will own a dedicated
// backing thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSharedStorageWorkletSharedBackingThreadImplementation);

// For the Shared Storage API, allow custom data origins in `createWorklet`.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kSharedStorageCreateWorkletCustomDataOrigin);

// For the Shared Storage API, allows saved queries in `selectURL()`.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageSelectURLSavedQueries);

// Enables WAL (write-ahead-logging) mode for the Shared Storage API SQLite
// database backend.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSharedStorageAPIEnableWALForDatabase);

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

// When enabled, some in-viewport images will initiate image decode immediately
// upon load, rather than waiting for the next BeginMainFrame to trigger decode
// as part of rasterization.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculativeImageDecodes);

// TODO(crbug/1431792): Speculatively warm-up service worker.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerWarmUp);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kSpeculativeServiceWorkerWarmUpMaxCount);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kSpeculativeServiceWorkerWarmUpDuration);
// Note: Following 3 bool params are declared without BASE_DECLARE_FEATURE_PARAM
// because the production code checks these values only once per param to
// initialize static local variables.
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerover;
BLINK_COMMON_EXPORT extern const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpOnPointerdown;

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerSyntheticResponse);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kServiceWorkerSyntheticResponseAllowedUrl);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kServiceWorkerSyntheticResponseDeniedUrlParams);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kServiceWorkerSyntheticResponseIgnoredHeaders);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kServiceWorkerSyntheticResponseReportInconsistentHeader);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kServiceWorkerSyntheticResponseDryRun);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kServiceWorkerSyntheticResponseBypassSubresource);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBoostRenderProcessForLoading);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kBoostRenderProcessForLoadingTargetUrls);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBoostRenderProcessForLoadingPrioritizePrerendering);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBoostRenderProcessForLoadingPrioritizePrerenderingOnly);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kBoostRenderProcessForLoadingPrioritizeRestore);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kBypassRequestForbiddenHeadersCheck);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStopInBackground);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kStreamlineRendererInit);

// Subsample a very chatty UKM metric.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSubSampleWindowProxyUsageMetrics);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kSupportOpeningDraggedLinksInSameTab);

// When enabled, task state traces are emitted for microtasks when the
// "task_attribution" trace category is enabled.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kTaskAttributionTraceMicrotaskTaskState);

// If enabled, reads and decodes navigation body data off the main thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedBodyLoader);

// If enabled, the HTMLPreloadScanner will run on a worker thread.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kThreadedPreloadScanner);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kThrottleFrameRateOnInitialization);

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

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUACHOverrideBlank);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kEmulateLoadStartedForInspectorOncePerResource);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUnloadBlocklisted);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUrgentMainFrameForInput);

// If enabled, URLPattern will use standard defined dummy URL canonicalization
// to canonicalize URL properties. See https://crbug.com/409350827
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kURLPatternDummyURLCanonicalization);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePageViewportInLCP);

// Always use IsPersistentCacheForCodeCacheEnabled() rather than checking this
// feature directly.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePersistentCacheForCodeCache);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSnappyForParkableStrings);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseZstdForParkableStrings);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kZstdCompressionLevel);

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

// Feature flag for controlling whether Web Bluetooth gatt.disconnect() can be
// used to cancel an ongoing gatt.connect() and have it rejected with an ABORT
// error. This makes the behavior match
// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattserver-disconnect.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothCancelConnect);

// Feature flag for making use of VideoFrameMetadata::capture_begin_time
// if set, instead of relating incoming media timestamps to local time in the
// WebRTC track source.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseCaptureBeginTimestamp);

// Feature to make WebRtcAudioSink use TimestampAligner to align absolute
// capture timestamps. This is disabled by default.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcAudioSinkUseTimestampAligner);

// This feature enables using Post-Quantum Crypto(PQC) for DTLS to improve
// WebRTC's security.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcPqcForDtls);

// TODO(crbug.com/466441366): Stop accepting 'borderless'.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppBorderless);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppEnableScopeExtensionsBySite);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppManifestLockScreen);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAppMigrationApi);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioAllowDenormalInProcessing);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebAudioDeferPullStatusUpdate);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebFontsCacheAwareTimeoutAdaption);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcHideLocalIpsWithMdns);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseMinMaxVEADimensions);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebUSBTransferSizeLimit);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebviewAccelerateSmallCanvases);

BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWorkerThreadSequentialShutdown);
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kWorkerThreadRespectTermRequest);

// Kill switch for https://crbug.com/415810136.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kNoReferrerForPreloadFromSubresource);

// Kill switch for crbug.com/407785197
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kWebRtcAllowDataChannelRecordingInWebrtcInternals);

// Indicates that renderer is running on an Android XR (AR/VR) device.
// Enables certain features which are not needed on other platforms.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kXrDevice);

// Enable the 'unframed' display override for IWAs. go/unframed-explainer-doc.
BLINK_COMMON_EXPORT BASE_DECLARE_FEATURE(kUnframedIwa);

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

BLINK_COMMON_EXPORT bool IsPersistentCacheForCodeCacheEnabled();

BLINK_COMMON_EXPORT bool IsSetIntervalWithoutClampEnabled();

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

BLINK_COMMON_EXPORT bool IsUpdateComplexSafaAreaConstraintsEnabled();

BLINK_COMMON_EXPORT bool IsXrDevice();

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
