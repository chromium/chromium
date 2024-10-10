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
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
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

// Enable the Protected Audience's reporting with ad macro API.
BASE_FEATURE(kAdAuctionReportingWithMacroApi,
             "AdAuctionReportingWithMacroApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BASE_FEATURE(kAdAuctionSignals,
             "AdAuctionSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAdAuctionSignalsMaxSizeBytes,
                   &kAdAuctionSignals,
                   "ad-auction-signals-max-size-bytes",
                   10000);

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Changes default Permissions Policy for features join-ad-interest-group and
// run-ad-auction to a more restricted EnableForSelf.
BASE_FEATURE(kAdInterestGroupAPIRestrictedPolicyByDefault,
             "AdInterestGroupAPIRestrictedPolicyByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow DeprecatedRenderURLReplacements when
// CookieDeprecationFacilitatedTesting is enabled.
BASE_FEATURE(kAlwaysAllowFledgeDeprecatedRenderURLReplacements,
             "kAlwaysAllowFledgeDeprecatedRenderURLReplacements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Block all MIDI access with the MIDI_SYSEX permission
BASE_FEATURE(kBlockMidiByDefault,
             "BlockMidiByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComputePressureRateObfuscationMitigation,
             "ComputePressureRateObfuscationMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLowerHighResolutionTimerThreshold,
             "LowerHighResolutionTimerThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Make all pending 'display: auto' web fonts enter the swap or failure period
// immediately before reaching the LCP time limit (~2500ms), so that web fonts
// do not become a source of bad LCP.
BASE_FEATURE(kAlignFontDisplayAutoTimeoutWithLCPGoal,
             "AlignFontDisplayAutoTimeoutWithLCPGoal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The amount of time allowed for 'display: auto' web fonts to load without
// intervention, counted from navigation start.
BASE_FEATURE_PARAM(int,
                   kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam,
                   &kAlignFontDisplayAutoTimeoutWithLCPGoal,
                   "lcp-limit-in-ms",
                   2000);

const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>::Option
    align_font_display_auto_timeout_with_lcp_goal_modes[] = {
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToFailurePeriod,
         "failure"},
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod, "swap"}};
BASE_FEATURE_ENUM_PARAM(
    AlignFontDisplayAutoTimeoutWithLCPGoalMode,
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam,
    &kAlignFontDisplayAutoTimeoutWithLCPGoal,
    "intervention-mode",
    AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod,
    &align_font_display_auto_timeout_with_lcp_goal_modes);

BASE_FEATURE(kAllowDatapipeDrainedAsBytesConsumerInBFCache,
             "AllowDatapipeDrainedAsBytesConsumerInBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDevToolsMainThreadDebuggerForMultipleMainFrames,
             "AllowDevToolsMainThreadDebuggerForMultipleMainFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BASE_FEATURE(kAllowDropAlphaForMediaStream,
             "AllowDropAlphaForMediaStream",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowSourceSwitchOnPausedVideoMediaStream,
             "AllowSourceSwitchOnPausedVideoMediaStream",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables URN URLs like those produced by Protected Audience auctions to be
// displayed by iframes (instead of requiring fenced frames).
BASE_FEATURE(kAllowURNsInIframes,
             "AllowURNsInIframes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A console warning is shown when the opaque url returned from Protected
// Audience/selectUrl is used to navigate an iframe. Since fenced frames are not
// going to be enforced for these APIs in the short-medium term, disabling this
// warning for now.
BASE_FEATURE(kDisplayWarningDeprecateURNIframesUseFencedFrames,
             "DisplayWarningDeprecateURNIframesUseFencedFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Anchor Element Interaction
BASE_FEATURE(kAnchorElementInteraction,
             "AnchorElementInteraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidExtendedKeyboardShortcuts,
             "AndroidExtendedKeyboardShortcuts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A server-side switch for the kRealtimeAudio thread type of
// RealtimeAudioWorkletThread object. This can be controlled by a field trial,
// it will use the kNormal type thread when disabled.
BASE_FEATURE(kAudioWorkletThreadRealtimePriority,
             "AudioWorkletThreadRealtimePriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
// When enabled, RealtimeAudioWorkletThread scheduling is optimized taking into
// account how often the worklet logic is executed (which is determined by the
// AudioContext buffer duration).
BASE_FEATURE(kAudioWorkletThreadRealtimePeriodMac,
             "AudioWorkletThreadRealtimePeriodMac",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A thread pool system for effective usage of RealtimeAudioWorkletThread
// instances.
BASE_FEATURE(kAudioWorkletThreadPool,
             "AudioWorkletThreadPool",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If disabled (default for many years), autofilling triggers KeyDown and
// KeyUp events that do not send any key codes. If enabled, these events
// contain the "Unidentified" key.
BASE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill,
             "AutofillSendUnidentifiedKeyAfterFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// https://crbug.com/1472970
BASE_FEATURE(kAutoSpeculationRules,
             "AutoSpeculationRules",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutoSpeculationRulesHoldback{
    &kAutoSpeculationRules, "holdback", false};

BASE_FEATURE(kAvifGainmapHdrImages,
             "AvifGainmapHdrImages",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidForcedLayoutOnInitialEmptyDocumentInSubframe,
             "AvoidForcedLayoutOnInitialEmptyDocumentInSubframe",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBFCacheOpenBroadcastChannel,
             "BFCacheOpenBroadcastChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackForwardCacheDWCOnJavaScriptExecution,
             "BackForwardCacheDWCOnJavaScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows pages with keepalive requests to stay eligible for the back/forward
// cache. See https://crbug.com/1347101 for more details.
BASE_FEATURE(kBackForwardCacheWithKeepaliveRequest,
             "BackForwardCacheWithKeepaliveRequest",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable background resource fetch in Blink. See https://crbug.com/1379780 for
// more details.
BASE_FEATURE(kBackgroundResourceFetch,
             "BackgroundResourceFetch",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
BASE_FEATURE(kBakedGamutMapping,
             "BakedGamutMapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BASE_FEATURE(kBackgroundTracingPerformanceMark,
             "BackgroundTracingPerformanceMark",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList{
        &kBackgroundTracingPerformanceMark, "allow_list", ""};

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Feature flag to enable debug reporting APIs.
BASE_FEATURE(kBiddingAndScoringDebugReportingAPI,
             "BiddingAndScoringDebugReportingAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BASE_FEATURE(kBoostImagePriority,
             "BoostImagePriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The number of images to bopost the priority of before returning
// to the default (low) priority.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityImageCount,
                   &kBoostImagePriority,
                   "image_count",
                   5);
// Maximum size of an image (in px^2) to be considered "small".
// Small images, where dimensions are specified in the markup, are not boosted.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityImageSize,
                   &kBoostImagePriority,
                   "image_size",
                   10000);
// Number of medium-priority requests to allow in tight-mode independent of the
// total number of outstanding requests.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityTightMediumLimit,
                   &kBoostImagePriority,
                   "tight_medium_limit",
                   2);

// Boost the priority of certain loading tasks (https://crbug.com/1470003).
BASE_FEATURE(kBoostImageSetLoadingTaskPriority,
             "BoostImageSetLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostFontLoadingTaskPriority,
             "BoostFontLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostVideoLoadingTaskPriority,
             "BoostVideoLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostRenderBlockingStyleLoadingTaskPriority,
             "BoostRenderBlockingStyleLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostNonRenderBlockingStyleLoadingTaskPriority,
             "BoostNonRenderBlockingStyleLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://github.com/patcg-individual-drafts/topics
// Kill switch for the Topics API.
BASE_FEATURE(kBrowsingTopics,
             "BrowsingTopics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the check for whether the IP address is publicly routable will be
// bypassed when determining the eligibility for a page to be included in topics
// calculation. This is useful for developers to test in local environment.
BASE_FEATURE(kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
             "BrowsingTopicsBypassIPIsPubliclyRoutableCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables calling the Topics API through Javascript (i.e.
// document.browsingTopics()). For this feature to take effect, the main Topics
// feature has to be enabled first (i.e. `kBrowsingTopics` is enabled, and,
// either a valid Origin Trial token exists or `kPrivacySandboxAdsAPIsOverride`
// is enabled.)
BASE_FEATURE(kBrowsingTopicsDocumentAPI,
             "BrowsingTopicsDocumentAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Decoupled with the main `kBrowsingTopics` feature, so it allows us to
// decouple the server side configs.
BASE_FEATURE(kBrowsingTopicsParameters,
             "BrowsingTopicsParameters",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
const base::FeatureParam<std::string> kBrowsingTopicsDisabledTopicsList{
    &kBrowsingTopicsParameters, "disabled_topics_list", ""};
// Comma separated list of Topic IDs. Prioritize these topics and their
// descendants during top topic selection.
const base::FeatureParam<std::string> kBrowsingTopicsPrioritizedTopicsList{
    &kBrowsingTopicsParameters, "prioritized_topics_list",
    "57,86,126,149,172,180,196,207,239,254,263,272,289,299,332"};
// When a topics calculation times out for the first time, the duration to wait
// before starting a new one.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsFirstTimeoutRetryDelay,
                   &kBrowsingTopicsParameters,
                   "first_timeout_retry_delay",
                   base::Minutes(1));

// When enabled, code cache is produced asynchronously from the script execution
// (https://crbug.com/1260908).
BASE_FEATURE(kCacheCodeOnIdle,
             "CacheCodeOnIdle",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kCacheCodeOnIdleDelayParam,
                   &kCacheCodeOnIdle,
                   "delay-in-ms",
                   1);
// Apply CacheCodeOnIdle only for service workers (https://crbug.com/1410082).
BASE_FEATURE_PARAM(bool,
                   kCacheCodeOnIdleDelayServiceWorkerOnlyParam,
                   &kCacheCodeOnIdle,
                   "service-worker-only",
                   true);

// When enabled allows the header name used in the blink
// CacheStorageCodeCacheHint runtime feature to be modified.  This runtime
// feature disables generating full code cache for responses stored in
// cache_storage during a service worker install event.  The runtime feature
// must be enabled via the blink runtime feature mechanism, however.
BASE_FEATURE(kCacheStorageCodeCacheHintHeader,
             "CacheStorageCodeCacheHintHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kCacheStorageCodeCacheHintHeaderName{
    &kCacheStorageCodeCacheHintHeader, "name", "x-CacheStorageCodeCacheHint"};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Enables camera preview in permission bubble and site settings.
BASE_FEATURE(kCameraMicPreview,
             "CameraMicPreview",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Temporarily disabled due to issues:
// - PDF blank previews
// - Canvas corruption on ARM64 macOS
// See https://g-issues.chromium.org/issues/328755781
BASE_FEATURE(kCanvas2DHibernation,
             "Canvas2DHibernation",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// When hibernating, make sure that the just-used transfer memory (to transfer
// the snapshot) is freed.
BASE_FEATURE(kCanvas2DHibernationReleaseTransferMemory,
             "Canvas2DHibernationReleaseTransferMemory",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCanvasSharedBitmapToSharedImage,
             "CanvasSharedBitmapToSharedImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to capture the source location of JavaScript execution, which is one
// of the renderer eviction reasons for Back/Forward Cache.
BASE_FEATURE(kCaptureJSExecutionLocation,
             "CaptureJSExecutionLocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckHTMLParserBudgetLessOften,
             "CheckHTMLParserBudgetLessOften",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable legacy `dpr` client hint.
BASE_FEATURE(kClientHintsDPR_DEPRECATED,
             "ClientHintsDPR_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `device-memory` client hint.
BASE_FEATURE(kClientHintsDeviceMemory_DEPRECATED,
             "ClientHintsDeviceMemory_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `width` client hint.
BASE_FEATURE(kClientHintsResourceWidth_DEPRECATED,
             "ClientHintsResourceWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `form-factor` client hint for XR devices.
BASE_FEATURE(kClientHintsXRFormFactor,
             "ClientHintsXRFormFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable legacy `viewport-width` client hint.
BASE_FEATURE(kClientHintsViewportWidth_DEPRECATED,
             "ClientHintsViewportWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
BASE_FEATURE(kCompressParkableStrings,
             "CompressParkableStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables more conservative settings for ParkableString: suspend parking in
// foreground, and increase aging tick intervals.
BASE_FEATURE(kLessAggressiveParkableString,
             "LessAggressiveParkableString",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Limits maximum capacity of disk data allocator per renderer process.
// DiskDataAllocator and its clients(ParkableString, ParkableImage) will try
// to keep the limitation.
const base::FeatureParam<int> kMaxDiskDataAllocatorCapacityMB{
    &kCompressParkableStrings, "max_disk_capacity_mb", -1};

// Controls off-thread code cache consumption.
BASE_FEATURE(kConsumeCodeCacheOffThread,
             "ConsumeCodeCacheOffThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the constant streaming in the ContentCapture task.
BASE_FEATURE(kContentCaptureConstantStreaming,
             "ContentCaptureConstantStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCorrectFloatExtensionTestForWebGL,
             "CorrectFloatExtensionTestForWebGL",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCrabbyAvif, "CrabbyAvif", base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, add a new option, {imageOrientation: 'none'}, to
// createImageBitmap, which ignores the image orientation metadata of the source
// and renders the image as encoded.
BASE_FEATURE(kCreateImageBitmapOrientationNone,
             "CreateImageBitmapOrientationNone",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, pages that don't specify a layout width will default to the
// window width rather than the traditional mobile fallback width of 980px.
// Has no effect unless viewport handling is enabled.
BASE_FEATURE(kDefaultViewportIsDeviceWidth,
             "DefaultViewportIsDeviceWidth",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeferRendererTasksAfterInput,
             "DeferRendererTasksAfterInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
                        TaskDeferralPolicy::kAllDeferrableTypes,
                        &kTaskDeferralOptions);

BASE_FEATURE(kDelayAsyncScriptExecution,
             "DelayAsyncScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<DelayAsyncScriptDelayType>::Option
    delay_async_script_execution_delay_types[] = {
        {DelayAsyncScriptDelayType::kFinishedParsing, "finished_parsing"},
        {DelayAsyncScriptDelayType::kFirstPaintOrFinishedParsing,
         "first_paint_or_finished_parsing"},
        {DelayAsyncScriptDelayType::kEachLcpCandidate, "each_lcp_candidate"},
        {DelayAsyncScriptDelayType::kEachPaint, "each_paint"},
        {DelayAsyncScriptDelayType::kTillFirstLcpCandidate,
         "till_first_lcp_candidate"},
};

BASE_FEATURE_ENUM_PARAM(DelayAsyncScriptDelayType,
                        kDelayAsyncScriptExecutionDelayParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_delay_type",
                        DelayAsyncScriptDelayType::kFinishedParsing,
                        &delay_async_script_execution_delay_types);

const base::FeatureParam<DelayAsyncScriptTarget>::Option
    delay_async_script_target_types[] = {
        {DelayAsyncScriptTarget::kAll, "all"},
        {DelayAsyncScriptTarget::kCrossSiteOnly, "cross_site_only"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowList,
         "cross_site_with_allow_list"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowListReportOnly,
         "cross_site_with_allow_list_report_only"},
};
BASE_FEATURE_ENUM_PARAM(DelayAsyncScriptTarget,
                        kDelayAsyncScriptTargetParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_target",
                        DelayAsyncScriptTarget::kAll,
                        &delay_async_script_target_types);

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

const base::FeatureParam<std::string> kDelayAsyncScriptAllowList{
    &kDelayAsyncScriptExecution, "delay_async_exec_allow_list", ""};

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
                        "delay_async_exec_target",
                        AsyncScriptExperimentalSchedulingTarget::kBoth,
                        &async_script_experimental_scheduling_targets);

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
                   false);

BASE_FEATURE(kDelayLowPriorityRequestsAccordingToNetworkState,
             "DelayLowPriorityRequestsAccordingToNetworkState",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxNumOfThrottleableRequestsInTightMode{
    &kDelayLowPriorityRequestsAccordingToNetworkState,
    "MaxNumOfThrottleableRequestsInTightMode", 5};

BASE_FEATURE_PARAM(base::TimeDelta,
                   kHttpRttThreshold,
                   &kDelayLowPriorityRequestsAccordingToNetworkState,
                   "HttpRttThreshold",
                   base::Milliseconds(450));

const base::FeatureParam<double> kCostReductionOfMultiplexedRequests{
    &kDelayLowPriorityRequestsAccordingToNetworkState,
    "CostReductionOfMultiplexedRequests", 0.5};

BASE_FEATURE(kDirectCompositorThreadIpc,
             "DirectCompositorThreadIpc",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableArrayBufferSizeLimitsForTesting,
             "DisableArrayBufferSizeLimitsForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardInputEventsToRecentlyMovedFrames,
             "DiscardInputEventsToRecentlyMovedFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableThirdPartyStoragePartitioningDeprecationTrial2,
             "DisableThirdPartyStoragePartitioningDeprecationTrial2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for not WebGL DrawingBuffer using SharedImage without
// GpuMemoryBuffer (overlay or low latency canvas).
BASE_FEATURE(kDrawingBufferWithoutGpuMemoryBuffer,
             "DrawingBufferWithoutGpuMemoryBuffer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Drop input events before user sees first paint https://crbug.com/1255485
BASE_FEATURE(kDropInputEventsBeforeFirstPaint,
             "DropInputEventsBeforeFirstPaint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEstablishGpuChannelAsync,
             "EstablishGpuChannelAsync",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(crbug.com/1278147): Experiment with this more on desktop to
             // see if it can help.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables unload handler deprecation via Permissions-Policy.
// https://crbug.com/1324111
BASE_FEATURE(kDeprecateUnload,
             "DeprecateUnload",
             base::FEATURE_DISABLED_BY_DEFAULT);
// If < 100, each user experiences the deprecation on this % of origins.
// Which origins varies per user.
const base::FeatureParam<int> kDeprecateUnloadPercent{&kDeprecateUnload,
                                                      "rollout_percent", 100};
// This buckets users, with users in each bucket having a consistent experience
// of the unload deprecation rollout.
const base::FeatureParam<int> kDeprecateUnloadBucket{&kDeprecateUnload,
                                                     "rollout_bucket", 0};

// Only used if `kDeprecateUnload` is enabled. The deprecation will only apply
// if the host is on the allow-list.
BASE_FEATURE(kDeprecateUnloadByAllowList,
             "DeprecateUnloadByAllowList",
             base::FEATURE_DISABLED_BY_DEFAULT);
// A list of hosts for which deprecation of unload is allowed. If it's empty
// the all hosts are allowed.
const base::FeatureParam<std::string> kDeprecateUnloadAllowlist{
    &kDeprecateUnloadByAllowList, "allowlist", ""};

// Enable not reporting orphan pointerup (pointerup not accompanied by
// pointerdown) as an interaction in performance event timing.
BASE_FEATURE(kEventTimingHandleOrphanPointerup,
             "EventTimingHandleOrphanPointerup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether LCP calculations should exclude low-entropy images. If
// enabled, then the associated parameter sets the cutoff, expressed as the
// minimum number of bits of encoded image data used to encode each rendered
// pixel. Note that this is not just pixels of decoded image data; the rendered
// size includes any scaling applied by the rendering engine to display the
// content.
BASE_FEATURE(kExcludeLowEntropyImagesFromLCP,
             "ExcludeLowEntropyImagesFromLCP",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<double> kMinimumEntropyForLCP{
    &kExcludeLowEntropyImagesFromLCP, "min_bpp", 0.05};

BASE_FEATURE(kExemptSpeculationRulesHeaderFromCSP,
             "ExemptSpeculationRulesHeaderFromCSP",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExpandCompositedCullRect,
             "ExpandCompositedCullRect",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kCullRectPixelDistanceToExpand,
                   &kExpandCompositedCullRect,
                   "pixels",
                   4000);
BASE_FEATURE_PARAM(double,
                   kCullRectExpansionDPRCoef,
                   &kExpandCompositedCullRect,
                   "dpr_coef",
                   0);
BASE_FEATURE_PARAM(bool,
                   kSmallScrollersUseMinCullRect,
                   &kExpandCompositedCullRect,
                   "small_scroller_opt",
                   false);

// Enable the <fencedframe> element; see crbug.com/1123606. Note that enabling
// this feature does not automatically expose this element to the web, it only
// allows the element to be enabled by the runtime enabled feature, for origin
// trials.
BASE_FEATURE(kFencedFrames, "FencedFrames", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable sending event-level reports through reportEvent() in cross-origin
// subframes. This requires opt-in both from the cross-origin subframe that is
// sending the beacon as well as the document that contains information about
// the reportEvent() endpoints.
// The "UnlabeledTraffic" flag only allows cross-origin reportEvent() beacons
// for non-Mode A/B 3PCD Chrome-facilitated testing traffic. See the
// "CookieDeprecationFacilitatedTesting" feature in
// `content/public/common/content_features.cc` for more information.
BASE_FEATURE(kFencedFramesCrossOriginEventReportingUnlabeledTraffic,
             "FencedFramesCrossOriginEventReportingUnlabeledTraffic",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The "AllTraffic" flag allows the feature for all traffic regardless of label.
BASE_FEATURE(kFencedFramesCrossOriginEventReportingAllTraffic,
             "FencedFramesCrossOriginEventReportingAllTraffic",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Temporarily un-disable credentials on fenced frame automatic beacons until
// third party cookie deprecation.
// TODO(crbug.com/1496395): Remove this after 3PCD.
BASE_FEATURE(kFencedFramesAutomaticBeaconCredentials,
             "FencedFramesAutomaticBeaconCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls functionality related to network revocation/local unpartitioned
// data access in fenced frames.
BASE_FEATURE(kFencedFramesLocalUnpartitionedDataAccess,
             "FencedFramesLocalUnpartitionedDataAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFencedFramesReportEventHeaderChanges,
             "FencedFramesReportEventHeaderChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a bug fix that allows a 'src' allowlist in the |allow| parameter of a
// <fencedframe> or <iframe> loaded with a FencedFrameConfig to behave as
// expected. See: https://crbug.com/349080952
BASE_FEATURE(kFencedFramesSrcPermissionsPolicy,
             "FencedFramesSrcPermissionsPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls access to an API to exempt certain URLs from fenced frame
// network revocation to facilitate testing.
BASE_FEATURE(kExemptUrlFromNetworkRevocationForTesting,
             "ExemptUrlFromNetworkRevocationForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use "style" and "json" destinations for CSS and JSON modules.
// https://crbug.com/1491336
BASE_FEATURE(kFetchDestinationJsonCssModules,
             "kFetchDestinationJsonCssModules",
             base::FEATURE_ENABLED_BY_DEFAULT);

// File handling icons. https://crbug.com/1218213
BASE_FEATURE(kFileHandlingIcons,
             "FileHandlingIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigation,
             "FileSystemUrlNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigationForChromeAppsOnly,
             "FileSystemUrlNavigationForChromeAppsOnly",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFilteringScrollPrediction,
             "FilteringScrollPrediction",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(b/284271126): Run the experiment on desktop and enable if
             // positive.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const base::FeatureParam<std::string> kFilteringScrollPredictionFilterParam{
    &kFilteringScrollPrediction, "filter", "one_euro_filter"};

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Enables FLEDGE implementation. See https://crbug.com/1186444.
BASE_FEATURE(kFledge, "Fledge", base::FEATURE_ENABLED_BY_DEFAULT);

// See
// https://github.com/WICG/turtledove/blob/main/FLEDGE_browser_bidding_and_auction_API.md
BASE_FEATURE(kFledgeBiddingAndAuctionServer,
             "FledgeBiddingAndAuctionServer",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kFledgeBiddingAndAuctionKeyURL{
    &kFledgeBiddingAndAuctionServer, "FledgeBiddingAndAuctionKeyURL", ""};
const base::FeatureParam<std::string> kFledgeBiddingAndAuctionKeyConfig{
    &kFledgeBiddingAndAuctionServer, "FledgeBiddingAndAuctionKeyConfig", ""};

// See in the header.
BASE_FEATURE(kFledgeConsiderKAnonymity,
             "FledgeConsiderKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFledgeEnforceKAnonymity,
             "FledgeEnforceKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgePassKAnonStatusToReportWin,
             "FledgePassKAnonStatusToReportWin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFledgePassRecencyToGenerateBid,
             "FledgePassRecencyToGenerateBid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeSampleDebugReports,
             "FledgeSampleDebugReports",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kFledgeDebugReportLockout{
    &kFledgeSampleDebugReports, "fledge_debug_report_lockout",
    base::Days(365 * 3)};
const base::FeatureParam<base::TimeDelta> kFledgeDebugReportRestrictedCooldown{
    &kFledgeSampleDebugReports, "fledge_debug_report_restricted_cooldown",
    base::Days(365)};
const base::FeatureParam<base::TimeDelta> kFledgeDebugReportShortCooldown{
    &kFledgeSampleDebugReports, "fledge_debug_report_short_cooldown",
    base::Days(14)};
const base::FeatureParam<int> kFledgeDebugReportSamplingRandomMax{
    &kFledgeSampleDebugReports, "fledge_debug_report_sampling_random_max",
    1000};
const base::FeatureParam<int>
    kFledgeDebugReportSamplingRestrictedCooldownRandomMax{
        &kFledgeSampleDebugReports,
        "fledge_debug_report_sampling_restricted_cooldown_random_max", 10};
const base::FeatureParam<base::TimeDelta>
    kFledgeEnableFilteringDebugReportStartingFrom{
        &kFledgeSampleDebugReports,
        "fledge_enable_filtering_debug_report_starting_from",
        base::Milliseconds(0)};

BASE_FEATURE(kFledgeSplitTrustedSignalsFetchingURL,
             "FledgeSplitTrustedSignalsFetchingURL",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kFledgeCustomMaxAuctionAdComponentsValue{
    &kFledgeCustomMaxAuctionAdComponents, "FledgeAdComponentLimit", 40};

BASE_FEATURE(kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
             "FledgeBidderWorkletGroupByOriginContextsToKeep",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue{
        &kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
        "GroupByOriginContextLimit", 10};
const base::FeatureParam<bool>
    kFledgeNumberBidderWorkletContextsIncludeFacilitedTesting{
        &kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
        "IncludeFacilitatedTestingGroups", false};

BASE_FEATURE(kFledgeAlwaysReuseBidderContext,
             "FledgeAlwaysReuseBidderContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeAlwaysReuseSellerContext,
             "FledgeAlwaysReuseSellerContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kFledgeRealTimeReportingNumBuckets{
    &kFledgeRealTimeReporting, "FledgeRealTimeReportingNumBuckets", 1024};
const base::FeatureParam<double> kFledgeRealTimeReportingEpsilon{
    &kFledgeRealTimeReporting, "FledgeRealTimeReportingEpsilon", 1};
const base::FeatureParam<double>
    kFledgeRealTimeReportingPlatformContributionPriority{
        &kFledgeRealTimeReporting,
        "FledgeRealTimeReportingPlatformContributionPriority", 1};
const base::FeatureParam<base::TimeDelta> kFledgeRealTimeReportingWindow{
    &kFledgeRealTimeReporting, "FledgeRealTimeReportingWindow",
    base::Seconds(20)};
const base::FeatureParam<int> kFledgeRealTimeReportingMaxReports{
    &kFledgeRealTimeReporting, "FledgeRealTimeReportingMaxReports", 10};

// Enable enforcement of permission policy for
// privateAggregation.contributeToHistogramOnEvent.
BASE_FEATURE(kFledgeEnforcePermissionPolicyContributeOnEvent,
             "FledgeEnforcePermissionPolicyContributeOnEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceHighPerformanceGPUForWebGL,
             "ForceHighPerformanceGPUForWebGL",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceInOrderScript,
             "ForceInOrderScript",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Automatically convert light-themed pages to use a Blink-generated dark theme
BASE_FEATURE(kForceWebContentsDarkMode,
             "WebContentsForceDark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Which algorithm should be used for color inversion?
const base::FeatureParam<ForceDarkInversionMethod>::Option
    forcedark_inversion_method_options[] = {
        {ForceDarkInversionMethod::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkInversionMethod::kHslBased, "hsl_based"},
        {ForceDarkInversionMethod::kCielabBased, "cielab_based"},
        {ForceDarkInversionMethod::kRgbBased, "rgb_based"}};

const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam{&kForceWebContentsDarkMode,
                                   "inversion_method",
                                   ForceDarkInversionMethod::kUseBlinkSettings,
                                   &forcedark_inversion_method_options};

// Should images be inverted?
const base::FeatureParam<ForceDarkImageBehavior>::Option
    forcedark_image_behavior_options[] = {
        {ForceDarkImageBehavior::kUseBlinkSettings,
         "use_blink_settings_for_images"},
        {ForceDarkImageBehavior::kInvertNone, "none"},
        {ForceDarkImageBehavior::kInvertSelectively, "selective"}};

const base::FeatureParam<ForceDarkImageBehavior> kForceDarkImageBehaviorParam{
    &kForceWebContentsDarkMode, "image_behavior",
    ForceDarkImageBehavior::kUseBlinkSettings,
    &forcedark_image_behavior_options};

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 255 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkForegroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "foreground_lightness_threshold", -1};

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 255 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkBackgroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "background_lightness_threshold", -1};

const base::FeatureParam<ForceDarkImageClassifier>::Option
    forcedark_image_classifier_policy_options[] = {
        {ForceDarkImageClassifier::kUseBlinkSettings,
         "use_blink_settings_for_image_policy"},
        {ForceDarkImageClassifier::kNumColorsWithMlFallback,
         "num_colors_with_ml_fallback"},
        {ForceDarkImageClassifier::kTransparencyAndNumColors,
         "transparency_and_num_colors"},
};

const base::FeatureParam<ForceDarkImageClassifier>
    kForceDarkImageClassifierParam{&kForceWebContentsDarkMode,
                                   "classifier_policy",
                                   ForceDarkImageClassifier::kUseBlinkSettings,
                                   &forcedark_image_classifier_policy_options};

// Enables the frequency capping for detecting large sticky ads.
// Large-sticky-ads are those ads that stick to the bottom of the page
// regardless of a user’s efforts to scroll, and take up more than 30% of the
// screen’s real estate.
BASE_FEATURE(kFrequencyCappingForLargeStickyAdDetection,
             "FrequencyCappingForLargeStickyAdDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the frequency capping for detecting overlay popups. Overlay-popups
// are the interstitials that pop up and block the main content of the page.
BASE_FEATURE(kFrequencyCappingForOverlayPopupDetection,
             "FrequencyCappingForOverlayPopupDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGMSCoreEmoji, "GMSCoreEmoji", base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Defers device selection until after permission is granted.
BASE_FEATURE(kGetUserMediaDeferredDeviceSettingsSelection,
             "GetUserMediaDeferredDeviceSettingsSelection",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kHiddenSelectionBounds,
             "HiddenSelectionBounds",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kImageLoadingPrioritizationFix,
             "ImageLoadingPrioritizationFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInputPredictorTypeChoice,
             "InputPredictorTypeChoice",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kIntensiveWakeUpThrottling,
             "IntensiveWakeUpThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Name of the parameter that controls the grace period during which there is no
// intensive wake up throttling after a page is hidden. Defined here to allow
// access from about_flags.cc. The FeatureParam is defined in
// third_party/blink/renderer/platform/scheduler/common/features.cc.
const char kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[] =
    "grace_period_seconds";

// Kill switch for the Interest Group API, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
BASE_FEATURE(kInterestGroupStorage,
             "InterestGroupStorage",
             base::FEATURE_ENABLED_BY_DEFAULT);
// TODO(crbug.com/1197209): Adjust these limits in response to usage.
const base::FeatureParam<int> kInterestGroupStorageMaxOwners{
    &kInterestGroupStorage, "max_owners", 1000};
const base::FeatureParam<int> kInterestGroupStorageMaxStoragePerOwner{
    &kInterestGroupStorage, "max_storage_per_owner", 10 * 1024 * 1024};
const base::FeatureParam<int> kInterestGroupStorageMaxGroupsPerOwner{
    &kInterestGroupStorage, "max_groups_per_owner", 2000};
const base::FeatureParam<int> kInterestGroupStorageMaxNegativeGroupsPerOwner{
    &kInterestGroupStorage, "max_negative_groups_per_owner", 20000};
const base::FeatureParam<int> kInterestGroupStorageMaxOpsBeforeMaintenance{
    &kInterestGroupStorage, "max_ops_before_maintenance", 1000};

// Allow process isolation of iframes with the 'sandbox' attribute set. Whether
// or not such an iframe will be isolated may depend on options specified with
// the attribute. Note: At present, only iframes with origin-restricted
// sandboxes are isolated.
BASE_FEATURE(kIsolateSandboxedIframes,
             "IsolateSandboxedIframes",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<IsolateSandboxedIframesGrouping>::Option
    isolated_sandboxed_iframes_grouping_types[] = {
        {IsolateSandboxedIframesGrouping::kPerSite, "per-site"},
        {IsolateSandboxedIframesGrouping::kPerOrigin, "per-origin"},
        {IsolateSandboxedIframesGrouping::kPerDocument, "per-document"}};
const base::FeatureParam<IsolateSandboxedIframesGrouping>
    kIsolateSandboxedIframesGroupingParam{
        &kIsolateSandboxedIframes, "grouping",
        IsolateSandboxedIframesGrouping::kPerOrigin,
        &isolated_sandboxed_iframes_grouping_types};

BASE_FEATURE(kKalmanDirectionCutOff,
             "KalmanDirectionCutOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKalmanHeuristics,
             "KalmanHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeepAliveInBrowserMigration,
             "KeepAliveInBrowserMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportingInBrowserMigration,
             "AttributionReportingInBrowserMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLCPCriticalPathPredictor,
             "LCPCriticalPathPredictor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kLCPCriticalPathAdjustImageLoadPriority{
    &kLCPCriticalPathPredictor, "lcpp_adjust_image_load_priority", false};

BASE_FEATURE_PARAM(size_t,
                   kLCPCriticalPathPredictorMaxElementLocatorLength,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_element_locator_length",
                   1024);

const base::FeatureParam<bool>
    kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost{
        &kLCPCriticalPathPredictor,
        "lcpp_adjust_image_load_priority_override_first_n_boost", false};

const base::FeatureParam<LcppRecordedLcpElementTypes>::Option
    lcpp_recorded_element_types[] = {
        {LcppRecordedLcpElementTypes::kAll, "all"},
        {LcppRecordedLcpElementTypes::kImageOnly, "image_only"},
};
const base::FeatureParam<LcppRecordedLcpElementTypes>
    kLCPCriticalPathPredictorRecordedLcpElementTypes{
        &kLCPCriticalPathPredictor, "lcpp_recorded_lcp_element_types",
        LcppRecordedLcpElementTypes::kImageOnly, &lcpp_recorded_element_types};

const base::FeatureParam<LcppResourceLoadPriority>::Option
    lcpp_resource_load_priorities[] = {
        {LcppResourceLoadPriority::kMedium, "medium"},
        {LcppResourceLoadPriority::kHigh, "high"},
        {LcppResourceLoadPriority::kVeryHigh, "very_high"},
};
const base::FeatureParam<LcppResourceLoadPriority>
    kLCPCriticalPathPredictorImageLoadPriority{
        &kLCPCriticalPathPredictor, "lcpp_image_load_priority",
        LcppResourceLoadPriority::kVeryHigh, &lcpp_resource_load_priorities};

const base::FeatureParam<bool>
    kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement{
        &kLCPCriticalPathPredictor,
        "lcpp_enable_image_load_priority_for_htmlimageelement", false};

const base::FeatureParam<int> kLCPCriticalPathPredictorMaxHostsToTrack{
    &kLCPCriticalPathPredictor, "lcpp_max_hosts_to_track", 100};

const base::FeatureParam<int>
    kLCPCriticalPathPredictorHistogramSlidingWindowSize{
        &kLCPCriticalPathPredictor, "lcpp_histogram_sliding_window_size", 1000};

const base::FeatureParam<int> kLCPCriticalPathPredictorMaxHistogramBuckets{
    &kLCPCriticalPathPredictor, "lcpp_max_histogram_buckets", 10};

BASE_FEATURE(kLCPScriptObserver,
             "LCPScriptObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<LcppResourceLoadPriority>
    kLCPScriptObserverScriptLoadPriority{
        &kLCPScriptObserver, "lcpscriptobserver_script_load_priority",
        LcppResourceLoadPriority::kVeryHigh, &lcpp_resource_load_priorities};

const base::FeatureParam<LcppResourceLoadPriority>
    kLCPScriptObserverImageLoadPriority{
        &kLCPScriptObserver, "lcpscriptobserver_image_load_priority",
        LcppResourceLoadPriority::kVeryHigh, &lcpp_resource_load_priorities};

const base::FeatureParam<int> kLCPScriptObserverMaxUrlLength{
    &kLCPScriptObserver, "lcpscriptobserver_script_max_url_length", 1024};

const base::FeatureParam<int> kLCPScriptObserverMaxUrlCountPerOrigin{
    &kLCPScriptObserver, "lcpscriptobserver_script_max_url_count_per_origin",
    5};

const base::FeatureParam<bool> kLCPScriptObserverAdjustImageLoadPriority{
    &kLCPScriptObserver, "lcpscriptobserver_adjust_image_load_priority", false};

BASE_FEATURE(kLCPTimingPredictorPrerender2,
             "LCPTimingPredictorPrerender2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLCPPAutoPreconnectLcpOrigin,
             "LCPPAutoPreconnectLcpOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kLCPPAutoPreconnectFrequencyThreshold{
    &kLCPPAutoPreconnectLcpOrigin, "lcpp_preconnect_frequency_threshold", 0.5};

const base::FeatureParam<int> kkLCPPAutoPreconnectMaxPreconnectOriginsCount{
    &kLCPPAutoPreconnectLcpOrigin, "lcpp_preconnect_max_origins", 2};

BASE_FEATURE(kLCPPDeferUnusedPreload,
             "LCPPDeferUnusedPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<LcppDeferUnusedPreloadExcludedResourceType>::Option
    lcpp_defer_unused_preload_excluded_resource_type[] = {
        {LcppDeferUnusedPreloadExcludedResourceType::kNone, "none"},
        {LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet, "stylesheet"},
        {LcppDeferUnusedPreloadExcludedResourceType::kScript, "script"},
        {LcppDeferUnusedPreloadExcludedResourceType::kMock, "mock"},
};

const base::FeatureParam<LcppDeferUnusedPreloadExcludedResourceType>
    kLcppDeferUnusedPreloadExcludedResourceType{
        &kLCPPDeferUnusedPreload, "excluded_resource_type",
        LcppDeferUnusedPreloadExcludedResourceType::kNone,
        &lcpp_defer_unused_preload_excluded_resource_type};

const base::FeatureParam<double> kLCPPDeferUnusedPreloadFrequencyThreshold{
    &kLCPPDeferUnusedPreload, "lcpp_unused_preload_frequency_threshold", 0.5};

const base::FeatureParam<LcppDeferUnusedPreloadPreloadedReason>::Option
    lcpp_defer_unused_preload_preloaded_reason[] = {
        {LcppDeferUnusedPreloadPreloadedReason::kAll, "all"},
        {LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly,
         "link_preload"},
        {LcppDeferUnusedPreloadPreloadedReason::kBrowserSpeculativePreloadOnly,
         "speculative_preload"},
};

const base::FeatureParam<LcppDeferUnusedPreloadPreloadedReason>
    kLcppDeferUnusedPreloadPreloadedReason{
        &kLCPPDeferUnusedPreload, "preloaded_reason",
        LcppDeferUnusedPreloadPreloadedReason::kAll,
        &lcpp_defer_unused_preload_preloaded_reason};

const base::FeatureParam<LcppDeferUnusedPreloadTiming>::Option
    lcpp_defer_unused_preload_timing[] = {
        {LcppDeferUnusedPreloadTiming::kPostTask, "post_task"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictor,
         "lcp_timing_predictor"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictorWithPostTask,
         "lcp_timing_predictor_with_post_task"},
};

const base::FeatureParam<LcppDeferUnusedPreloadTiming>
    kLcppDeferUnusedPreloadTiming{&kLCPPDeferUnusedPreload, "load_timing",
                                  LcppDeferUnusedPreloadTiming::kPostTask,
                                  &lcpp_defer_unused_preload_timing};

BASE_FEATURE(kLCPPFontURLPredictor,
             "LCPPFontURLPredictor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLCPPFontURLPredictorMaxUrlLength{
    &kLCPPFontURLPredictor, "lcpp_max_font_url_length", 1024};

const base::FeatureParam<int> kLCPPFontURLPredictorMaxUrlCountPerOrigin{
    &kLCPPFontURLPredictor, "lcpp_max_font_url_count_per_origin", 10};

const base::FeatureParam<double> kLCPPFontURLPredictorFrequencyThreshold{
    &kLCPPFontURLPredictor, "lcpp_font_url_frequency_threshold", 0.5};

const base::FeatureParam<int> kLCPPFontURLPredictorMaxPreloadCount{
    &kLCPPFontURLPredictor, "lcpp_max_font_url_to_preload", 5};

const base::FeatureParam<bool> kLCPPFontURLPredictorEnablePrefetch{
    &kLCPPFontURLPredictor, "lcpp_enable_font_prefetch", false};

// Negative value is used for disabling this threshold.
const base::FeatureParam<double> kLCPPFontURLPredictorThresholdInMbps{
    &kLCPPFontURLPredictor, "lcpp_font_prefetch_threshold", -1};

const base::FeatureParam<std::string> kLCPPFontURLPredictorExcludedHosts{
    &kLCPPFontURLPredictor, "lcpp_font_prefetch_excluded_hosts", ""};

BASE_FEATURE_PARAM(bool,
                   kLCPPCrossSiteFontPredictionAllowed,
                   &kLCPPFontURLPredictor,
                   "lcpp_cross_site_font_prediction_allowed",
                   false);

BASE_FEATURE(kLCPPInitiatorOrigin,
             "LCPPInitiatorOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLcppInitiatorOriginHistogramSlidingWindowSize{
    &kLCPPInitiatorOrigin,
    "lcpp_initiator_origin_histogram_sliding_window_size", 10000};

const base::FeatureParam<int> kLcppInitiatorOriginMaxHistogramBuckets{
    &kLCPPInitiatorOrigin, "lcpp_initiator_origin_max_histogram_buckets", 100};

BASE_FEATURE(kLCPPLazyLoadImagePreload,
             "LCPPLazyLoadImagePreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
const base::FeatureParam<LcppPreloadLazyLoadImageType>
    kLCPCriticalPathPredictorPreloadLazyLoadImageType{
        &kLCPPLazyLoadImagePreload, "lcpp_preload_lazy_load_image_type",
        LcppPreloadLazyLoadImageType::kNone, &lcpp_preload_lazy_load_image};

BASE_FEATURE(kPreloadSystemFonts,
             "PreloadSystemFonts",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPreloadSystemFontsTargets{
    &kPreloadSystemFonts, "preload_system_fonts_targets", "[]"};

const base::FeatureParam<int> kPreloadSystemFontsRequiredMemoryGB{
    &kPreloadSystemFonts, "preload_system_fonts_required_memory_gb", 4};

BASE_FEATURE(kLCPPMultipleKey,
             "LCPPMultipleKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLCPPMultipleKeyMaxPathLength{
    &kLCPPMultipleKey, "lcpp_multiple_key_max_path_length", 15};

const base::FeatureParam<LcppMultipleKeyTypes>::Option
    lcpp_multiple_key_types[] = {
        {LcppMultipleKeyTypes::kDefault, "default"},
        {LcppMultipleKeyTypes::kLcppKeyStat, "lcpp_key_stat"},
};

const base::FeatureParam<LcppMultipleKeyTypes> kLcppMultipleKeyType{
    &kLCPPMultipleKey, "lcpp_multiple_key_type", LcppMultipleKeyTypes::kDefault,
    &lcpp_multiple_key_types};

const base::FeatureParam<int> kLcppMultipleKeyHistogramSlidingWindowSize{
    &kLCPPMultipleKey, "lcpp_multiple_key_histogram_sliding_window_size", 1000};

const base::FeatureParam<int> kLcppMultipleKeyMaxHistogramBuckets{
    &kLCPPMultipleKey, "lcpp_multiple_key_max_histogram_buckets", 10};

BASE_FEATURE(kHttpDiskCachePrewarming,
             "HttpDiskCachePrewarming",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kHttpDiskCachePrewarmingMaxUrlLength{
    &kHttpDiskCachePrewarming, "http_disk_cache_prewarming_max_url_length",
    1024};

const base::FeatureParam<int> kHttpDiskCachePrewarmingHistorySize{
    &kHttpDiskCachePrewarming, "http_disk_cache_prewarming_history_size", 1024};

const base::FeatureParam<base::TimeDelta>
    kHttpDiskCachePrewarmingReprewarmPeriod{
        &kHttpDiskCachePrewarming,
        "http_disk_cache_prewarming_reprewarm_period", base::Minutes(10)};

const base::FeatureParam<bool> kHttpDiskCachePrewarmingTriggerOnNavigation{
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_trigger_on_navigation", true};

const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover{
        &kHttpDiskCachePrewarming,
        "http_disk_cache_prewarming_trigger_on_pointer_down_or_hover", true};

const base::FeatureParam<bool>
    kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption{
        &kHttpDiskCachePrewarming,
        "http_disk_cache_prewarming_use_read_and_discard_body_option", false};

const base::FeatureParam<bool> kHttpDiskCachePrewarmingSkipDuringBrowserStartup{
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_skip_during_browser_startup", true};

BASE_FEATURE(kLegacyParsingOfXContentTypeOptions,
             "LegacyParsingOfXContentTypeOptions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to reduce the set of resources fetched by No-State Prefetch.
BASE_FEATURE(kLightweightNoStatePrefetch,
             "LightweightNoStatePrefetch",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLinkPreview, "LinkPreview", base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<LinkPreviewTriggerType>::Option
    link_preview_trigger_type_options[] = {
        {LinkPreviewTriggerType::kAltClick, "alt_click"},
        {LinkPreviewTriggerType::kAltHover, "alt_hover"},
        {LinkPreviewTriggerType::kLongPress, "long_press"}};
const base::FeatureParam<LinkPreviewTriggerType> kLinkPreviewTriggerType{
    &kLinkPreview, "trigger_type", LinkPreviewTriggerType::kAltHover,
    &link_preview_trigger_type_options};

// A feature to control whether the loading phase should be extended beyond
// First Meaningful Paint by a configurable buffer.
BASE_FEATURE(kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint,
             "LoadingPhaseBufferTimeAfterFirstMeaningfulPaint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes network loading tasks unfreezable so that they can be processed while
// the page is frozen.
BASE_FEATURE(kLoadingTasksUnfreezable,
             "LoadingTasksUnfreezable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
             "LogUnexpectedIPCPostedToBackForwardCachedDocuments",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow low latency canvas 2D to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (Canvas2DImageChromium is disabled).
BASE_FEATURE(kLowLatencyCanvas2dImageChromium,
             "LowLatencyCanvas2dImageChromium",
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
);

// Allow low latency WebGL to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (WebGLImageChromium is disabled).
BASE_FEATURE(kLowLatencyWebGLImageChromium,
             "LowLatencyWebGLImageChromium",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLowPriorityAsyncScriptExecution,
             "LowPriorityAsyncScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kTimeoutForLowPriorityAsyncScriptExecution{
        &kLowPriorityAsyncScriptExecution, "low_pri_async_exec_timeout",
        base::Milliseconds(0)};

// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |low_pri_async_exec_feature_limit|. Zero value means no limit.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kLowPriorityAsyncScriptExecutionFeatureLimitParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_feature_limit",
                   base::Seconds(0));

// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_cross_site_only",
                   false);

BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionMainFrameOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_main_frame_only",
                   false);

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

// kLowPriorityAsyncScriptExecution will use the specified priority as a lower
// task priority.
const base::FeatureParam<AsyncScriptPrioritisationType>::Option
    async_script_prioritisation_types[] = {
        {AsyncScriptPrioritisationType::kHigh, "high"},
        {AsyncScriptPrioritisationType::kLow, "low"},
        {AsyncScriptPrioritisationType::kBestEffort, "best_effort"},
};
const base::FeatureParam<AsyncScriptPrioritisationType>
    kLowPriorityAsyncScriptExecutionLowerTaskPriorityParam{
        &kLowPriorityAsyncScriptExecution,
        "low_pri_async_exec_lower_task_priority",
        AsyncScriptPrioritisationType::kBestEffort,
        &async_script_prioritisation_types};
// kLowPriorityAsyncScriptExecution will change evaluation schedule for the
// specified target.
const base::FeatureParam<AsyncScriptExperimentalSchedulingTarget>
    kLowPriorityAsyncScriptExecutionTargetParam{
        &kLowPriorityAsyncScriptExecution, "low_pri_async_exec_target",
        AsyncScriptExperimentalSchedulingTarget::kBoth,
        &async_script_experimental_scheduling_targets};
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
                   false);

// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// low.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutLowFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec__opt_out_low_fetch_priority_hint",
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
    false);

BASE_FEATURE(kLowPriorityScriptLoading,
             "LowPriorityScriptLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kLowPriorityScriptLoadingCrossSiteOnlyParam,
                   &kLowPriorityScriptLoading,
                   "low_pri_async_loading_cross_site_only",
                   false);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kLowPriorityScriptLoadingFeatureLimitParam,
                   &kLowPriorityScriptLoading,
                   "low_pri_async_loading_feature_limit",
                   base::Seconds(0));
const base::FeatureParam<std::string> kLowPriorityScriptLoadingDenyListParam{
    &kLowPriorityScriptLoading, "low_pri_async_loading_deny_list", ""};
BASE_FEATURE_PARAM(bool,
                   kLowPriorityScriptLoadingMainFrameOnlyParam,
                   &kLowPriorityScriptLoading,
                   "low_pri_async_loading_main_frame_only",
                   false);

BASE_FEATURE(kMixedContentAutoupgrade,
             "AutoupgradeMixedContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResourceFetcherStoresStrongReferences,
             "ResourceFetcherStoresStrongReferences",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReference,
             "MemoryCacheStrongReference",
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

BASE_FEATURE(kMHTML_Improvements,
             "MHTML_Improvements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded.
// Note that the desktop roll out is being done separately from android. See
// https://crbug.com/40258405
BASE_FEATURE(kNavigationPredictor,
             "NavigationPredictor",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kPredictorTrafficClientEnabledPercent{
    &kNavigationPredictor, "traffic_client_enabled_percent",
#if BUILDFLAG(IS_ANDROID)
    100
#else
    5
#endif
};

// Used to control the collection of new viewport related anchor element
// metrics. Metrics will not be recorded if either this or kNavigationPredictor
// is disabled.
BASE_FEATURE(kNavigationPredictorNewViewportFeatures,
             "NavigationPredictorNewViewportFeatures",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kNoForcedFrameUpdatesForWebTests,
             "NoForcedFrameUpdatesForWebTests",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoThrottlingVisibleAgent,
             "NoThrottlingVisibleAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeLoadingDataUrls,
             "OptimizeLoadingDataUrls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOriginAgentClusterDefaultEnabled,
             "OriginAgentClusterDefaultEnable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOriginTrialStateHostApplyFeatureDiff,
             "OriginTrialStateHostApplyFeatureDiff",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable defer commits to avoid flash of unstyled content, for same origin
// navigation only.
BASE_FEATURE(kPaintHolding, "PaintHolding", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable defer commits to avoid flash of unstyled content, for all navigation.
BASE_FEATURE(kPaintHoldingCrossOrigin,
             "PaintHoldingCrossOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kParkableImagesToDisk,
             "ParkableImagesToDisk",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude CanvasFontCache from
// PartialLowModeOnMidRangeDevices. This is used to see how
// CanvasFontCache affects graphics smoothness and renderer memory usage.
const base::FeatureParam<bool> kPartialLowEndModeExcludeCanvasFontCache{
    &base::features::kPartialLowEndModeOnMidRangeDevices,
    "exclude-canvas-font-cache", false};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

// When enabled, this flag partitions the :visited link hashtable by <link url,
// top-level site, frame origin>.
BASE_FEATURE(kPartitionVisitedLinkDatabase,
             "PartitionVisitedLinkDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this flag partitions the :visited link hashtable by <link url,
// top-level site, frame origin> AND adds the "self link" <link url, link as a
// schemeful site, link as an origin> for each link :visited from a top-level or
// same-origin subframe to the hashtable as well.
// NOTE: users need only enable kPartitionVisitedLinkDatabaseWithSelfLinks
// to achieve partitioning AND self links. You do NOT need to enable
// kPartitionVisitedLinkDatabase as well, though doing so is a no-op and will
// not change the behavior of this feature.
BASE_FEATURE(kPartitionVisitedLinkDatabaseWithSelfLinks,
             "PartitionVisitedLinkDatabaseWithSelfLinks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of the PaintCache for Path2D objects that are rasterized
// out of process.  Has no effect when kCanvasOopRasterization is disabled.
BASE_FEATURE(kPath2DPaintCache,
             "Path2DPaintCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
// TODO(350785857): remove the non-plzdedicatedworker path.
#if BUILDFLAG(IS_FUCHSIA)
#define DISABLED_ON_FUCHSIA base::FEATURE_DISABLED_BY_DEFAULT
#else
#define DISABLED_ON_FUCHSIA base::FEATURE_ENABLED_BY_DEFAULT
#endif
BASE_FEATURE(kPlzDedicatedWorker, "PlzDedicatedWorker", DISABLED_ON_FUCHSIA);

BASE_FEATURE(kDedicatedWorkerAblationStudyEnabled,
             "DedicatedWorkerAblationStudyEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDedicatedWorkerStartDelayInMs = {
    &kDedicatedWorkerAblationStudyEnabled, "DedicatedWorkerStartDelayInMs", 0};

BASE_FEATURE(kUseAncestorRenderFrameForWorker,
             "UseAncestorRenderFrameForWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreInitializePageAndFrameForSVGImage,
             "PreInitializePageAndFrameForSVGImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kMaxCountOfPreInitializePageAndFrameForSVGImage{
        &kPreInitializePageAndFrameForSVGImage, "max_pre_initialize_count", 5};

BASE_FEATURE(kPrecompileInlineScripts,
             "PrecompileInlineScripts",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should composite a PLSA (paint layer scrollable area) even if it
// means losing lcd text.
BASE_FEATURE(kPreferCompositingToLCDText,
             "PreferCompositingToLCDText",
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
             "PrefetchFontLookupTables",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
#endif

// Prefetch request properties are updated to be privacy-preserving. See
// crbug.com/988956.
BASE_FEATURE(kPrefetchPrivacyChanges,
             "PrefetchPrivacyChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreloadingHeuristicsMLModel,
             "PreloadingHeuristicsMLModel",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kPreloadingModelTimerStartDelay{
    &kPreloadingHeuristicsMLModel, "timer_start_delay", 0};
const base::FeatureParam<int> kPreloadingModelTimerInterval{
    &kPreloadingHeuristicsMLModel, "timer_interval", 100};
const base::FeatureParam<bool> kPreloadingModelOneExecutionPerHover{
    &kPreloadingHeuristicsMLModel, "one_execution_per_hover", true};
// The default max hover time of 10s covers the 98th percentile of hovering
// cases that are relevant to the model.
const base::FeatureParam<base::TimeDelta> kPreloadingModelMaxHoverTime{
    &kPreloadingHeuristicsMLModel, "max_hover_time", base::Seconds(10)};
const base::FeatureParam<bool> kPreloadingModelEnactCandidates{
    &kPreloadingHeuristicsMLModel, "enact_candidates", false};
const base::FeatureParam<int> kPreloadingModelPrefetchModerateThreshold{
    &kPreloadingHeuristicsMLModel, "prefetch_moderate_threshold", 50};
const base::FeatureParam<int> kPreloadingModelPrerenderModerateThreshold{
    &kPreloadingHeuristicsMLModel, "prerender_moderate_threshold", 50};

BASE_FEATURE(kPrerender2InNewTab,
             "Prerender2InNewTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2MainFrameNavigation,
             "Prerender2MainFrameNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrerender2MaxNumOfRunningSpeculationRules[] =
    "max_num_of_running_speculation_rules";

BASE_FEATURE(kPrerender2MemoryControls,
             "Prerender2MemoryControls",
             base::FEATURE_ENABLED_BY_DEFAULT);
const char kPrerender2MemoryThresholdParamName[] = "memory_threshold_in_mb";
const char kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName[] =
    "acceptable_percent_of_system_memory";

BASE_FEATURE(kPrerender2EarlyDocumentLifecycleUpdate,
             "Prerender2EarlyDocumentLifecycleUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2NoVarySearch,
             "Prerender2NoVarySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2WarmUpCompositor,
             "Prerender2WarmUpCompositor",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<Prerender2WarmUpCompositorTriggerPoint>::Option
    prerender2_warm_up_compositor_trigger_point[] = {
        {Prerender2WarmUpCompositorTriggerPoint::kDidCommitLoad,
         "did_commit_load"},
        {Prerender2WarmUpCompositorTriggerPoint::
             kDidDispatchDOMContentLoadedEvent,
         "did_dispatch_dom_content_loaded_event"},
        {Prerender2WarmUpCompositorTriggerPoint::kDidFinishLoad,
         "did_finish_load"},
};
const base::FeatureParam<Prerender2WarmUpCompositorTriggerPoint>
    kPrerender2WarmUpCompositorTriggerPoint{
        &kPrerender2WarmUpCompositor, "trigger_point",
        Prerender2WarmUpCompositorTriggerPoint::kDidCommitLoad,
        &prerender2_warm_up_compositor_trigger_point};

// Enable limiting previews loading hints to specific resource types.
BASE_FEATURE(kPreviewsResourceLoadingHintsSpecificResourceTypes,
             "PreviewsResourceLoadingHintsSpecificResourceTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kPrewarmDefaultFontFamilies,
             "PrewarmDefaultFontFamilies",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrewarmStandard = {&kPrewarmDefaultFontFamilies,
                                                   "prewarm_standard", false};
const base::FeatureParam<bool> kPrewarmFixed = {&kPrewarmDefaultFontFamilies,
                                                "prewarm_fixed", false};
const base::FeatureParam<bool> kPrewarmSerif = {&kPrewarmDefaultFontFamilies,
                                                "prewarm_serif", true};
const base::FeatureParam<bool> kPrewarmSansSerif = {
    &kPrewarmDefaultFontFamilies, "prewarm_sans_serif", true};
const base::FeatureParam<bool> kPrewarmCursive = {&kPrewarmDefaultFontFamilies,
                                                  "prewarm_cursive", false};
const base::FeatureParam<bool> kPrewarmFantasy = {&kPrewarmDefaultFontFamilies,
                                                  "prewarm_fantasy", false};
#endif

// Enables the Private Aggregation API. Note that this API also requires the
// `kPrivacySandboxAggregationService` to be enabled to successfully send
// reports.
BASE_FEATURE(kPrivateAggregationApi,
             "PrivateAggregationApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Selectively allows the JavaScript API to be disabled in just one of the
// contexts. The Protected Audience param's name has not been updated (from
// "fledge") for consistency across versions
constexpr base::FeatureParam<bool> kPrivateAggregationApiEnabledInSharedStorage{
    &kPrivateAggregationApi, "enabled_in_shared_storage",
    /*default_value=*/true};
constexpr base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInProtectedAudience{&kPrivateAggregationApi,
                                                     "enabled_in_fledge",
                                                     /*default_value=*/true};

// Selectively allows the Protected Audience-specific extensions to be disabled.
// The name has not been updated (from "fledge") for consistency across versions
constexpr base::FeatureParam<bool>
    kPrivateAggregationApiProtectedAudienceExtensionsEnabled{
        &kPrivateAggregationApi, "fledge_extensions_enabled",
        /*default_value=*/true};

// Selectively allows the debug mode to be disabled while leaving the rest of
// the API in place. If disabled, any `enableDebugMode()` calls will essentially
// have no effect.
constexpr base::FeatureParam<bool> kPrivateAggregationApiDebugModeEnabledAtAll{
    &kPrivateAggregationApi, "debug_mode_enabled_at_all",
    /*default_value=*/true};

// Adds some additional functionality (new reserved event types, base values)
// to things enabled by
// kPrivateAggregationApiProtectedAudienceExtensionsEnabled.
BASE_FEATURE(kPrivateAggregationApiProtectedAudienceAdditionalExtensions,
             "PrivateAggregationApiProtectedAudienceAdditionalExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProcessHtmlDataImmediately,
             "ProcessHtmlDataImmediately",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kProduceCompileHints2,
             "ProduceCompileHints2",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kProduceCompileHintsOnIdleDelayParam{
    &kProduceCompileHints2, "delay-in-ms", 10000};
const base::FeatureParam<double> kProduceCompileHintsNoiseLevel{
    &kProduceCompileHints2, "noise-probability", 0.5};
const base::FeatureParam<double> kProduceCompileHintsDataProductionLevel{
    &kProduceCompileHints2, "data-production-probability", 0.005};
BASE_FEATURE(kForceProduceCompileHints,
             "ForceProduceCompileHints",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsumeCompileHints,
             "ConsumeCompileHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLocalCompileHints,
             "LocalCompileHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuoteEmptySecChUaStringHeadersConsistently,
             "QuoteEmptySecChUaStringHeadersConsistently",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reduce the amount of information in the default 'referer' header for
// cross-origin requests.
BASE_FEATURE(kReducedReferrerGranularity,
             "ReducedReferrerGranularity",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kUserAgentFrozenBuildVersion{
    &kReduceUserAgentMinorVersion, "build_version", "0"};

const base::FeatureParam<bool> kAllExceptLegacyWindowsPlatform = {
    &kReduceUserAgentPlatformOsCpu, "all_except_legacy_windows_platform", true};
const base::FeatureParam<bool> kLegacyWindowsPlatform = {
    &kReduceUserAgentPlatformOsCpu, "legacy_windows_platform", true};

// When enabled, Source Location blocking BFCache is captured
// to send it to the browser.
BASE_FEATURE(kRegisterJSSourceLocationBlockingBFCache,
             "RegisterJSSourceLocationBlockingBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveAuthroizationOnCrossOriginRedirect,
             "RemoveAutorizationOnCrossOriginRedirect",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInteractiveDetectorIgnoreFcp,
             "InteractiveDetectorIgnoreFcp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRenderBlockingFonts,
             "RenderBlockingFonts",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxBlockingTimeMsForRenderBlockingFonts(
    &features::kRenderBlockingFonts,
    "max-blocking-time",
    1500);

const base::FeatureParam<int> kMaxFCPDelayMsForRenderBlockingFonts(
    &features::kRenderBlockingFonts,
    "max-fcp-delay",
    100);

BASE_FEATURE(kRenderSizeInScoreAdBrowserSignals,
             "RenderSizeInScoreAdBrowserSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingInputEvents,
             "ResamplingInputEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingScrollEvents,
             "ResamplingScrollEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRunTextInputUpdatePostLifecycle,
             "RunTextInputUpdatePostLifecycle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistFTPToRegisterProtocolHandler,
             "SafelistFTPToRegisterProtocolHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistPaytoToRegisterProtocolHandler,
             "SafelistPaytoToRegisterProtocolHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to enable using the smallest image specified within image srcset
// for users with Save Data enabled.
BASE_FEATURE(kSaveDataImgSrcset,
             "SaveDataImgSrcset",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPausePagesPerBrowsingContextGroup,
             "PausePagesPerBrowsingContextGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowHudDisplayForPausedPages,
             "ShowHudDisplayForPausedPages",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls script streaming for http and https scripts.
BASE_FEATURE(kScriptStreaming,
             "ScriptStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables script streaming for non-http scripts.
BASE_FEATURE(kScriptStreamingForNonHTTP,
             "ScriptStreamingForNonHTTP",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSelectiveInOrderScript,
             "SelectiveInOrderScript",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSelectiveInOrderScriptTarget,
             "SelectiveInOrderScriptTarget",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kSelectiveInOrderScriptAllowList{
    &kSelectiveInOrderScriptTarget, "allow_list", ""};

// When enabled, the SubresourceFilter receives calls from the ResourceLoader
// to perform additional checks against any aliases found from DNS CNAME records
// for the requested URL.
BASE_FEATURE(kSendCnameAliasesToSubresourceFilterFromRenderer,
             "SendCnameAliasesToSubresourceFilterFromRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
BASE_FEATURE(kServiceWorkerUpdateDelay,
             "ServiceWorkerUpdateDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If disabled, client_id and resultingClientId behavior keeps the old
// Chromium behavior even after the PlzDedicatedWorker is enabled.
// This is workaround for crbug.com/1520512 until the fix gets ready.
BASE_FEATURE(kServiceWorkerClientIdAlignedWithSpec,
             "ServiceWorkerClientIdAlignedWithSpec",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the "not" condition become usable in the ServiceWorker
// static routing API.
BASE_FEATURE(kServiceWorkerStaticRouterNotConditionEnabled,
             "ServiceWorkerStaticRouterNotConditionEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, beacons (and friends) have ResourceLoadPriority::kLow,
// not ResourceLoadPriority::kVeryLow.
BASE_FEATURE(kSetLowPriorityForBeacon,
             "SetLowPriorityForBeacon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the setTimeout(..., 0) will not clamp to 1ms.
// Tracking bug: https://crbug.com/402694.
BASE_FEATURE(kSetTimeoutWithoutClamp,
             "SetTimeoutWithoutClamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the shared storage API. Note that enabling this feature does not
// automatically expose this API to the web, it only allows the element to be
// enabled by the runtime enabled feature, for origin trials.
// https://github.com/pythagoraskitty/shared-storage/blob/main/README.md
BASE_FEATURE(kSharedStorageAPI,
             "SharedStorageAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit{
        &kSharedStorageAPI, "url_selection_operation_input_url_size_limit", 8};
// We use a max of 5 MB = 5 * 1024 * 1024 B = 5242880 B.
const base::FeatureParam<int> kMaxSharedStorageBytesPerOrigin = {
    &kSharedStorageAPI, "MaxSharedStorageBytesPerOrigin", 5242880};
const base::FeatureParam<int> kMaxSharedStoragePageSize = {
    &kSharedStorageAPI, "MaxSharedStoragePageSize", 4096};
const base::FeatureParam<int> kMaxSharedStorageCacheSize = {
    &kSharedStorageAPI, "MaxSharedStorageCacheSize", 1024};
const base::FeatureParam<int> kMaxSharedStorageInitTries = {
    &kSharedStorageAPI, "MaxSharedStorageInitTries", 2};
const base::FeatureParam<int> kMaxSharedStorageIteratorBatchSize = {
    &kSharedStorageAPI, "MaxSharedStorageIteratorBatchSize", 100};
const base::FeatureParam<int> kSharedStorageBitBudget = {
    &kSharedStorageAPI, "SharedStorageBitBudget", 12};
const base::FeatureParam<base::TimeDelta> kSharedStorageBudgetInterval = {
    &kSharedStorageAPI, "SharedStorageBudgetInterval", base::Hours(24)};
const base::FeatureParam<base::TimeDelta>
    kSharedStorageStalePurgeInitialInterval = {
        &kSharedStorageAPI, "SharedStorageStalePurgeInitialInterval",
        base::Minutes(2)};
const base::FeatureParam<base::TimeDelta>
    kSharedStorageStalePurgeRecurringInterval = {
        &kSharedStorageAPI, "SharedStorageStalePurgeRecurringInterval",
        base::Hours(2)};
const base::FeatureParam<base::TimeDelta> kSharedStorageStalenessThreshold = {
    &kSharedStorageAPI, "SharedStorageStalenessThreshold", base::Days(30)};
const base::FeatureParam<int>
    kSharedStorageMaxAllowedFencedFrameDepthForSelectURL = {
        &kSharedStorageAPI,
        "SharedStorageMaxAllowedFencedFrameDepthForSelectURL", 1};
// NOTE: To preserve user privacy, the
// `kSharedStorageExposeDebugMessageForSettingsStatus` feature param MUST remain
// false by default.
const base::FeatureParam<bool>
    kSharedStorageExposeDebugMessageForSettingsStatus = {
        &kSharedStorageAPI, "ExposeDebugMessageForSettingsStatus", false};

BASE_FEATURE(kSharedStorageSelectURLLimit,
             "SharedStorageSelectURLLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kSharedStorageSelectURLBitBudgetPerPageLoad = {
    &kSharedStorageSelectURLLimit, "SharedStorageSelectURLBitBudgetPerPageLoad",
    12};
const base::FeatureParam<int>
    kSharedStorageSelectURLBitBudgetPerSitePerPageLoad = {
        &kSharedStorageSelectURLLimit,
        "SharedStorageSelectURLBitBudgetPerSitePerPageLoad", 6};

BASE_FEATURE(kSharedStorageWorkletSharedBackingThreadImplementation,
             "SharedStorageWorkletSharedBackingThreadImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageAPIM118,
             "SharedStorageAPIM118",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageAPIM125,
             "SharedStorageAPIM125",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageCrossOriginScript,
             "SharedStorageCrossOriginScript",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageCreateWorkletUseContextOriginByDefault,
             "SharedStorageCreateWorkletUseContextOriginByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageAPIEnableWALForDatabase,
             "SharedStorageAPIEnableWALForDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimplifyLoadingTransparentPlaceholderImage,
             "SimplifyLoadingTransparentPlaceholderImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

BASE_FEATURE(kSpeculationRulesPrefetchFuture,
             "SpeculationRulesPrefetchFuture",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable service worker warming-up feature. (https://crbug.com/1431792)
BASE_FEATURE(kSpeculativeServiceWorkerWarmUp,
             "SpeculativeServiceWorkerWarmUp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// kSpeculativeServiceWorkerWarmUp warms up service workers up to this max
// count.
const base::FeatureParam<int> kSpeculativeServiceWorkerWarmUpMaxCount{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_max_count", 10};

// Duration to keep worker warmed-up.
const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpDuration{&kSpeculativeServiceWorkerWarmUp,
                                            "sw_warm_up_duration",
                                            base::Minutes(10)};

// Warms up service workers when a pointerover event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerover{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerover", true};

// Warms up service workers when a pointerdown event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerdown{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerdown", true};

// Warms up service worker after service worker is stopped on idle timeout.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnIdleTimeout{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_idle_timeout", true};

// If enabled, ServiceWorkerStorage suppresses posting tasks when it is
// possible. This behavior is expected to improve performance by getting rid of
// redundant posting tasks.
BASE_FEATURE(kServiceWorkerStorageSuppressPostTask,
             "ServiceWorkerStorageSuppressPostTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, force renderer process foregrounded from CommitNavigation to
// DOMContentLoad (crbug/351953350).
BASE_FEATURE(kBoostRenderProcessForLoading,
             "BoostRenderProcessForLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// An empty json array means that this feature is applied unconditionally. If
// specified, it means that the specified URLs will be the target of the new
// behavior.
const base::FeatureParam<std::string> kBoostRenderProcessForLoadingTargetUrls{
    &kBoostRenderProcessForLoading, "target_urls", "[]"};

// If true is specified, kBoostRenderProcessForLoading feature also prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerendering,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering",
                   false);

// If true is specified, kBoostRenderProcessForLoading feature only prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerenderingOnly,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering_only",
                   false);

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
BASE_FEATURE(kStreamlineRendererInit,
             "StreamlineRendererInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSubSampleWindowProxyUsageMetrics,
             "SubSampleWindowProxyUsageMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStylusPointerAdjustment,
             "StylusPointerAdjustment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStylusRichGestures,
             "StylusRichGestures",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables third party script regex matching for detecting technologies.
BASE_FEATURE(kThirdPartyScriptDetection,
             "ThirdPartyScriptDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedBodyLoader,
             "ThreadedBodyLoader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedPreloadScanner,
             "ThreadedPreloadScanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable throttling of fetch() requests from service workers in the
// installing state.  The limit of 3 was chosen to match the limit
// in background main frames.  In addition, trials showed that this
// did not cause excessive timeouts and resulted in a net improvement
// in successful install rate on some platforms.
BASE_FEATURE(kThrottleInstallingServiceWorker,
             "ThrottleInstallingServiceWorker",
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
             "ThrottleUnimportantFrameTimers",
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

BASE_FEATURE(kTimedHTMLParserBudget,
             "TimedHTMLParserBudget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTreatHTTPExpiresHeaderValueZeroAsExpiredInBlink,
             "TreatHTTPExpiresHeaderValueZeroAsExpiredInBlink",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes behavior of User-Agent Client Hints to send blank headers when the
// User-Agent string is overridden, instead of disabling the headers altogether.
BASE_FEATURE(kUACHOverrideBlank,
             "UACHOverrideBlank",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the body of `EmulateLoadStartedForInspector` is executed only
// once per Resource per ResourceFetcher, and thus duplicated network load
// entries in DevTools caused by `EmulateLoadStartedForInspector` are removed.
// https://crbug.com/1502591
BASE_FEATURE(kEmulateLoadStartedForInspectorOncePerResource,
             "kEmulateLoadStartedForInspectorOncePerResource",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlinkSchedulerDiscreteInputMatchesResponsivenessMetrics,
             "BlinkSchedulerDiscreteInputMatchesResponsivenessMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedScrollPreventRenderingStarvation,
             "ThreadedScrollPreventRenderingStarvation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the usage of unload handlers causes a blocklisted reason for
// BFCache. The purpose is to capture their source location.
BASE_FEATURE(kUnloadBlocklisted,
             "UnloadBlocklisted",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use 'TexImage2D' instead of 'TexStorage2DEXT' when creating a
// staging texture for |DrawingBuffer|. This is a killswitch; remove when
// launched.
BASE_FEATURE(kUseImageInsteadOfStorageForStagingBuffer,
             "UseImageInsteadOfStorageForStagingBuffer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BASE_FEATURE(kUsePageViewportInLCP,
             "UsePageViewportInLCP",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabling this will cause parkable strings to use Snappy for compression iff
// kCompressParkableStrings is enabled.
BASE_FEATURE(kUseSnappyForParkableStrings,
             "UseSnappyForParkableStrings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the zstd compression algorithm for ParkableString compression.
BASE_FEATURE(kUseZstdForParkableStrings,
             "UseZstdForParkableStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kUseThreadPoolForMediaStreamVideoTaskRunner,
             "UseThreadPoolForMediaStreamVideoTaskRunner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVSyncDecoding,
             "VSyncDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVSyncDecodingHiddenOccludedTickDuration,
                   &kVSyncDecoding,
                   "occluded_tick_duration",
                   base::Hertz(10));

BASE_FEATURE(kVSyncEncoding,
             "VSyncEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcUseCaptureBeginTimestamp,
             "WebRtcUseCaptureBeginTimestamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcAudioSinkUseTimestampAligner,
             "WebRtcAudioSinkUseTimestampAligner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable borderless mode for desktop PWAs. go/borderless-mode
BASE_FEATURE(kWebAppBorderless,
             "WebAppBorderless",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls scope extensions feature in web apps. Controls parsing of
// "scope_extensions" field in web app manifests. See explainer for more
// information:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
BASE_FEATURE(kWebAppEnableScopeExtensions,
             "WebAppEnableScopeExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls URL handling feature in web apps. Controls parsing of "url_handlers"
// field in web app manifests. See explainer for more information:
// https://github.com/WICG/pwa-url-handler/blob/main/explainer.md
BASE_FEATURE(kWebAppEnableUrlHandlers,
             "WebAppEnableUrlHandlers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls parsing of the "lock_screen" dictionary field and its "start_url"
// entry in web app manifests.  See explainer for more information:
// https://github.com/WICG/lock-screen/
// Note: the lock screen API and OS integration is separately controlled by
// the content feature `kWebLockScreenApi`.
BASE_FEATURE(kWebAppManifestLockScreen,
             "WebAppManifestLockScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabling this flag bypasses additional buffering when using the Web Audio
// API, which may reduce audio output latency but may also increase the
// probability of an audio glitch.
BASE_FEATURE(kWebAudioBypassOutputBuffering,
             "WebAudioBypassOutputBuffering",
             base::FEATURE_DISABLED_BY_DEFAULT);

/// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
BASE_FEATURE(kWebFontsCacheAwareTimeoutAdaption,
             "WebFontsCacheAwareTimeoutAdaption",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread,
             "WebRtcCombinedNetworkAndWorkerThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/355256378): OpenH264 for encoding and FFmpeg for H264 decoding
// should be detangled such that software decoding can be enabled without
// software encoding.
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && \
    BUILDFLAG(ENABLE_OPENH264)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
BASE_FEATURE(kWebRtcH264WithOpenH264FFmpeg,
             "WebRTC-H264WithOpenH264FFmpeg",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) &&
        // BUILDFLAG(ENABLE_OPENH264)

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
BASE_FEATURE(kWebRtcHideLocalIpsWithMdns,
             "WebRtcHideLocalIpsWithMdns",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Causes WebRTC to not set the color space of video frames on the receive side
// in case it's unspecified. Otherwise we will guess that the color space is
// BT709. http://crbug.com/1129243
BASE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace,
             "WebRtcIgnoreUnspecifiedColorSpace",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcThreadsUseResourceEfficientType,
             "WebRtcThreadsUseResourceEfficientType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Instructs WebRTC to honor the Min/Max Video Encode Accelerator dimensions.
BASE_FEATURE(kWebRtcUseMinMaxVEADimensions,
             "WebRtcUseMinMaxVEADimensions",
// TODO(crbug.com/1008491): enable other platforms.
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Allow access to WebSQL APIs.
BASE_FEATURE(kWebSQLAccess, "kWebSQLAccess", base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for https://crbug.com/338955051.
BASE_FEATURE(kWebUSBTransferSizeLimit,
             "WebUSBTransferSizeLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables small accelerated canvases for webview (crbug.com/1004304)
BASE_FEATURE(kWebviewAccelerateSmallCanvases,
             "WebviewAccelerateSmallCanvases",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

bool IsParkableImagesToDiskEnabled() {
  return base::FeatureList::IsEnabled(kParkableImagesToDisk);
}

bool IsSetTimeoutWithoutClampEnabled() {
  return base::FeatureList::IsEnabled(features::kSetTimeoutWithoutClamp);
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

bool IsCanvasSharedBitmapConversionEnabled() {
  return base::FeatureList::IsEnabled(
      features::kCanvasSharedBitmapToSharedImage);
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
