// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "net/http/http_cache.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/switches.h"

namespace blink::features {

// -----------------------------------------------------------------------------
// Feature definitions and associated constants (feature params, et cetera)
//
// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When defining feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// Controls whether to include information about the page's open popup in
// AIPageContent.
BASE_FEATURE(kAIPageContentIncludePopupWindows,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether a missing subframe while generating the APC proto is
// silently dropped. If false, the entire APC is considered failed when this
// happens. When true, the subframe is simply skipped but the rest of APC
// generation is unaffected.
BASE_FEATURE(kAIPageContentMissingSubframesFailSilently,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BASE_FEATURE(kAdAuctionSignals, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAdAuctionSignalsMaxSizeBytes,
                   &kAdAuctionSignals,
                   "ad-auction-signals-max-size-bytes",
                   10000);

#if BUILDFLAG(IS_ANDROID)
// If enabled, then use desktop page webprefs for Android devices that have
// large displays, specifically tablets and desktops.
BASE_FEATURE(kAndroidDesktopWebPrefsLargeDisplays,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidSpellcheckNativeUi, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAndroidSpellcheckFullApiBlink, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Avoids copying ResourceRequest::TrustedParams when possible.
BASE_FEATURE(kAvoidTrustedParamsCopies, base::FEATURE_ENABLED_BY_DEFAULT);

// Async touchmoves after scroll.
// TODO(https://crbug.com/468997811): Cleanup feature flag.
BASE_FEATURE(kAsyncTouchMovesImmediatelyAfterScroll,
             base::FEATURE_ENABLED_BY_DEFAULT
);

// Block all MIDI access with the MIDI_SYSEX permission
BASE_FEATURE(kBlockMidiByDefault, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComputePressureRateObfuscationMitigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCrashReportingAPIMoreContextData,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideCrashReportingEndpoint, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLowerHighResolutionTimerThreshold,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDatapipeDrainedAsBytesConsumerInBFCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDevToolsMainThreadDebuggerForMultipleMainFrames,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables URN URLs like those produced by Protected Audience auctions to be
// displayed by iframes (instead of requiring fenced frames).
BASE_FEATURE(kAllowURNsInIframes, base::FEATURE_ENABLED_BY_DEFAULT);

// A console warning is shown when the opaque url returned from Protected
// Audience/selectUrl is used to navigate an iframe. Since fenced frames are not
// going to be enforced for these APIs in the short-medium term, disabling this
// warning for now.
BASE_FEATURE(kDisplayWarningDeprecateURNIframesUseFencedFrames,
             base::FEATURE_DISABLED_BY_DEFAULT);

// A server-side switch for the kRealtimeAudio thread type of
// RealtimeAudioWorkletThread object. This can be controlled by a field trial,
// it will use the kNormal type thread when disabled.
BASE_FEATURE(kAudioWorkletThreadRealtimePriority,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
// When enabled, RealtimeAudioWorkletThread scheduling is optimized taking into
// account how often the worklet logic is executed (which is determined by the
// AudioContext buffer duration).
BASE_FEATURE(kAudioWorkletThreadRealtimePeriodMac,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A thread pool system for effective usage of RealtimeAudioWorkletThread
// instances.
BASE_FEATURE(kAudioWorkletThreadPool, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, synthetic select metrics are logged.
// See go/analyzing-synthetic-selects for more details.
BASE_FEATURE(kAutofillEnableSyntheticSelectMetricsLogging,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, WebFormElement applies the same special case to nested forms
// as it does for the outermost form. The fix is relevant only to Autofill.
// For other callers of HTMLFormElement::ListedElements(), which don't traverse
// shadow trees and flatten nested forms, are not affected by the feature at
// all. This is a kill switch.
BASE_FEATURE(kAutofillFixFieldsAssociatedWithNestedFormsByParser,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If disabled (default for many years), autofilling triggers KeyDown and
// KeyUp events that do not send any key codes. If enabled, these events
// contain the "Unidentified" key.
BASE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill,
             base::FEATURE_DISABLED_BY_DEFAULT);

// https://crbug.com/1472970
BASE_FEATURE(kAutoSpeculationRules, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kAutoSpeculationRulesHoldback,
                   &kAutoSpeculationRules,
                   "holdback",
                   false);

// TODO(https://crbug.com/327075943): Delete this.
BASE_FEATURE(kBFCacheOpenBroadcastChannel, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBFCacheWithSharedWorker, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackForwardCacheDWCOnJavaScriptExecution,
             base::FEATURE_DISABLED_BY_DEFAULT);
// This is a kill switch for pausing microtask while the page is in the BFCache.
// Remove by m148 if things go well.
BASE_FEATURE(kBackForwardCachePauseMicrotasks,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable background resource fetch in Blink. See https://crbug.com/1379780 for
// more details.
BASE_FEATURE(kBackgroundResourceFetch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kBackgroundFontResponseProcessor,
                   &kBackgroundResourceFetch,
                   "background-font-response-processor",
                   true);
BASE_FEATURE_PARAM(bool,
                   kBackgroundScriptResponseProcessor,
                   &kBackgroundResourceFetch,
                   "background-script-response-processor",
                   true);
BASE_FEATURE_PARAM(bool,
                   kBackgroundCodeCacheDecoderStart,
                   &kBackgroundResourceFetch,
                   "background-code-cache-decoder-start",
                   true);

// Redefine the oklab and oklch spaces to have gamut mapping baked into them.
// https://crbug.com/1508329
BASE_FEATURE(kBakedGamutMapping, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BASE_FEATURE(kBackgroundTracingPerformanceMark,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kBackgroundTracingPerformanceMark_AllowList,
                   &kBackgroundTracingPerformanceMark,
                   "allow_list",
                   "");

// Boost the priority of certain loading tasks (https://crbug.com/1470003).
BASE_FEATURE(kBoostImageSetLoadingTaskPriority,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostFontLoadingTaskPriority, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostVideoLoadingTaskPriority, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostRenderBlockingStyleLoadingTaskPriority,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostNonRenderBlockingStyleLoadingTaskPriority,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the check for whether the IP address is publicly routable will be
// bypassed when determining the eligibility for a page to be included in topics
// calculation. This is useful for developers to test in local environment.
BASE_FEATURE(kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables calling the Topics API through Javascript (i.e.
// document.browsingTopics()). For this feature to take effect, the main Topics
// feature has to be enabled first (i.e. `kBrowsingTopics` is enabled, and,
// either a valid Origin Trial token exists or `kPrivacySandboxAdsAPIsOverride`
// is enabled.)
BASE_FEATURE(kBrowsingTopicsDocumentAPI, base::FEATURE_ENABLED_BY_DEFAULT);

// Decoupled with the main `kBrowsingTopics` feature, so it allows us to
// decouple the server side configs.
BASE_FEATURE(kBrowsingTopicsParameters, base::FEATURE_ENABLED_BY_DEFAULT);
// The periodic topics calculation interval.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsTimePeriodPerEpoch,
                   &kBrowsingTopicsParameters,
                   "time_period_per_epoch",
                   base::Days(7));
// The number of epochs from where to calculate the topics to give to a
// requesting contexts.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsNumberOfEpochsToExpose,
                   &kBrowsingTopicsParameters,
                   "number_of_epochs_to_expose",
                   3);
// The number of top topics to derive and to keep for each epoch (week).
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsNumberOfTopTopicsPerEpoch,
                   &kBrowsingTopicsParameters,
                   "number_of_top_topics_per_epoch",
                   5);
// The probability (in percent number) to return the random topic to a site. The
// "random topic" is per-site, and is selected from the full taxonomy uniformly
// at random, and each site has a
// `kBrowsingTopicsUseRandomTopicProbabilityPercent`% chance to see their random
// topic instead of one of the top topics.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsUseRandomTopicProbabilityPercent,
                   &kBrowsingTopicsParameters,
                   "use_random_topic_probability_percent",
                   5);
// Maximum delay between the calculation of the latest epoch and when a site
// starts seeing that epoch's topics. Each site transitions to the latest epoch
// at a per-site, per-epoch random time within
// [calculation time, calculation time + max delay).
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsMaxEpochIntroductionDelay,
                   &kBrowsingTopicsParameters,
                   "max_epoch_introduction_delay",
                   base::Days(2));
// The duration an epoch is retained before deletion.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsEpochRetentionDuration,
                   &kBrowsingTopicsParameters,
                   "epoch_retention_duration",
                   base::Days(28));
// Maximum time offset between when a site stops seeing an epoch's topics and
// when the epoch is actually deleted. Each site transitions away from the
// epoch at a per-site, per-epoch random time within
// [deletion time - max offset, deletion time].
//
// Note: The actual phase-out time can be influenced by the
// 'kBrowsingTopicsNumberOfEpochsToExpose' setting. If this setting enforces a
// more restrictive phase-out, that will take precedence.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsMaxEpochPhaseOutTimeOffset,
                   &kBrowsingTopicsParameters,
                   "max_epoch_phase_out_time_offset",
                   base::Days(2));
// How many epochs (weeks) of API usage data (i.e. topics observations) will be
// based off for the filtering of topics for a calling context.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering,
    &kBrowsingTopicsParameters,
    "number_of_epochs_of_observation_data_to_use_for_filtering",
    3);
// The max number of observed-by context domains to keep for each top topic
// during the epoch topics calculation. The final number of domains associated
// with each topic may be larger than this threshold, because that set of
// domains will also include all domains associated with the topic's descendant
// topics. The intent is to cap the in-use memory.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_domains_to_keep_per_topic",
    1000);
// The max number of entries allowed to be retrieved from the
// `BrowsingTopicsSiteDataStorage` database for each query for the API usage
// contexts. The query will occur once per epoch (week) at topics calculation
// time. The intent is to cap the peak memory usage.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_entries_to_load_per_epoch",
    100000);
// The max number of API usage context domains allowed to be stored per page
// load.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_domains_to_store_per_page_load",
    30);
// The taxonomy version. This only affects the topics classification that occurs
// during this browser session, and doesn't affect the pre-existing epochs.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsTaxonomyVersion,
                   &kBrowsingTopicsParameters,
                   "taxonomy_version",
                   kBrowsingTopicsTaxonomyVersionDefault);
// Comma separated Topic IDs to be blocked. Descendant topics of each blocked
// topic will be blocked as well.
BASE_FEATURE_PARAM(std::string,
                   kBrowsingTopicsDisabledTopicsList,
                   &kBrowsingTopicsParameters,
                   "disabled_topics_list",
                   "");
// Comma separated list of Topic IDs. Prioritize these topics and their
// descendants during top topic selection.
BASE_FEATURE_PARAM(std::string,
                   kBrowsingTopicsPrioritizedTopicsList,
                   &kBrowsingTopicsParameters,
                   "prioritized_topics_list",
                   "57,86,126,149,172,180,196,207,239,254,263,272,289,299,332");
// When a topics calculation times out for the first time, the duration to wait
// before starting a new one.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsFirstTimeoutRetryDelay,
                   &kBrowsingTopicsParameters,
                   "first_timeout_retry_delay",
                   base::Minutes(1));

// When enabled allows the header name used in the blink
// CacheStorageCodeCacheHint runtime feature to be modified.  This runtime
// feature disables generating full code cache for responses stored in
// cache_storage during a service worker install event.  The runtime feature
// must be enabled via the blink runtime feature mechanism, however.
BASE_FEATURE(kCacheStorageCodeCacheHintHeader,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kCacheStorageCodeCacheHintHeaderName,
                   &kCacheStorageCodeCacheHintHeader,
                   "name",
                   "x-CacheStorageCodeCacheHint");

// Temporarily disabled due to issues:
// - PDF blank previews
// - Canvas corruption on ARM64 macOS
// See https://g-issues.chromium.org/issues/328755781
BASE_FEATURE(kCanvas2DHibernation,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// When hibernating, make sure that the just-used transfer memory (to transfer
// the snapshot) is freed.
BASE_FEATURE(kCanvas2DHibernationReleaseTransferMemory,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// Don't hibernate small canvas elements.
BASE_FEATURE(kCanvas2DHibernationNoSmallCanvas,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// Whether to capture the source location of JavaScript execution, which is one
// of the renderer eviction reasons for Back/Forward Cache.
BASE_FEATURE(kCaptureJSExecutionLocation, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearSiteDataPrefetchPrerenderCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fix for CSS font comparison logic.
BASE_FEATURE(kCSSFontComparisonFix, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `dpr` client hint.
BASE_FEATURE(kClientHintsDPR_DEPRECATED, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `device-memory` client hint.
BASE_FEATURE(kClientHintsDeviceMemory_DEPRECATED,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `width` client hint.
BASE_FEATURE(kClientHintsResourceWidth_DEPRECATED,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `viewport-width` client hint.
BASE_FEATURE(kClientHintsViewportWidth_DEPRECATED,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
BASE_FEATURE(kCompressParkableStrings, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables more conservative settings for ParkableString: suspend parking in
// foreground, and increase aging tick intervals.
BASE_FEATURE(kLessAggressiveParkableString, base::FEATURE_ENABLED_BY_DEFAULT);

// Limits maximum capacity of disk data allocator per renderer process.
// DiskDataAllocator and its clients(ParkableString, ParkableImage) will try
// to keep the limitation.
BASE_FEATURE_PARAM(int,
                   kMaxDiskDataAllocatorCapacityMB,
                   &kCompressParkableStrings,
                   "max_disk_capacity_mb",
                   -1);

// When enabled, CreateNewWindow() and ShowCreatedWindow() mojo calls are
// coalesced into a single call to CreateNewWindow().
BASE_FEATURE(kCombineNewWindowIPCs, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls off-thread code cache consumption.
BASE_FEATURE(kConsumeCodeCacheOffThread, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the constant streaming in the ContentCapture task.
BASE_FEATURE(kContentCaptureConstantStreaming,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, add a new option, {imageOrientation: 'none'}, to
// createImageBitmap, which ignores the image orientation metadata of the source
// and renders the image as encoded.
BASE_FEATURE(kCreateImageBitmapOrientationNone,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Declarative CSS Modules will generate a DataURI instead of a
// Blob URL in the generated Import Map.
BASE_FEATURE(kDeclarativeCSSModulesUseDataURI,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeferRendererTasksAfterInput, base::FEATURE_ENABLED_BY_DEFAULT);

const char kDeferRendererTasksAfterInputPolicyParamName[] = "policy";
const char kDeferRendererTasksAfterInputMinimalTypesPolicyName[] =
    "minimal-types";
const char
    kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName[] =
        "non-user-blocking-deferrable-types";
const char kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName[] =
    "non-user-blocking-types";
const char kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName[] =
    "all-deferrable-types";
const char kDeferRendererTasksAfterInputAllTypesPolicyName[] = "all-types";

const base::FeatureParam<TaskDeferralPolicy>::Option kTaskDeferralOptions[] = {
    {TaskDeferralPolicy::kMinimalTypes,
     kDeferRendererTasksAfterInputMinimalTypesPolicyName},
    {TaskDeferralPolicy::kNonUserBlockingDeferrableTypes,
     kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName},
    {TaskDeferralPolicy::kNonUserBlockingTypes,
     kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName},
    {TaskDeferralPolicy::kAllDeferrableTypes,
     kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName},
    {TaskDeferralPolicy::kAllTypes,
     kDeferRendererTasksAfterInputAllTypesPolicyName}};

BASE_FEATURE_ENUM_PARAM(TaskDeferralPolicy,
                        kTaskDeferralPolicyParam,
                        &kDeferRendererTasksAfterInput,
                        kDeferRendererTasksAfterInputPolicyParamName,
                        TaskDeferralPolicy::kAllTypes,
                        &kTaskDeferralOptions);

BASE_FEATURE(kDelayAsyncScriptExecution, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<DelayAsyncScriptDelayType>::Option
    delay_async_script_execution_delay_types[] = {
        {DelayAsyncScriptDelayType::kFinishedParsing, "finished_parsing"},
        {DelayAsyncScriptDelayType::kFirstPaintOrFinishedParsing,
         "first_paint_or_finished_parsing"},
        {DelayAsyncScriptDelayType::kTillFirstLcpCandidate,
         "till_first_lcp_candidate"},
};

BASE_FEATURE_ENUM_PARAM(DelayAsyncScriptDelayType,
                        kDelayAsyncScriptExecutionDelayParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_delay_type",
                        DelayAsyncScriptDelayType::kFinishedParsing,
                        &delay_async_script_execution_delay_types);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionCrossSiteOnlyParam,
                   &kDelayAsyncScriptExecution,
                   "cross_site_only",
                   false);

// kDelayAsyncScriptExecution will delay executing async script at max
// |delay_async_exec_delay_limit|.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayAsyncScriptExecutionDelayLimitParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_delay_limit",
                   base::Seconds(0));

// kDelayAsyncScriptExecution will be disabled after document elapsed more than
// |delay_async_exec_feature_limit|. Zero value means no limit.
// This is to avoid unnecessary async script delay after LCP (for
// kEachLcpCandidate or kEachPaint). Because we can't determine the LCP timing
// while loading, we use timeout instead.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayAsyncScriptExecutionFeatureLimitParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_feature_limit",
                   base::Seconds(0));

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionDelayByDefaultParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_delay_by_default",
                   true);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionMainFrameOnlyParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_main_frame_only",
                   false);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionWhenLcpFoundInHtml,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_when_lcp_found_in_html",
                   false);

// kDelayAsyncScriptExecution will change evaluation schedule for the
// specified target.
const base::FeatureParam<AsyncScriptExperimentalSchedulingTarget>::Option
    async_script_experimental_scheduling_targets[] = {
        {AsyncScriptExperimentalSchedulingTarget::kAds, "ads"},
        {AsyncScriptExperimentalSchedulingTarget::kNonAds, "non_ads"},
        {AsyncScriptExperimentalSchedulingTarget::kBoth, "both"},
};
BASE_FEATURE_ENUM_PARAM(AsyncScriptExperimentalSchedulingTarget,
                        kDelayAsyncScriptExecutionTargetParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_target_script_category",
                        AsyncScriptExperimentalSchedulingTarget::kBoth,
                        &async_script_experimental_scheduling_targets);

// If true, kDelayAsyncScriptExecution will not change the script
// evaluation timing for the non parser inserted script.
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncExecExcludeNonParserInsertedParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_exclude_non_parser_inserted",
                   false);

// If true, kDelayAsyncScriptExecution will not change the script
// evaluation timing for the scripts that were added via document.write().
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncExecExcludeDocumentWriteParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_exclude_document_write",
                   false);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutLowFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_low_fetch_priority_hint",
                   false);
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutAutoFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_auto_fetch_priority_hint",
                   false);
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutHighFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_high_fetch_priority_hint",
                   true);

BASE_FEATURE(kDelayLayerTreeViewDeletionOnLocalSwap,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam,
                   &kDelayLayerTreeViewDeletionOnLocalSwap,
                   "deletion_task_delay",
                   base::Milliseconds(1000));

// Improves the signal-to-noise ratio of network error related messages in the
// DevTools Console.
// See http://crbug.com/124534.
BASE_FEATURE(kDevToolsImprovedNetworkError, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDirectCompositorThreadIpc,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDisableArrayBufferSizeLimitsForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardInputEventsToRecentlyMovedFrames,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Drop input events at the browser process until the process receives the first
// signal that the renderer has sent a frame to cc (https://crbug.com/40057499).
BASE_FEATURE(kDropInputEventsWhilePaintHolding,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Extends console.timestamp to support adding deep-links into the DevTools
// Performance Panel, which (when clicked) call into a DevTools extension.
BASE_FEATURE(kEnableDevtoolsDeepLinkViaExtensibilityApi,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to respect loading=lazy attribute for images when they are on
// invisible pages.
BASE_FEATURE(kEnableLazyLoadImageForInvisiblePage,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<EnableLazyLoadImageForInvisiblePageType>::Option
    enable_lazy_load_image_for_invisible_page_types[] = {
        {EnableLazyLoadImageForInvisiblePageType::kAllInvisiblePage,
         "all_invisible_page"},
        {EnableLazyLoadImageForInvisiblePageType::kPrerenderPage,
         "prerender_page"}};
BASE_FEATURE_ENUM_PARAM(
    EnableLazyLoadImageForInvisiblePageType,
    kEnableLazyLoadImageForInvisiblePageTypeParam,
    &kEnableLazyLoadImageForInvisiblePage,
    "enabled_page_type",
    EnableLazyLoadImageForInvisiblePageType::kAllInvisiblePage,
    &enable_lazy_load_image_for_invisible_page_types);

// Prevents an opener from being returned when a BlobURL is cross-site to the
// window's top-level site.
BASE_FEATURE(kEnforceNoopenerOnBlobURLNavigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExpandCompositedCullRect, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kCullRectPixelDistanceToExpand,
                   &kExpandCompositedCullRect,
                   "pixels",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
                   2000
#else
                   4000
#endif
);
BASE_FEATURE_PARAM(double,
                   kCullRectExpansionDPRCoef,
                   &kExpandCompositedCullRect,
                   "dpr_coef",
                   1);
BASE_FEATURE_PARAM(bool,
                   kSmallScrollersUseMinCullRect,
                   &kExpandCompositedCullRect,
                   "small_scroller_opt",
                   true);
BASE_FEATURE_PARAM(int,
                   kCullRectChangedEnoughDistance,
                   &kExpandCompositedCullRect,
                   "changed_enough",
                   512);

BASE_FEATURE(kFadeInScrollbarWhenMouseWheelMayBegin,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the <fencedframe> element; see crbug.com/1123606. Note that enabling
// this feature does not automatically expose this element to the web, it only
// allows the element to be enabled by the runtime enabled feature, for origin
// trials.
BASE_FEATURE(kFencedFrames, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable sending event-level reports through reportEvent() in cross-origin
// subframes. This requires opt-in both from the cross-origin subframe that is
// sending the beacon as well as the document that contains information about
// the reportEvent() endpoints.
BASE_FEATURE(kFencedFramesCrossOriginEventReporting,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Temporarily un-disable credentials on fenced frame automatic beacons until
// third party cookie deprecation.
// TODO(crbug.com/1496395): Remove this after 3PCD.
BASE_FEATURE(kFencedFramesAutomaticBeaconCredentials,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFencedFramesCrossOriginAutomaticBeaconData,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls functionality related to network revocation/local unpartitioned
// data access in fenced frames.
BASE_FEATURE(kFencedFramesLocalUnpartitionedDataAccess,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFencedFramesReportEventHeaderChanges,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a bug fix that allows a 'src' allowlist in the |allow| parameter of a
// <fencedframe> or <iframe> loaded with a FencedFrameConfig to behave as
// expected. See: https://crbug.com/349080952
BASE_FEATURE(kFencedFramesSrcPermissionsPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls access to an API to exempt certain URLs from fenced frame
// network revocation to facilitate testing.
BASE_FEATURE(kExemptUrlFromNetworkRevocationForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use "style" and "json" destinations for CSS and JSON modules.
// https://crbug.com/1491336
BASE_FEATURE(kFetchDestinationJsonCssModules,
             "kFetchDestinationJsonCssModules",
             base::FEATURE_ENABLED_BY_DEFAULT);

// File handling icons. https://crbug.com/1218213
BASE_FEATURE(kFileHandlingIcons, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigationForChromeAppsOnly,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFilteringScrollPrediction,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(b/284271126): Run the experiment on desktop and enable if
             // positive.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE_PARAM(std::string,
                   kFilteringScrollPredictionFilterParam,
                   &kFilteringScrollPrediction,
                   "filter",
                   "one_euro_filter");

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Enables FLEDGE implementation. See https://crbug.com/1186444.
BASE_FEATURE(kFledge, base::FEATURE_ENABLED_BY_DEFAULT);

// See
// https://github.com/WICG/turtledove/blob/main/FLEDGE_browser_bidding_and_auction_API.md
BASE_FEATURE(kFledgeBiddingAndAuctionServer, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kFledgeBiddingAndAuctionKeyURL,
                   &kFledgeBiddingAndAuctionServer,
                   "FledgeBiddingAndAuctionKeyURL",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kFledgeBiddingAndAuctionKeyConfig,
                   &kFledgeBiddingAndAuctionServer,
                   "FledgeBiddingAndAuctionKeyConfig",
                   "");

// See https://github.com/WICG/turtledove/issues/1334
BASE_FEATURE(kFledgeOriginScopedKeys, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kFledgeOriginScopedKeyConfig,
                   &kFledgeOriginScopedKeys,
                   "FledgeOriginScopedKeyConfig",
                   "");

// See in the header.
BASE_FEATURE(kFledgeConsiderKAnonymity, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFledgeEnforceKAnonymity, base::FEATURE_DISABLED_BY_DEFAULT);

// See the header for more details.
BASE_FEATURE(kFledgeLimitSelectableBuyerAndSellerReportingIds,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeSelectableBuyerAndSellerReportingIdsSoftLimit,
                   &kFledgeLimitSelectableBuyerAndSellerReportingIds,
                   "SelectableBuyerAndSellerReportingIdsSoftLimit",
                   -1);
BASE_FEATURE_PARAM(int,
                   kFledgeSelectableBuyerAndSellerReportingIdsHardLimit,
                   &kFledgeLimitSelectableBuyerAndSellerReportingIds,
                   "SelectableBuyerAndSellerReportingIdsHardLimit",
                   -1);

BASE_FEATURE(kFledgeMaxGroupLifetimeFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeMaxGroupLifetime,
                   &kFledgeMaxGroupLifetimeFeature,
                   "fledge_max_group_lifetime",
                   base::Days(30));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeMaxGroupLifetimeForMetadata,
                   &kFledgeMaxGroupLifetimeFeature,
                   "fledge_max_group_lifetime_for_metadata",
                   base::Days(30));

BASE_FEATURE(kFledgeEnableSampleDebugReportOnCookieSetting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeSampleDebugReports, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportLockout,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_lockout",
                   base::Days(365 * 3));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportRestrictedCooldown,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_restricted_cooldown",
                   base::Days(365));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportShortCooldown,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_short_cooldown",
                   base::Days(14));
BASE_FEATURE_PARAM(int,
                   kFledgeDebugReportSamplingRandomMax,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_sampling_random_max",
                   1000);
BASE_FEATURE_PARAM(
    int,
    kFledgeDebugReportSamplingRestrictedCooldownRandomMax,
    &kFledgeSampleDebugReports,
    "fledge_debug_report_sampling_restricted_cooldown_random_max",
    10);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeEnableFilteringDebugReportStartingFrom,
                   &kFledgeSampleDebugReports,
                   "fledge_enable_filtering_debug_report_starting_from",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(int,
                   kFledgeCustomMaxAuctionAdComponentsValue,
                   &kFledgeCustomMaxAuctionAdComponents,
                   "FledgeAdComponentLimit",
                   40);

BASE_FEATURE_PARAM(int,
                   kFledgeRealTimeReportingNumBuckets,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingNumBuckets",
                   1024);
BASE_FEATURE_PARAM(double,
                   kFledgeRealTimeReportingEpsilon,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingEpsilon",
                   1);
BASE_FEATURE_PARAM(double,
                   kFledgeRealTimeReportingPlatformContributionPriority,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingPlatformContributionPriority",
                   1);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeRealTimeReportingWindow,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingWindow",
                   base::Seconds(20));
BASE_FEATURE_PARAM(int,
                   kFledgeRealTimeReportingMaxReports,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingMaxReports",
                   10);

// Enable enforcement of permission policy for
// privateAggregation.contributeToHistogramOnEvent.
BASE_FEATURE(kFledgeEnforcePermissionPolicyContributeOnEvent,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeDisableLocalAdsAuctions, base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a configurable limit on the number of
// `selectableBuyerAndSellerReportingIds` for which the browser fetches k-anon
// keys. If the `SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` is
// negative, no limit is enforced.
BASE_FEATURE(kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit,
    &kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
    "SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit",
    -1);

// Feature flag to truncate the set of `selectableBuyerAndSellerReportingIds`
// to only those for which k-anon status was fetched, as limited by the
// `kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` parameter
// defined above. This is only meaningful if
// `kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` is >= 0.
BASE_FEATURE(kFledgeTruncateSelectableBuyerAndSellerReportingIdsToKAnonLimit,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceHighPerformanceGPUForWebGL,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Text autosizing uses heuristics to inflate text sizes on devices with
// small screens. This feature is for disabling these heuristics.
BASE_FEATURE(kForceOffTextAutosizing, base::FEATURE_ENABLED_BY_DEFAULT);

// Automatically convert light-themed pages to use a Blink-generated dark theme
BASE_FEATURE(kForceWebContentsDarkMode,
             "WebContentsForceDark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 255 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
BASE_FEATURE_PARAM(int,
                   kForceDarkForegroundLightnessThresholdParam,
                   &kForceWebContentsDarkMode,
                   "foreground_lightness_threshold",
                   -1);

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 255 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
BASE_FEATURE_PARAM(int,
                   kForceDarkBackgroundLightnessThresholdParam,
                   &kForceWebContentsDarkMode,
                   "background_lightness_threshold",
                   -1);

BASE_FEATURE(kFrameMetadataObserver, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the frequency capping for detecting large sticky ads.
// Large-sticky-ads are those ads that stick to the bottom of the page
// regardless of a user’s efforts to scroll, and take up more than 30% of the
// screen’s real estate.
BASE_FEATURE(kFrequencyCappingForLargeStickyAdDetection,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the frequency capping for detecting overlay popups. Overlay-popups
// are the interstitials that pop up and block the main content of the page.
BASE_FEATURE(kFrequencyCappingForOverlayPopupDetection,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGMSCoreEmoji, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, then display audio track permission failures are ignored.
BASE_FEATURE(kGetDisplayMediaIgnoreAudioPermissionFailures,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kHTMLParserYieldEventNameForPause,
                   &kHTMLParserYieldByUserTiming,
                   "pause_event_name",
                   "");

BASE_FEATURE_PARAM(std::string,
                   kHTMLParserYieldEventNameForResume,
                   &kHTMLParserYieldByUserTiming,
                   "resume_event_name",
                   "");

BASE_FEATURE_PARAM(size_t,
                   kHTMLParserYieldTimeoutInMs,
                   &kHTMLParserYieldByUserTiming,
                   "timeout_ms",
                   20);

BASE_FEATURE(kIgnoreInputWhileHidden,
             // TODO(crbug.com/407265465) Some Accessibility tools on Windows
             // appear to mark the Renderer as Hidden. This feature currently
             // breaks them. Disabling until the root cause can be identified.
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kImageLoadingPrioritizationFix, base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInitialWebUIWithoutExtensions, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kIndexedDBCompressValuesWithSnappy,
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int>
    kIndexedDBCompressValuesWithSnappyCompressionThreshold{
        &features::kIndexedDBCompressValuesWithSnappy,
        /*name=*/"compression-threshold",
        /*default_value=*/-1};

BASE_FEATURE(kInputPredictorTypeChoice, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, wake ups from throttleable TaskQueues are limited to 1 per
// minute in a page that has been backgrounded for 5 minutes.
//
// Intensive wake up throttling is enforced in addition to other throttling
// mechanisms:
//  - 1 wake up per second in a background page or hidden cross-origin frame
//  - 1% CPU time in a page that has been backgrounded for 10 seconds
//
// Feature tracking bug: https://crbug.com/1075553
//
// The base::Feature should not be read from; rather the provided accessors
// should be used, which also take into account the managed policy override of
// the feature.
//
// The base::Feature is enabled by default on all platforms. However, on
// Android, it has no effect because page freezing kicks in at the same time. It
// would have an effect if the grace period ("grace_period_seconds" param) was
// reduced.
BASE_FEATURE(kIntensiveWakeUpThrottling, base::FEATURE_ENABLED_BY_DEFAULT);

// Name of the parameter that controls the grace period during which there is no
// intensive wake up throttling after a page is hidden. Defined here to allow
// access from about_flags.cc. The FeatureParam is defined in
// third_party/blink/renderer/platform/scheduler/common/features.cc.
const char kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[] =
    "grace_period_seconds";

BASE_FEATURE(kInteractiveDetectorIgnoreFcp, base::FEATURE_DISABLED_BY_DEFAULT);

// Allow process isolation of iframes with the 'sandbox' attribute set. Whether
// or not such an iframe will be isolated may depend on options specified with
// the attribute. Note: At present, only iframes with origin-restricted
// sandboxes are isolated.
BASE_FEATURE(kIsolateSandboxedIframes, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<IsolateSandboxedIframesGrouping>::Option
    isolated_sandboxed_iframes_grouping_types[] = {
        {IsolateSandboxedIframesGrouping::kPerSite, "per-site"},
        {IsolateSandboxedIframesGrouping::kPerOrigin, "per-origin"},
        {IsolateSandboxedIframesGrouping::kPerDocument, "per-document"}};
BASE_FEATURE_ENUM_PARAM(IsolateSandboxedIframesGrouping,
                        kIsolateSandboxedIframesGroupingParam,
                        &kIsolateSandboxedIframes,
                        "grouping",
                        IsolateSandboxedIframesGrouping::kPerOrigin,
                        &isolated_sandboxed_iframes_grouping_types);

BASE_FEATURE(kKeepAliveInBrowserMigration, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLimitLayerMergeDistance, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kLayerMergeDistanceLimit,
                   &kLimitLayerMergeDistance,
                   "limit",
                   0x10000000);

BASE_FEATURE(kLCPCriticalPathPredictor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kLCPCriticalPathAdjustImageLoadPriority,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority",
                   false);

BASE_FEATURE_PARAM(size_t,
                   kLCPCriticalPathPredictorMaxElementLocatorLength,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_element_locator_length",
                   1024);

BASE_FEATURE_PARAM(bool,
                   kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority_override_first_n_boost",
                   false);

BASE_FEATURE_PARAM(double,
                   kLcppAdjustImageLoadPriorityConfidenceThreshold,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority_confidence_threshold",
                   0);

const base::FeatureParam<LcppRecordedLcpElementTypes>::Option
    lcpp_recorded_element_types[] = {
        {LcppRecordedLcpElementTypes::kAll, "all"},
        {LcppRecordedLcpElementTypes::kImageOnly, "image_only"},
};
BASE_FEATURE_ENUM_PARAM(LcppRecordedLcpElementTypes,
                        kLCPCriticalPathPredictorRecordedLcpElementTypes,
                        &kLCPCriticalPathPredictor,
                        "lcpp_recorded_lcp_element_types",
                        LcppRecordedLcpElementTypes::kImageOnly,
                        &lcpp_recorded_element_types);

const base::FeatureParam<LcppResourceLoadPriority>::Option
    lcpp_resource_load_priorities[] = {
        {LcppResourceLoadPriority::kMedium, "medium"},
        {LcppResourceLoadPriority::kHigh, "high"},
        {LcppResourceLoadPriority::kVeryHigh, "very_high"},
};
BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPCriticalPathPredictorImageLoadPriority,
                        &kLCPCriticalPathPredictor,
                        "lcpp_image_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_PARAM(
    bool,
    kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement,
    &kLCPCriticalPathPredictor,
    "lcpp_enable_image_load_priority_for_htmlimageelement",
    false);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorMaxHostsToTrack,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_hosts_to_track",
                   100);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorSlidingWindowSize,
                   &kLCPCriticalPathPredictor,
                   "lcpp_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorMaxHistogramBuckets,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPScriptObserver, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPScriptObserverScriptLoadPriority,
                        &kLCPScriptObserver,
                        "lcpscriptobserver_script_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPScriptObserverImageLoadPriority,
                        &kLCPScriptObserver,
                        "lcpscriptobserver_image_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_PARAM(size_t,
                   kLCPScriptObserverMaxUrlLength,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_script_max_url_length",
                   1024);

BASE_FEATURE_PARAM(size_t,
                   kLCPScriptObserverMaxUrlCountPerOrigin,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_script_max_url_count_per_origin",
                   5);

BASE_FEATURE_PARAM(bool,
                   kLCPScriptObserverAdjustImageLoadPriority,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_adjust_image_load_priority",
                   false);

BASE_FEATURE_PARAM(int,
                   kLCPScriptObserverSlidingWindowSize,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPScriptObserverMaxHistogramBuckets,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPTimingPredictorPrerender2, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kLCPTimingPredictorSlidingWindowSize,
                   &kLCPTimingPredictorPrerender2,
                   "lcp_timing_predictor_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPTimingPredictorMaxHistogramBuckets,
                   &kLCPTimingPredictorPrerender2,
                   "lcp_timing_predictor_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPAutoPreconnectLcpOrigin, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(double,
                   kLCPPAutoPreconnectFrequencyThreshold,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_frequency_threshold",
                   0.5);

BASE_FEATURE_PARAM(int,
                   kkLCPPAutoPreconnectMaxPreconnectOriginsCount,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_max_origins",
                   2);

BASE_FEATURE_PARAM(int,
                   kLCPPAutoPreconnectSlidingWindowSize,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPAutoPreconnectMaxHistogramBuckets,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_max_histogram_buckets",
                   10);

BASE_FEATURE_PARAM(bool,
                   kLCPPAutoPreconnectRecordAllOrigins,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_record_all_origins",
                   false);

BASE_FEATURE(kLCPPDeferUnusedPreload, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<LcppDeferUnusedPreloadExcludedResourceType>::Option
    lcpp_defer_unused_preload_excluded_resource_type[] = {
        {LcppDeferUnusedPreloadExcludedResourceType::kNone, "none"},
        {LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet, "stylesheet"},
        {LcppDeferUnusedPreloadExcludedResourceType::kScript, "script"},
        {LcppDeferUnusedPreloadExcludedResourceType::kMock, "mock"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadExcludedResourceType,
                        kLcppDeferUnusedPreloadExcludedResourceType,
                        &kLCPPDeferUnusedPreload,
                        "excluded_resource_type",
                        LcppDeferUnusedPreloadExcludedResourceType::kNone,
                        &lcpp_defer_unused_preload_excluded_resource_type);

BASE_FEATURE_PARAM(double,
                   kLCPPDeferUnusedPreloadFrequencyThreshold,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_frequency_threshold",
                   0.5);

const base::FeatureParam<LcppDeferUnusedPreloadPreloadedReason>::Option
    lcpp_defer_unused_preload_preloaded_reason[] = {
        {LcppDeferUnusedPreloadPreloadedReason::kAll, "all"},
        {LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly,
         "link_preload"},
        {LcppDeferUnusedPreloadPreloadedReason::kBrowserSpeculativePreloadOnly,
         "speculative_preload"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadPreloadedReason,
                        kLcppDeferUnusedPreloadPreloadedReason,
                        &kLCPPDeferUnusedPreload,
                        "preloaded_reason",
                        LcppDeferUnusedPreloadPreloadedReason::kAll,
                        &lcpp_defer_unused_preload_preloaded_reason);

const base::FeatureParam<LcppDeferUnusedPreloadTiming>::Option
    lcpp_defer_unused_preload_timing[] = {
        {LcppDeferUnusedPreloadTiming::kPostTask, "post_task"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictor,
         "lcp_timing_predictor"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictorWithPostTask,
         "lcp_timing_predictor_with_post_task"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadTiming,
                        kLcppDeferUnusedPreloadTiming,
                        &kLCPPDeferUnusedPreload,
                        "load_timing",
                        LcppDeferUnusedPreloadTiming::kPostTask,
                        &lcpp_defer_unused_preload_timing);

BASE_FEATURE_PARAM(int,
                   kLCPPDeferUnusedPreloadSlidingWindowSize,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPDeferUnusedPreloadMaxHistogramBuckets,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPFontURLPredictor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kLCPPFontURLPredictorMaxUrlLength,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_length",
                   1024);

BASE_FEATURE_PARAM(size_t,
                   kLCPPFontURLPredictorMaxUrlCountPerOrigin,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_count_per_origin",
                   10);

BASE_FEATURE_PARAM(double,
                   kLCPPFontURLPredictorFrequencyThreshold,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_url_frequency_threshold",
                   0.5);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorMaxPreloadCount,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_to_preload",
                   5);

BASE_FEATURE_PARAM(bool,
                   kLCPPFontURLPredictorEnablePrefetch,
                   &kLCPPFontURLPredictor,
                   "lcpp_enable_font_prefetch",
                   false);

// Negative value is used for disabling this threshold.
BASE_FEATURE_PARAM(double,
                   kLCPPFontURLPredictorThresholdInMbps,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_prefetch_threshold",
                   -1);

const base::FeatureParam<std::string> kLCPPFontURLPredictorExcludedHosts{
    &kLCPPFontURLPredictor, "lcpp_font_prefetch_excluded_hosts", ""};

BASE_FEATURE_PARAM(bool,
                   kLCPPCrossSiteFontPredictionAllowed,
                   &kLCPPFontURLPredictor,
                   "lcpp_cross_site_font_prediction_allowed",
                   false);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorSlidingWindowSize,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorMaxHistogramBuckets,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPInitiatorOrigin, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kLcppInitiatorOriginHistogramSlidingWindowSize,
                   &kLCPPInitiatorOrigin,
                   "lcpp_initiator_origin_histogram_sliding_window_size",
                   10000);

BASE_FEATURE_PARAM(int,
                   kLcppInitiatorOriginMaxHistogramBuckets,
                   &kLCPPInitiatorOrigin,
                   "lcpp_initiator_origin_max_histogram_buckets",
                   100);

BASE_FEATURE(kLCPPLazyLoadImagePreload, base::FEATURE_ENABLED_BY_DEFAULT);

// If true, do not make a preload request.
BASE_FEATURE_PARAM(bool,
                   kLCPPLazyLoadImagePreloadDryRun,
                   &kLCPPLazyLoadImagePreload,
                   "lcpp_lazy_load_image_preload_dry_run",
                   false);

const base::FeatureParam<
    LcppPreloadLazyLoadImageType>::Option lcpp_preload_lazy_load_image[] = {
    {LcppPreloadLazyLoadImageType::kNone, "none"},
    {LcppPreloadLazyLoadImageType::kNativeLazyLoading, "native_lazy_loading"},
    {LcppPreloadLazyLoadImageType::kCustomLazyLoading, "custom_lazy_loading"},
    {LcppPreloadLazyLoadImageType::kAll, "all"},
};
BASE_FEATURE_ENUM_PARAM(LcppPreloadLazyLoadImageType,
                        kLCPCriticalPathPredictorPreloadLazyLoadImageType,
                        &kLCPPLazyLoadImagePreload,
                        "lcpp_preload_lazy_load_image_type",
                        LcppPreloadLazyLoadImageType::kNativeLazyLoading,
                        &lcpp_preload_lazy_load_image);

BASE_FEATURE(kPreloadSystemFonts, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPreloadSystemFontsTargets{
    &kPreloadSystemFonts, "preload_system_fonts_targets", "[]"};

BASE_FEATURE_PARAM(int,
                   kPreloadSystemFontsRequiredMemoryGB,
                   &kPreloadSystemFonts,
                   "preload_system_fonts_required_memory_gb",
                   4);

BASE_FEATURE(kLCPPMultipleKey, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kLCPPMultipleKeyMaxPathLength,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_max_path_length",
                   15);

const base::FeatureParam<LcppMultipleKeyTypes>::Option
    lcpp_multiple_key_types[] = {
        {LcppMultipleKeyTypes::kDefault, "default"},
        {LcppMultipleKeyTypes::kLcppKeyStat, "lcpp_key_stat"},
};

BASE_FEATURE_ENUM_PARAM(LcppMultipleKeyTypes,
                        kLcppMultipleKeyType,
                        &kLCPPMultipleKey,
                        "lcpp_multiple_key_type",
                        LcppMultipleKeyTypes::kLcppKeyStat,
                        &lcpp_multiple_key_types);

BASE_FEATURE_PARAM(int,
                   kLcppMultipleKeyHistogramSlidingWindowSize,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_histogram_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLcppMultipleKeyMaxHistogramBuckets,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPPrefetchSubresource, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLCPPPrefetchSubresourceAsync, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHttpDiskCachePrewarming, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kHttpDiskCachePrewarmingMaxUrlLength,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_max_url_length",
                   1024);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingHistorySize,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_history_size",
                   1024);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kHttpDiskCachePrewarmingReprewarmPeriod,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_reprewarm_period",
                   base::Minutes(10));

BASE_FEATURE_PARAM(bool,
                   kHttpDiskCachePrewarmingTriggerOnNavigation,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_trigger_on_navigation",
                   true);

BASE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover,
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_trigger_on_pointer_down_or_hover",
    true);

BASE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption,
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_use_read_and_discard_body_option",
    false);

BASE_FEATURE_PARAM(bool,
                   kHttpDiskCachePrewarmingSkipDuringBrowserStartup,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_skip_during_browser_startup",
                   true);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingSlidingWindowSize,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingMaxHistogramBuckets,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_max_histogram_buckets",
                   10);

BASE_FEATURE(kLegacyParsingOfXContentTypeOptions,
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to reduce the set of resources fetched by No-State Prefetch.
BASE_FEATURE(kLightweightNoStatePrefetch,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLinkPreview, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<LinkPreviewTriggerType>::Option
    link_preview_trigger_type_options[] = {
        {LinkPreviewTriggerType::kAltClick, "alt_click"},
        {LinkPreviewTriggerType::kAltHover, "alt_hover"},
        {LinkPreviewTriggerType::kLongPress, "long_press"}};
BASE_FEATURE_ENUM_PARAM(LinkPreviewTriggerType,
                        kLinkPreviewTriggerType,
                        &kLinkPreview,
                        "trigger_type",
                        LinkPreviewTriggerType::kAltHover,
                        &link_preview_trigger_type_options);

// Makes network loading tasks unfreezable so that they can be processed while
// the page is frozen.
BASE_FEATURE(kLoadingTasksUnfreezable, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow low latency canvas 2D to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (Canvas2DImageChromium is disabled).
BASE_FEATURE(kLowLatencyCanvas2dImageChromium,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
);

// Allow low latency WebGL to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (WebGLImageChromium is disabled).
BASE_FEATURE(kLowLatencyWebGLImageChromium,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLowPriorityAsyncScriptExecution,
// TODO(crbug/429069717): Fix the high power consumption on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(double,
                   kMinimumPhysicalMemoryForLowPriorityAsyncScriptExecution,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_minimum_physical_memory_gb",
                   3.0);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTimeoutForLowPriorityAsyncScriptExecution,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_timeout",
                   base::Seconds(1));

// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |low_pri_async_exec_feature_limit|. Zero value means no limit.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kLowPriorityAsyncScriptExecutionFeatureLimitParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_feature_limit",
                   base::Seconds(3));

// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_cross_site_only",
                   true);

BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionMainFrameOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_main_frame_only",
                   true);

// kLowPriorityAsyncScriptExecution will exclude scripts that influence LCP
// element.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_exclude_lcp_influencers",
                   false);

// kLowPriorityAsyncScriptExecution will exclude scripts on pages where LCP
// element isn't directly embedded in HTML.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_disable_when_lcp_not_in_html",
                   false);

// kLowPriorityAsyncScriptExecution will change evaluation schedule for the
// specified target.
BASE_FEATURE_ENUM_PARAM(AsyncScriptExperimentalSchedulingTarget,
                        kLowPriorityAsyncScriptExecutionTargetParam,
                        &kLowPriorityAsyncScriptExecution,
                        "low_pri_async_exec_target",
                        AsyncScriptExperimentalSchedulingTarget::kNonAds,
                        &async_script_experimental_scheduling_targets);
// If true, kLowPriorityAsyncScriptExecution will not change the script
// evaluation timing for the non parser inserted script.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionExcludeNonParserInsertedParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_exclude_non_parser_inserted",
    false);
// If true, kLowPriorityAsyncScriptExecution will not change the script
// evaluation timing for the scripts that were added via document.write().
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionExcludeDocumentWriteParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_exclude_document_write",
                   true);

// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// low.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutLowFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_low_fetch_priority_hint",
    false);
// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// auto.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutAutoFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_auto_fetch_priority_hint",
    false);
// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// high.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutHighFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_high_fetch_priority_hint",
    true);

BASE_FEATURE(kMixedContentAutoupgrade,
             "AutoupgradeMixedContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheIntelligentPruning, base::FEATURE_DISABLED_BY_DEFAULT);

// Weight for the resource's type priority in the value calculation.
// A high default makes type a primary factor in determining importance.
BASE_FEATURE_PARAM(double,
                   kMemoryCacheIntelligentPruningFreqWeight,
                   &kMemoryCacheIntelligentPruning,
                   "freq_weight",
                   50.0);

// This weight is intentionally low to scale down the raw byte size. It ensures
// that cost acts as a secondary, tie-breaking factor and does not dominate
// the score compared to the more critical signals of resource type or
// frequency.
BASE_FEATURE_PARAM(double,
                   kMemoryCacheIntelligentPruningCostWeight,
                   &kMemoryCacheIntelligentPruning,
                   "cost_weight",
                   0.0001);

// Weight for the resource's usage frequency score in the value calculation.
// This is tuned to balance the logarithmic hit count against other factors.
BASE_FEATURE_PARAM(double,
                   kMemoryCacheIntelligentPruningTypeWeight,
                   &kMemoryCacheIntelligentPruning,
                   "type_weight",
                   100.0);

BASE_FEATURE(kMemoryCacheStrongReferenceExtensions,
             base::FEATURE_DISABLED_BY_DEFAULT);

// --- High Priority ---
BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefXSLStyleSheet,
                   &kMemoryCacheStrongReferenceExtensions,
                   "xsl_stylesheet",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefRaw,
                   &kMemoryCacheStrongReferenceExtensions,
                   "raw",
                   false);

// --- Medium Priority ---
BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefImage,
                   &kMemoryCacheStrongReferenceExtensions,
                   "image",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefSVGDocument,
                   &kMemoryCacheStrongReferenceExtensions,
                   "svg_document",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefManifest,
                   &kMemoryCacheStrongReferenceExtensions,
                   "manifest",
                   false);

// --- Low Priority ---
BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefAudio,
                   &kMemoryCacheStrongReferenceExtensions,
                   "audio",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefVideo,
                   &kMemoryCacheStrongReferenceExtensions,
                   "video",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefTextTrack,
                   &kMemoryCacheStrongReferenceExtensions,
                   "text_track",
                   false);

// --- Lowest Priority ---
BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefLinkPrefetch,
                   &kMemoryCacheStrongReferenceExtensions,
                   "link_prefetch",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefSpeculationRules,
                   &kMemoryCacheStrongReferenceExtensions,
                   "speculation_rules",
                   false);

BASE_FEATURE_PARAM(bool,
                   kMemoryCacheStrongRefDictionary,
                   &kMemoryCacheStrongReferenceExtensions,
                   "dictionary",
                   false);

BASE_FEATURE(kMemoryCacheStrongReference,
// Finch study showed no improvement on Android for strong memory cache.
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(int,
                   kMemoryCacheStrongReferenceTotalSizeThresholdParam,
                   &kMemoryCacheStrongReference,
                   "memory_cache_strong_ref_total_size_threshold",
                   15 * 1024 * 1024);
BASE_FEATURE_PARAM(int,
                   kMemoryCacheStrongReferenceResourceSizeThresholdParam,
                   &kMemoryCacheStrongReference,
                   "memory_cache_strong_ref_resource_size_threshold",
                   3 * 1024 * 1024);

BASE_FEATURE(kMemoryPurgeOnFreeze,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kMemoryPurgeOnFreezeLimit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemorySaverModeRenderTuning, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAvailableMemoryThresholdParamMb,
                   &kMemorySaverModeRenderTuning,
                   "available_memory_threshold_mb",
                   740);

BASE_FEATURE(kMHTML_Improvements, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded.
// Note that the desktop roll out is being done separately from android. See
// https://crbug.com/40258405
BASE_FEATURE(kNavigationPredictor, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPredictorTrafficClientEnabledPercent,
                   &kNavigationPredictor,
                   "traffic_client_enabled_percent",
#if BUILDFLAG(IS_ANDROID)
                   100
#else
                   5
#endif
);

// Used to control the collection of new viewport related anchor element
// metrics. Metrics will not be recorded if either this or kNavigationPredictor
// is disabled.
BASE_FEATURE(kNavigationPredictorNewViewportFeatures,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kNoForcedFrameUpdatesForWebTests,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoReferrerForPreloadFromSubresource,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoThrottlingVisibleAgent, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoThrowForCSPBlockedWorker, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOpenAllUrlsOrFilesOnDrop, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeHTMLElementUrls, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kDocumentURLCacheSize,
                   &kOptimizeHTMLElementUrls,
                   "cache_size",
                   500);

BASE_FEATURE(kOriginAgentClusterDefaultEnabled,
             "OriginAgentClusterDefaultEnable",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable defer commits to avoid flash of unstyled content, for all navigations.
BASE_FEATURE(kPaintHolding, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude CanvasFontCache from
// PartialLowModeOnMidRangeDevices. This is used to see how
// CanvasFontCache affects graphics smoothness and renderer memory usage.
BASE_FEATURE_PARAM(bool,
                   kPartialLowEndModeExcludeCanvasFontCache,
                   &base::features::kPartialLowEndModeOnMidRangeDevices,
                   "exclude-canvas-font-cache",
                   false);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

// Enables the use of the PaintCache for Path2D objects that are rasterized
// out of process.  Has no effect when kCanvasOopRasterization is disabled.
BASE_FEATURE(kPath2DPaintCache, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDedicatedWorkerAblationStudyEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kDedicatedWorkerStartDelayInMs,
                   &kDedicatedWorkerAblationStudyEnabled,
                   "DedicatedWorkerStartDelayInMs",
                   0);

BASE_FEATURE(kUseAncestorRenderFrameForWorker,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrecompileInlineScripts, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should composite a PLSA (paint layer scrollable area) even if it
// means losing lcd text.
BASE_FEATURE(kPreferCompositingToLCDText,
// On Android we never have LCD text. On Chrome OS we prefer composited
// scrolling for better scrolling performance.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrefetchFontLookupTables,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
#endif

// Launch mouse hover feature only on Desktop. Note that Android Desktop mode is
// currently out of scope.
BASE_FEATURE(kPreloadingEagerHoverHeuristics,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kPreloadingEagerHoverHeuristicsDwellTime,
                   &kPreloadingEagerHoverHeuristics,
                   "hover_dwell_time",
                   base::Milliseconds(10));
BASE_FEATURE(kPreloadingEagerViewportHeuristics,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kPreloadingEagerViewportHeuristicsPresentTime,
                   &kPreloadingEagerViewportHeuristics,
                   "viewport_present_time",
                   base::Milliseconds(100));

BASE_FEATURE(kPreloadingHeuristicsMLModel, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelTimerStartDelay,
                   &kPreloadingHeuristicsMLModel,
                   "timer_start_delay",
                   0);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelTimerInterval,
                   &kPreloadingHeuristicsMLModel,
                   "timer_interval",
                   100);
// The default max hover time of 10s covers the 98th percentile of hovering
// cases that are relevant to the model.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kPreloadingModelMaxHoverTime,
                   &kPreloadingHeuristicsMLModel,
                   "max_hover_time",
                   base::Seconds(10));
BASE_FEATURE_PARAM(bool,
                   kPreloadingModelEnactCandidates,
                   &kPreloadingHeuristicsMLModel,
                   "enact_candidates",
                   false);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelPrefetchModerateThreshold,
                   &kPreloadingHeuristicsMLModel,
                   "prefetch_moderate_threshold",
                   50);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelPrerenderModerateThreshold,
                   &kPreloadingHeuristicsMLModel,
                   "prerender_moderate_threshold",
                   50);

BASE_FEATURE(kPreloadingModerateViewportHeuristics,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Firing pagehide events for intended prerender cancellation. See
// crbug.com/353628449 for more details.
BASE_FEATURE(kPageHideEventForPrerender2, base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrerender2MaxNumOfRunningSpeculationRules[] =
    "max_num_of_running_speculation_rules";

BASE_FEATURE(kPrerender2MemoryControls, base::FEATURE_ENABLED_BY_DEFAULT);
const char kPrerender2MemoryThresholdParamName[] = "memory_threshold_in_mb";
const char kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName[] =
    "acceptable_percent_of_system_memory";

BASE_FEATURE(kPrerender2EarlyDocumentLifecycleUpdate,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Private Aggregation API.
BASE_FEATURE(kPrivateAggregationApi, base::FEATURE_ENABLED_BY_DEFAULT);

// Selectively allows the JavaScript API to be disabled in just one of the
// contexts. The Protected Audience param's name has not been updated (from
// "fledge") for consistency across versions
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiEnabledInSharedStorage,
                   &kPrivateAggregationApi,
                   "enabled_in_shared_storage",
                   /*default_value=*/true);
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiEnabledInProtectedAudience,
                   &kPrivateAggregationApi,
                   "enabled_in_fledge",
                   /*default_value=*/true);

// Selectively allows the debug mode to be disabled while leaving the rest of
// the API in place. If disabled, any `enableDebugMode()` calls will essentially
// have no effect.
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiDebugModeEnabledAtAll,
                   &kPrivateAggregationApi,
                   "debug_mode_enabled_at_all",
                   /*default_value=*/true);

// Adds some additional functionality (new reserved event types, base values)
// to things enabled by
// kPrivateAggregationApiEnabledInProtectedAudience.
BASE_FEATURE(kPrivateAggregationApiProtectedAudienceAdditionalExtensions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProcessHtmlDataImmediately, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyChildFrame,
                   &kProcessHtmlDataImmediately,
                   "child",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyFirstChunk,
                   &kProcessHtmlDataImmediately,
                   "first",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyMainFrame,
                   &kProcessHtmlDataImmediately,
                   "main",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelySubsequentChunks,
                   &kProcessHtmlDataImmediately,
                   "rest",
                   false);

BASE_FEATURE(kForceProduceCompileHints, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLocalCompileHints, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuoteEmptySecChUaStringHeadersConsistently,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reduce the amount of information in the default 'referer' header for
// cross-origin requests.
BASE_FEATURE(kReducedReferrerGranularity, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRefactorCompositorThreadEventQueue,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kUserAgentFrozenBuildVersion,
                   &kReduceUserAgentMinorVersion,
                   "build_version",
                   "0");

// Whether `blink::MemoryCache` and `blink::ResourceFetcher` release their
// strong references to resources on memory pressure.
BASE_FEATURE(kReleaseResourceStrongReferencesOnMemoryPressure,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether `blink::Resource` deletes its decoded data on memory pressure.
BASE_FEATURE(kReleaseResourceDecodedDataOnMemoryPressure,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Flag guard for removing usage of the CommitNavigationParams.redirects
// array of URLs in the renderer process.
BASE_FEATURE(kRemoveCommitRedirectUrlsArray, base::FEATURE_ENABLED_BY_DEFAULT);

// Disables sending the Purpose: "prefetch" header for prefetches and
// prerenders.
BASE_FEATURE(kRemovePurposeHeaderForPrefetch, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRenderBlockingFonts, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kMaxBlockingTimeMsForRenderBlockingFonts,
                   &features::kRenderBlockingFonts,
                   "max-blocking-time",
                   1500);

BASE_FEATURE_PARAM(int,
                   kMaxFCPDelayMsForRenderBlockingFonts,
                   &features::kRenderBlockingFonts,
                   "max-fcp-delay",
                   100);

BASE_FEATURE(kRenderSizeInScoreAdBrowserSignals,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingInputEvents, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingScrollEvents, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictLinkHeaderOnSubresource,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kRestrictLinkHeaderOnSubresourceCompressionDictionary,
                   &kRestrictLinkHeaderOnSubresource,
                   "disable_compression_dictionary",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictLinkHeaderOnSubresourceCrossOrigin,
                   &kRestrictLinkHeaderOnSubresource,
                   "disable_cross_origin",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictLinkHeaderOnSubresourceNetworkHint,
                   &kRestrictLinkHeaderOnSubresource,
                   "disable_network_hint",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictLinkHeaderOnSubresourceResourceLoad,
                   &kRestrictLinkHeaderOnSubresource,
                   "disable_resource_load",
                   false);

BASE_FEATURE(kRestrictSpellingAndGrammarHighlights,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedContents,
                   &kRestrictSpellingAndGrammarHighlights,
                   "RestrictSpellingAndGrammarHighlightsChangedContents",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedEnablement,
                   &kRestrictSpellingAndGrammarHighlights,
                   "RestrictSpellingAndGrammarHighlightsChangedEnablement",
                   false);
BASE_FEATURE_PARAM(bool,
                   kRestrictSpellingAndGrammarHighlightsChangedSelection,
                   &kRestrictSpellingAndGrammarHighlights,
                   "RestrictSpellingAndGrammarHighlightsChangedSelection",
                   false);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistPaytoToRegisterProtocolHandler,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPausePagesPerBrowsingContextGroup,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowHudDisplayForPausedPages, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls script streaming for http and https scripts.
BASE_FEATURE(kScriptStreaming, base::FEATURE_ENABLED_BY_DEFAULT);
// Enables script streaming for non-http scripts.
BASE_FEATURE(kScriptStreamingForNonHTTP,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enables sending Sec-Purpose: "prefetch" header for
// NoStatePrefetchURLLoaderThrottle.
BASE_FEATURE(kSecPurposePrefetchHeaderNoStatePrefetch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending Sec-Purpose: "prefetch" header for rel="prefetch".
BASE_FEATURE(kSecPurposePrefetchHeaderRelPrefetch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the SubresourceFilter receives calls from the ResourceLoader
// to perform additional checks against any aliases found from DNS CNAME records
// for the requested URL.
BASE_FEATURE(kSendCnameAliasesToSubresourceFilterFromRenderer,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calling setInterval(..., 0) will not clamp to 1ms.
// Tracking bug: https://crbug.com/402694.
BASE_FEATURE(kSetIntervalWithoutClamp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageWorkletSharedBackingThreadImplementation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageCreateWorkletCustomDataOrigin,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageSelectURLSavedQueries,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageAPIEnableWALForDatabase,
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

BASE_FEATURE(kSpeculativeImageDecodes, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable service worker warming-up feature. (https://crbug.com/1431792)
BASE_FEATURE(kSpeculativeServiceWorkerWarmUp, base::FEATURE_ENABLED_BY_DEFAULT);

// kSpeculativeServiceWorkerWarmUp warms up service workers up to this max
// count.
BASE_FEATURE_PARAM(int,
                   kSpeculativeServiceWorkerWarmUpMaxCount,
                   &kSpeculativeServiceWorkerWarmUp,
                   "sw_warm_up_max_count",
                   2);

// Duration to keep worker warmed-up.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSpeculativeServiceWorkerWarmUpDuration,
                   &kSpeculativeServiceWorkerWarmUp,
                   "sw_warm_up_duration",
                   base::Minutes(5));

// Warms up service workers when a pointerover event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerover{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerover", true};

// Warms up service workers when a pointerdown event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerdown{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerdown", true};

// (crbug.com/352578800): Enables building a sysnthetic response by
// ServiceWorker. For navigation requests, the pre-learned static response
// header is returned in parallel with dispatching the network request.
BASE_FEATURE(kServiceWorkerSyntheticResponse,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Define the allowed websites to enable SyntheticResponse. Allowed urls are
// expected to be passed as a comma separated string.
// e.g. https://example1.test,https://example2.test/foo?query
BASE_FEATURE_PARAM(std::string,
                   kServiceWorkerSyntheticResponseAllowedUrl,
                   &kServiceWorkerSyntheticResponse,
                   "allowed_url",
                   "");

// The comma-separated URL parameters that explains non-eligible for the
// synthetic response.
BASE_FEATURE_PARAM(std::string,
                   kServiceWorkerSyntheticResponseDeniedUrlParams,
                   &kServiceWorkerSyntheticResponse,
                   "denied_url_params",
                   "");

// The comma-separated list of headers to be ignored for the consistency check.
BASE_FEATURE_PARAM(std::string,
                   kServiceWorkerSyntheticResponseIgnoredHeaders,
                   &kServiceWorkerSyntheticResponse,
                   "ignored_headers",
                   "date,alt-svc,p3p,strict-transport-security");

// If true, the browser reports crashes via `DumpWithoutCrashing()` when theare
// was a header mismatch.
BASE_FEATURE_PARAM(bool,
                   kServiceWorkerSyntheticResponseReportInconsistentHeader,
                   &kServiceWorkerSyntheticResponse,
                   "report_inconsistent_header",
                   false);

// If true, the browser enables synthetic response with the dry run mode. With
// this mode, the navigation request is involved with the service worker code
// path, and the synthetic response eligiblity is evaluated as if the feature is
// enabled. But it doesn't store response headers and actually "synthesize"
// responses with them. This mode is used to compare the metrics.
BASE_FEATURE_PARAM(bool,
                   kServiceWorkerSyntheticResponseDryRun,
                   &kServiceWorkerSyntheticResponse,
                   "dry_run",
                   false);

// If true, the service worker for synthetic response doesn't intercept
// subresources.
BASE_FEATURE_PARAM(bool,
                   kServiceWorkerSyntheticResponseBypassSubresource,
                   &kServiceWorkerSyntheticResponse,
                   "bypass_subresource",
                   true);

// If enabled, force renderer process foregrounded from CommitNavigation to
// DOMContentLoad (crbug/351953350).
BASE_FEATURE(
    kBoostRenderProcessForLoading,
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/351953350): Enable this feature on Android as well after
    // confirming that this feature doesn't regress anything.
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// An empty json array means that this feature is applied unconditionally. If
// specified, it means that the specified URLs will be the target of the new
// behavior.
BASE_FEATURE_PARAM(std::string,
                   kBoostRenderProcessForLoadingTargetUrls,
                   &kBoostRenderProcessForLoading,
                   "target_urls",
                   "[]");

// If true is specified, kBoostRenderProcessForLoading feature also prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerendering,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering",
                   true);

// If true is specified, kBoostRenderProcessForLoading feature only prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerenderingOnly,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering_only",
                   false);

// If true is specified, kBoostRenderProcessForLoading feature also prioritizes
// the renderer process for restore cases.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizeRestore,
                   &kBoostRenderProcessForLoading,
                   "prioritize_restore",
                   false);

// Bypasses the enforcement of fetch() requests that set HTTP forbidden headers
// (https://developer.mozilla.org/en-US/docs/Glossary/Forbidden_request_header)
// when the context has origin access to the fetch() target.
// TODO(crbug.com/418811955): This only controls the renderer side now. Expand
// to also have this control the browser side.
BASE_FEATURE(kBypassRequestForbiddenHeadersCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
BASE_FEATURE(kStopInBackground,
             "stop-in-background",
// b/248036988 - Disable this for Chromecast on Android builds to prevent apps
// that play audio in the background from stopping.
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID) && \
    !BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Reduces the work done during renderer initialization.
BASE_FEATURE(kStreamlineRendererInit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSubSampleWindowProxyUsageMetrics,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSupportOpeningDraggedLinksInSameTab,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTaskAttributionTraceMicrotaskTaskState,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedBodyLoader, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedPreloadScanner, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kThrottleFrameRateOnInitialization,
                   &features::kRenderBlockingFullFrameRate,
                   "throttle-frame-rate-on-initialization",
                   false);

// Enable throttling of fetch() requests from service workers in the
// installing state.  The limit of 3 was chosen to match the limit
// in background main frames.  In addition, trials showed that this
// did not cause excessive timeouts and resulted in a net improvement
// in successful install rate on some platforms.
BASE_FEATURE(kThrottleInstallingServiceWorker,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kInstallingServiceWorkerOutstandingThrottledLimit,
                   &kThrottleInstallingServiceWorker,
                   "limit",
                   3);

// Throttles Javascript timer wake ups of unimportant frames (cross origin
// frames with small proportion of the page's visible area and no user
// activation) on foreground pages.
BASE_FEATURE(kThrottleUnimportantFrameTimers,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Interval between Javascript timer wake ups for unimportant frames (small
// cross origin frames with no user activation) when the
// "ThrottleUnimportantFrameTimers" feature is enabled.
BASE_FEATURE_PARAM(int,
                   kUnimportantFrameTimersThrottledWakeUpIntervalMills,
                   &features::kThrottleUnimportantFrameTimers,
                   "unimportant_frame_timers_throttled_wake_up_interval_millis",
                   32);
// The percentage of the page's visible area below which a frame is considered
// small. Only small frames can be throttled by ThrottleUnimportantFrameTimers.
BASE_FEATURE_PARAM(int,
                   kLargeFrameSizePercentThreshold,
                   &features::kThrottleUnimportantFrameTimers,
                   "large_frame_size_percent_threshold",
                   75);

// Changes behavior of User-Agent Client Hints to send blank headers when the
// User-Agent string is overridden, instead of disabling the headers altogether.
BASE_FEATURE(kUACHOverrideBlank, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the body of `EmulateLoadStartedForInspector` is executed only
// once per Resource per ResourceFetcher, and thus duplicated network load
// entries in DevTools caused by `EmulateLoadStartedForInspector` are removed.
// https://crbug.com/1502591
BASE_FEATURE(kEmulateLoadStartedForInspectorOncePerResource,
             "kEmulateLoadStartedForInspectorOncePerResource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether force-showing popovers is enabled.
BASE_FEATURE(kDevToolsAllowPopoverForcing, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the usage of unload handlers causes a blocklisted reason for
// BFCache. The purpose is to capture their source location.
BASE_FEATURE(kUnloadBlocklisted, base::FEATURE_DISABLED_BY_DEFAULT);

// When BeginMainFrame() is throttled, whether input-related BeginMainFrame()s
// are marked urgent, and thus unthtrottled.
//
// Enabled on Android, since a field trial showed benefits.
BASE_FEATURE(kUrgentMainFrameForInput,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, URLPattern will use standard defined dummy URL canonicalization
// to canonicalize URL properties. See https://crbug.com/409350827
BASE_FEATURE(kURLPatternDummyURLCanonicalization,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BASE_FEATURE(kUsePageViewportInLCP, base::FEATURE_ENABLED_BY_DEFAULT);

// Use PersistentCache on either side of blink.mojom.CodeCacheHost. This feature
// is dependent on net::HttpCache::IsSplitCacheEnabled() being true. Always use
// IsPersistentCacheForCodeCacheEnabled() rather than querying this feature
// directly.
BASE_FEATURE(kUsePersistentCacheForCodeCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabling this will cause parkable strings to use Snappy for compression iff
// kCompressParkableStrings is enabled.
BASE_FEATURE(kUseSnappyForParkableStrings, base::FEATURE_DISABLED_BY_DEFAULT);

// Use the zstd compression algorithm for ParkableString compression.
BASE_FEATURE(kUseZstdForParkableStrings, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows to tweak the compression / speed tradeoff.
//
// According to https://github.com/facebook/zstd, level 1 should be:
// - Much faster than zlib, with a similar compression ratio
// - Roughly as fast as snappy, with a better compression ratio.
//
// And even -3 should be smaller *and* faster than snappy.
BASE_FEATURE_PARAM(int,
                   kZstdCompressionLevel,
                   &features::kUseZstdForParkableStrings,
                   "compression_level",
                   1);

BASE_FEATURE(kVSyncDecoding, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVSyncDecodingHiddenOccludedTickDuration,
                   &kVSyncDecoding,
                   "occluded_tick_duration",
                   base::Hertz(10));

BASE_FEATURE(kVSyncEncoding, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebBluetoothCancelConnect,
// TODO(382556910): Enable on Windows when DCHECK issue is resolved.
// TODO(40502943): Enable on Android when connect callback can be called when
// cancelled.
// GATT connect on Windows/Android will timeout after a few seconds if the
// device is unreachable, so it does not have hang issue like MacOS which
// definitely needs cancel to get from the hang state.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kWebRtcUseCaptureBeginTimestamp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcAudioSinkUseTimestampAligner,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcPqcForDtls, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable borderless mode for desktop PWAs. go/borderless-mode
BASE_FEATURE(kWebAppBorderless, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls scope extensions feature in web apps. Enables parsing of "site"
// entries in "scope_extensions" field in web app manifests. See explainer for
// more information:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
BASE_FEATURE(kWebAppEnableScopeExtensionsBySite,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls parsing of the "lock_screen" dictionary field and its "start_url"
// entry in web app manifests.  See explainer for more information:
// https://github.com/WICG/lock-screen/
// Note: the lock screen API and OS integration is separately controlled by
// the content feature `kWebLockScreenApi`.
BASE_FEATURE(kWebAppManifestLockScreen, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables web apps to be migrated from one manifest id to another.
BASE_FEATURE(kWebAppMigrationApi, base::FEATURE_DISABLED_BY_DEFAULT);

// Allow denormals in AudioWorklet and ScriptProcessorNode, to enable strict
// JavaScript denormal compliance.  See https://crbug.com/382005099.
BASE_FEATURE(kWebAudioAllowDenormalInProcessing,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use deferred pull status update instead of updating the status directly
// on audio thread. See https://crbug.com/40249972.
BASE_FEATURE(kWebAudioDeferPullStatusUpdate, base::FEATURE_DISABLED_BY_DEFAULT);

/// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
BASE_FEATURE(kWebFontsCacheAwareTimeoutAdaption,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
BASE_FEATURE(kWebRtcHideLocalIpsWithMdns, base::FEATURE_ENABLED_BY_DEFAULT);

// Causes WebRTC to not set the color space of video frames on the receive side
// in case it's unspecified. Otherwise we will guess that the color space is
// BT709. http://crbug.com/1129243
BASE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Instructs WebRTC to honor the Min/Max Video Encode Accelerator dimensions.
BASE_FEATURE(kWebRtcUseMinMaxVEADimensions,
// TODO(crbug.com/1008491): enable other platforms.
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for crbug.com/407785197.
BASE_FEATURE(kWebRtcAllowDataChannelRecordingInWebrtcInternals,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for https://crbug.com/338955051.
BASE_FEATURE(kWebUSBTransferSizeLimit, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables small accelerated canvases for webview (crbug.com/1004304)
BASE_FEATURE(kWebviewAccelerateSmallCanvases,
             base::FEATURE_DISABLED_BY_DEFAULT);

// WorkerThread termination procedure (prepare and shutdown) runs sequentially
// in the same task without calling another cross thread post task.
// Kill switch for crbug.com/409059706.
BASE_FEATURE(kWorkerThreadSequentialShutdown, base::FEATURE_ENABLED_BY_DEFAULT);

// WorkerThread termination respects the current thread termination request.
BASE_FEATURE(kWorkerThreadRespectTermRequest, base::FEATURE_ENABLED_BY_DEFAULT);

// Indicates that renderer is running on an Android XR (AR/VR) device.
// Enables certain features which are not needed on other platforms.
BASE_FEATURE(kXrDevice, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the 'unframed' display override for IWAs. go/unframed-explainer-doc.
BASE_FEATURE(kUnframedIwa, base::FEATURE_DISABLED_BY_DEFAULT);

// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When defining feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// ---------------------------------------------------------------------------
// Helper functions for querying feature status. Please define any features or
// constants for features in the section above.

bool IsAllowURNsInIframeEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kAllowURNsInIframes);
}

bool IsCanvas2DHibernationEnabled() {
  return base::FeatureList::IsEnabled(features::kCanvas2DHibernation);
}

bool DisplayWarningDeprecateURNIframesUseFencedFrames() {
  return base::FeatureList::IsEnabled(
      blink::features::kDisplayWarningDeprecateURNIframesUseFencedFrames);
}

bool IsFencedFramesEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kFencedFrames);
}

bool IsParkableStringsToDiskEnabled() {
  // Always enabled as soon as compression is enabled.
  return base::FeatureList::IsEnabled(kCompressParkableStrings);
}

bool IsPersistentCacheForCodeCacheEnabled() {
  // PersistentCache for CodeCache requires HTTP split cache.
  return net::HttpCache::IsSplitCacheEnabled() &&
         base::FeatureList::IsEnabled(kUsePersistentCacheForCodeCache);
}

bool IsSetIntervalWithoutClampEnabled() {
  return base::FeatureList::IsEnabled(features::kSetIntervalWithoutClamp);
}

bool IsUnloadBlocklisted() {
  return base::FeatureList::IsEnabled(kUnloadBlocklisted);
}

bool ParkableStringsUseSnappy() {
  return base::FeatureList::IsEnabled(kUseSnappyForParkableStrings);
}

bool IsKeepAliveURLLoaderServiceEnabled() {
  return base::FeatureList::IsEnabled(kKeepAliveInBrowserMigration) ||
         base::FeatureList::IsEnabled(kFetchLaterAPI);
}

bool IsLinkPreviewTriggerTypeEnabled(LinkPreviewTriggerType type) {
  return base::FeatureList::IsEnabled(blink::features::kLinkPreview) &&
         type == blink::features::kLinkPreviewTriggerType.Get();
}

bool IsXrDevice() {
  return base::FeatureList::IsEnabled(blink::features::kXrDevice);
}

// DO NOT ADD NEW FEATURES HERE.
//
// The section above is for helper functions for querying feature status. The
// section below should have nothing. Please add new features in the giant block
// of features that already exist in this file, trying to keep newly-added
// features in sorted order.
//
// DO NOT ADD NEW FEATURES HERE.

}  // namespace blink::features
