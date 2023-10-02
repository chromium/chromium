// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace features {

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

BASE_FEATURE(kAcceleratedStaticBitmapImageSerialization,
             "AcceleratedStaticBitmapImageSerialization",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the Protected Audience's reporting with ad macro API.
BASE_FEATURE(kAdAuctionReportingWithMacroApi,
             "AdAuctionReportingWithMacroApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BASE_FEATURE(kAdAuctionSignals,
             "AdAuctionSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kAdAuctionSignalsMaxSizeBytes{
    &kAdAuctionSignals, "ad-auction-signals-max-size-bytes", 10000};

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Changes default Permissions Policy for features join-ad-interest-group and
// run-ad-auction to a more restricted EnableForSelf.
BASE_FEATURE(kAdInterestGroupAPIRestrictedPolicyByDefault,
             "AdInterestGroupAPIRestrictedPolicyByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Make all pending 'display: auto' web fonts enter the swap or failure period
// immediately before reaching the LCP time limit (~2500ms), so that web fonts
// do not become a source of bad LCP.
BASE_FEATURE(kAlignFontDisplayAutoTimeoutWithLCPGoal,
             "AlignFontDisplayAutoTimeoutWithLCPGoal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The amount of time allowed for 'display: auto' web fonts to load without
// intervention, counted from navigation start.
const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "lcp-limit-in-ms", 2000};

const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>::Option
    align_font_display_auto_timeout_with_lcp_goal_modes[] = {
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToFailurePeriod,
         "failure"},
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod, "swap"}};
const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "intervention-mode",
        AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod,
        &align_font_display_auto_timeout_with_lcp_goal_modes};

BASE_FEATURE(kAllowDevToolsMainThreadDebuggerForMultipleMainFrames,
             "AllowDevToolsMainThreadDebuggerForMultipleMainFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
BASE_FEATURE(kAllowDropAlphaForMediaStream,
             "AllowDropAlphaForMediaStream",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(https://crbug.com/1331187): Delete the flag.
BASE_FEATURE(kAllowPageWithIDBConnectionInBFCache,
             "AllowPageWithIDBConnectionInBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(https://crbug.com/1331187): Delete the flag.
BASE_FEATURE(kAllowPageWithIDBTransactionInBFCache,
             "AllowPageWithIDBTransactionInBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowSourceSwitchOnPausedVideoMediaStream,
             "AllowSourceSwitchOnPausedVideoMediaStream",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows for synchronous XHR requests during page dismissal
BASE_FEATURE(kAllowSyncXHRInPageDismissal,
             "AllowSyncXHRInPageDismissal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
BASE_FEATURE(kAllowURNsInIframes,
             "AllowURNsInIframes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Anchor Element Interaction
BASE_FEATURE(kAnchorElementInteraction,
             "AnchorElementInteraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable anchor element mouse motion estimator.
BASE_FEATURE(kAnchorElementMouseMotionEstimator,
             "AnchorElementMouseMotionEstimator",
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

// If enabled, whenever form controls are removed from the DOM, the ChromeClient
// is informed about this. This enables Autofill to trigger a reparsing of
// forms.
BASE_FEATURE(kAutofillDetectRemovedFormControls,
             "AutofillDetectRemovedFormControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If disabled (default for many years), autofilling triggers KeyDown and
// KeyUp events that do not send any key codes. If enabled, these events
// contain the "Unidentified" key.
BASE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill,
             "AutofillSendUnidentifiedKeyAfterFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will start identifying web elements using DOMNodeIds
// instead of static counters.
BASE_FEATURE(kAutofillUseDomNodeIdForRendererId,
             "AutofillUseDomNodeIdForRendererId",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Apply lazy-loading to ad frames which have embeds likely impacting Core Web
// Vitals.
BASE_FEATURE(kAutomaticLazyFrameLoadingToAds,
             "AutomaticLazyFrameLoadingToAds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The timeout value that forces loading iframes that are lazy loaded by
// LazyAds. After this timeout, the frame loading is triggered even when the
// intersection observer does not trigger iframe loading.
const base::FeatureParam<int> kTimeoutMillisForLazyAds(
    &features::kAutomaticLazyFrameLoadingToAds,
    "timeout",
    0);

// Skip applying LazyAds for the first "skip_frame_count" frames in the
// document, and apply LazyAds the rest if they are eligible.
const base::FeatureParam<int> kSkipFrameCountForLazyAds(
    &features::kAutomaticLazyFrameLoadingToAds,
    "skip_frame_count",
    0);

// Apply lazy-loading to frames which have embeds likely impacting Core Web
// Vitals.
BASE_FEATURE(kAutomaticLazyFrameLoadingToEmbeds,
             "AutomaticLazyFrameLoadingToEmbeds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The timeout value that forces loading iframes that are lazy loaded by
// LazyEmbeds. After this timeout, the frame loading is triggered even when the
// intersection observer does not trigger iframe loading.
const base::FeatureParam<int> kTimeoutMillisForLazyEmbeds(
    &features::kAutomaticLazyFrameLoadingToEmbeds,
    "timeout",
    0);

// Skip applying LazyEmbeds for the first "skip_frame_count" frames in the
// document, and apply LazyEmbeds the rest if they are eligible.
const base::FeatureParam<int> kSkipFrameCountForLazyEmbeds(
    &features::kAutomaticLazyFrameLoadingToEmbeds,
    "skip_frame_count",
    0);

// Define the allowed websites to use LazyEmbeds. The allowed websites need to
// be defined separately from kAutomaticLazyFrameLoadingToEmbeds because we want
// to gather Blink.AutomaticLazyLoadFrame.LazyEmbedFrameCount UKM data even when
// kAutomaticLazyFrameLoadingToEmbeds is disabled.
BASE_FEATURE(kAutomaticLazyFrameLoadingToEmbedUrls,
             "AutomaticLazyFrameLoadingToEmbedUrls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Define the strategy for LazyEmbeds to decide which frames we apply
// lazy-loading or not. If the loading strategy is kAllowList, the detection
// logic is based on the allowlist that kAutomaticLazyFrameLoadingToEmbedUrls
// passes to the client. If the strategy is kNonAds, the detection logic is
// based on the Ad Tagging in chromium.
const base::FeatureParam<AutomaticLazyFrameLoadingToEmbedLoadingStrategy>::
    Option kAutomaticLazyFrameLoadingToEmbedLoadingStrategies[] = {
        {AutomaticLazyFrameLoadingToEmbedLoadingStrategy::kAllowList,
         "allow_list"},
        {AutomaticLazyFrameLoadingToEmbedLoadingStrategy::kNonAds, "non_ads"}};
const base::FeatureParam<AutomaticLazyFrameLoadingToEmbedLoadingStrategy>
    kAutomaticLazyFrameLoadingToEmbedLoadingStrategyParam{
        &kAutomaticLazyFrameLoadingToEmbedUrls, "strategy",
        AutomaticLazyFrameLoadingToEmbedLoadingStrategy::kAllowList,
        &kAutomaticLazyFrameLoadingToEmbedLoadingStrategies};

BASE_FEATURE(kAvifGainmapHdrImages,
             "AvifGainmapHdrImages",
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
// Due to an issue in how prevWins were stored this flag should not be enabled
// until July 2023.
BASE_FEATURE(kBiddingAndScoringDebugReportingAPI,
             "BiddingAndScoringDebugReportingAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Blink garbage collection.
// Enables compaction of backing stores on Blink's heap.
BASE_FEATURE(kBlinkHeapCompaction,
             "BlinkHeapCompaction",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables concurrently marking Blink's heap.
BASE_FEATURE(kBlinkHeapConcurrentMarking,
             "BlinkHeapConcurrentMarking",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables concurrently sweeping Blink's heap.
BASE_FEATURE(kBlinkHeapConcurrentSweeping,
             "BlinkHeapConcurrentSweeping",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables incrementally marking Blink's heap.
BASE_FEATURE(kBlinkHeapIncrementalMarking,
             "BlinkHeapIncrementalMarking",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables a marking stress mode that schedules more garbage collections and
// also adds additional verification passes.
BASE_FEATURE(kBlinkHeapIncrementalMarkingStress,
             "BlinkHeapIncrementalMarkingStress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
BASE_FEATURE(kBlockingDownloadsInAdFrameWithoutUserActivation,
             "BlockingDownloadsInAdFrameWithoutUserActivation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BASE_FEATURE(kBoostImagePriority,
             "BoostImagePriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The number of images to bopost the priority of before returning
// to the default (low) priority.
const base::FeatureParam<int> kBoostImagePriorityImageCount{
    &kBoostImagePriority, "image_count", 5};
// Maximum size of an image (in px^2) to be considered "small".
// Small images, where dimensions are specified in the markup, are not boosted.
const base::FeatureParam<int> kBoostImagePriorityImageSize{&kBoostImagePriority,
                                                           "image_size", 10000};
// Number of medium-priority requests to allow in tight-mode independent of the
// total number of outstanding requests.
const base::FeatureParam<int> kBoostImagePriorityTightMediumLimit{
    &kBoostImagePriority, "tight_medium_limit", 2};

// https://github.com/patcg-individual-drafts/topics
// Kill switch for the Topics API.
BASE_FEATURE(kBrowsingTopics,
             "BrowsingTopics",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// The number of epochs from where to calculate the topics to give to a
// requesting contexts.
const base::FeatureParam<int> kBrowsingTopicsNumberOfEpochsToExpose{
    &kBrowsingTopicsParameters, "number_of_epochs_to_expose", 3};
// The periodic topics calculation interval.
const base::FeatureParam<base::TimeDelta> kBrowsingTopicsTimePeriodPerEpoch{
    &kBrowsingTopicsParameters, "time_period_per_epoch", base::Days(7)};
// The number of top topics to derive and to keep for each epoch (week).
const base::FeatureParam<int> kBrowsingTopicsNumberOfTopTopicsPerEpoch{
    &kBrowsingTopicsParameters, "number_of_top_topics_per_epoch", 5};
// The probability (in percent number) to return the random topic to a site. The
// "random topic" is per-site, and is selected from the full taxonomy uniformly
// at random, and each site has a
// `kBrowsingTopicsUseRandomTopicProbabilityPercent`% chance to see their random
// topic instead of one of the top topics.
const base::FeatureParam<int> kBrowsingTopicsUseRandomTopicProbabilityPercent{
    &kBrowsingTopicsParameters, "use_random_topic_probability_percent", 5};
// Maximum duration between when a epoch is calculated and when a site starts
// using that new epoch's topics. The time chosen is a per-site random point in
// time between [calculation time, calculation time + max duration).
const base::FeatureParam<base::TimeDelta>
    kBrowsingTopicsMaxEpochIntroductionDelay{&kBrowsingTopicsParameters,
                                             "max_epoch_introduction_delay",
                                             base::Days(2)};
// How many epochs (weeks) of API usage data (i.e. topics observations) will be
// based off for the filtering of topics for a calling context.
const base::FeatureParam<int>
    kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering{
        &kBrowsingTopicsParameters,
        "number_of_epochs_of_observation_data_to_use_for_filtering", 3};
// The max number of observed-by context domains to keep for each top topic
// during the epoch topics calculation. The final number of domains associated
// with each topic may be larger than this threshold, because that set of
// domains will also include all domains associated with the topic's descendant
// topics. The intent is to cap the in-use memory.
const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic{
        &kBrowsingTopicsParameters,
        "max_number_of_api_usage_context_domains_to_keep_per_topic", 1000};
// The max number of entries allowed to be retrieved from the
// `BrowsingTopicsSiteDataStorage` database for each query for the API usage
// contexts. The query will occur once per epoch (week) at topics calculation
// time. The intent is to cap the peak memory usage.
const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch{
        &kBrowsingTopicsParameters,
        "max_number_of_api_usage_context_entries_to_load_per_epoch", 100000};
// The max number of API usage context domains allowed to be stored per page
// load.
const base::FeatureParam<int>
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad{
        &kBrowsingTopicsParameters,
        "max_number_of_api_usage_context_domains_to_store_per_page_load", 30};
// The taxonomy version. This only affects the topics classification that occurs
// during this browser session, and doesn't affect the pre-existing epochs.
const base::FeatureParam<int> kBrowsingTopicsTaxonomyVersion{
    &kBrowsingTopicsParameters, "taxonomy_version",
    kBrowsingTopicsTaxonomyVersionDefault};
// Comma separated Topic IDs to be blocked. Descendant topics of each blocked
// topic will be blocked as well.
const base::FeatureParam<std::string> kBrowsingTopicsDisabledTopicsList{
    &kBrowsingTopicsParameters, "disabled_topics_list", ""};

// Comma separated list of Topic IDs. Prioritize these topics and their
// descendants during top topic selection.
const base::FeatureParam<std::string> kBrowsingTopicsPrioritizedTopicsList{
    &kBrowsingTopicsParameters, "prioritized_topics_list", ""};

// Enables the deprecatedBrowsingTopics XHR attribute. For this feature to take
// effect, the main Topics feature has to be enabled first (i.e.
// `kBrowsingTopics` is enabled, and, either a valid Origin Trial token exists
// or `kPrivacySandboxAdsAPIsOverride` is enabled.)
BASE_FEATURE(kBrowsingTopicsXHR,
             "BrowsingTopicsXHR",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
BASE_FEATURE(kCORSErrorsIssueOnly,
             "CORSErrorsIssueOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, code cache is produced asynchronously from the script execution
// (https://crbug.com/1260908).
BASE_FEATURE(kCacheCodeOnIdle,
             "CacheCodeOnIdle",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kCacheCodeOnIdleDelayParam{&kCacheCodeOnIdle,
                                                         "delay-in-ms", 1};
// Apply CacheCodeOnIdle only for service workers (https://crbug.com/1410082).
const base::FeatureParam<bool> kCacheCodeOnIdleDelayServiceWorkerOnlyParam{
    &kCacheCodeOnIdle, "service-worker-only", true};

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

BASE_FEATURE(
    kCanvas2DHibernation,
    "Canvas2DHibernation",
#if BUILDFLAG(IS_MAC)
    // Canvas hibernation is not always enabled on MacOS X due to a bug that
    // causes content loss. TODO: Find a better fix for crbug.com/588434
    base::FeatureState::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FeatureState::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Whether to losslessly compress the resulting image after canvas hibernation.
BASE_FEATURE(kCanvasCompressHibernatedImage,
             "CanvasCompressHibernatedImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to aggressively free resources for canvases in background pages.
BASE_FEATURE(kCanvasFreeMemoryWhenHidden,
             "CanvasFreeMemoryWhenHidden",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckHTMLParserBudgetLessOften,
             "CheckHTMLParserBudgetLessOften",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable `sec-ch-dpr` client hint.
BASE_FEATURE(kClientHintsDPR,
             "ClientHintsDPR",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `dpr` client hint.
BASE_FEATURE(kClientHintsDPR_DEPRECATED,
             "ClientHintsDPR_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `sec-ch-device-memory` client hint.
BASE_FEATURE(kClientHintsDeviceMemory,
             "ClientHintsDeviceMemory",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `device-memory` client hint.
BASE_FEATURE(kClientHintsDeviceMemory_DEPRECATED,
             "ClientHintsDeviceMemory_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `form-factor` client hint.
BASE_FEATURE(kClientHintsFormFactor,
             "ClientHintsFormFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable `sec-ch-prefers-reduced-transparency` client hint.
BASE_FEATURE(kClientHintsPrefersReducedTransparency,
             "ClientHintsPrefersReducedTransparency",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `sec-ch-width` client hint.
BASE_FEATURE(kClientHintsResourceWidth,
             "ClientHintsResourceWidth",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `width` client hint.
BASE_FEATURE(kClientHintsResourceWidth_DEPRECATED,
             "ClientHintsResourceWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `save-data` client hint.
BASE_FEATURE(kClientHintsSaveData,
             "ClientHintsSaveData",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `sec-ch-viewport-width` client hint.
BASE_FEATURE(kClientHintsViewportWidth,
             "ClientHintsViewportWidth",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `viewport-width` client hint.
BASE_FEATURE(kClientHintsViewportWidth_DEPRECATED,
             "ClientHintsViewportWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClipboardUnsanitizedContent,
             "ClipboardUnsanitizedContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
BASE_FEATURE(kCompressParkableStrings,
             "CompressParkableStrings",
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

// When enabled, add a new option, {imageOrientation: 'none'}, to
// createImageBitmap, which ignores the image orientation metadata of the source
// and renders the image as encoded.
BASE_FEATURE(kCreateImageBitmapOrientationNone,
             "CreateImageBitmapOrientationNone",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDOMContentLoadedWaitForAsyncScript,
             "DOMContentLoadedWaitForAsyncScript",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDecodeScriptSourceOffThread,
             "DecodeScriptSourceOffThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, pages that don't specify a layout width will default to the
// window width rather than the traditional mobile fallback width of 980px.
// Has no effect unless viewport handling is enabled.
BASE_FEATURE(kDefaultViewportIsDeviceWidth,
             "DefaultViewportIsDeviceWidth",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
};

const base::FeatureParam<DelayAsyncScriptDelayType>
    kDelayAsyncScriptExecutionDelayParam{
        &kDelayAsyncScriptExecution, "delay_async_exec_delay_type",
        DelayAsyncScriptDelayType::kFinishedParsing,
        &delay_async_script_execution_delay_types};

const base::FeatureParam<DelayAsyncScriptTarget>::Option
    delay_async_script_target_types[] = {
        {DelayAsyncScriptTarget::kAll, "all"},
        {DelayAsyncScriptTarget::kCrossSiteOnly, "cross_site_only"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowList,
         "cross_site_with_allow_list"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowListReportOnly,
         "cross_site_with_allow_list_report_only"},
};
const base::FeatureParam<DelayAsyncScriptTarget> kDelayAsyncScriptTargetParam{
    &kDelayAsyncScriptExecution, "delay_async_exec_target",
    DelayAsyncScriptTarget::kAll, &delay_async_script_target_types};

// kDelayAsyncScriptExecution will delay executing async script at max
// |delay_async_exec_delay_limit|.
const base::FeatureParam<base::TimeDelta>
    kDelayAsyncScriptExecutionDelayLimitParam{&kDelayAsyncScriptExecution,
                                              "delay_async_exec_delay_limit",
                                              base::Seconds(0)};

// kDelayAsyncScriptExecution will be disabled after document elapsed more than
// |delay_async_exec_feature_limit|. Zero value means no limit.
// This is to avoid unnecessary async script delay after LCP (for
// kEachLcpCandidate or kEachPaint). Because we can't determine the LCP timing
// while loading, we use timeout instead.
const base::FeatureParam<base::TimeDelta>
    kDelayAsyncScriptExecutionFeatureLimitParam{
        &kDelayAsyncScriptExecution, "delay_async_exec_feature_limit",
        base::Seconds(0)};

const base::FeatureParam<std::string> kDelayAsyncScriptAllowList{
    &kDelayAsyncScriptExecution, "delay_async_exec_allow_list", ""};

const base::FeatureParam<bool> kDelayAsyncScriptExecutionMainFrameOnlyParam{
    &kDelayAsyncScriptExecution, "delay_async_exec_main_frame_only", false};

BASE_FEATURE(kDelayLowPriorityRequestsAccordingToNetworkState,
             "DelayLowPriorityRequestsAccordingToNetworkState",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxNumOfThrottleableRequestsInTightMode{
    &kDelayLowPriorityRequestsAccordingToNetworkState,
    "MaxNumOfThrottleableRequestsInTightMode", 5};

const base::FeatureParam<base::TimeDelta> kHttpRttThreshold{
    &kDelayLowPriorityRequestsAccordingToNetworkState, "HttpRttThreshold",
    base::Milliseconds(450)};

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

BASE_FEATURE(kDisableThirdPartyStoragePartitioningDeprecationTrial,
             "DisableThirdPartyStoragePartitioningDeprecationTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the beforeunload handler is dispatched when a frame is frozen.
// This allows the browser to know whether discarding the frame could result in
// lost user data, at the cost of extra CPU usage. The feature will be removed
// once we have determine whether the CPU cost is acceptable.
BASE_FEATURE(kDispatchBeforeUnloadOnFreeze,
             "DispatchBeforeUnloadOnFreeze",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable Display Locking JavaScript APIs.
BASE_FEATURE(kDisplayLocking,
             "DisplayLocking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Drop input events before user sees first paint https://crbug.com/1255485
BASE_FEATURE(kDropInputEventsBeforeFirstPaint,
             "DropInputEventsBeforeFirstPaint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Drop touch-end dispatch from `InputHandlerProxy` when all other touch-events
// in current interaction sequence are dropeed.
//
// TODO(https://crbug.com/1417126): This is disabled because of a suspicious
// flake in AR/XR tests.
BASE_FEATURE(kDroppedTouchSequenceIncludesTouchEnd,
             "DroppedTouchSequenceIncludesTouchEnd",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
BASE_FEATURE(kEagerCacheStorageSetupForServiceWorkers,
             "EagerCacheStorageSetupForServiceWorkers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEarlyExitOnNoopClassOrStyleChange,
             "EarlyExitOnNoopClassOrStyleChange",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables reporting Event Timing with matching presentation promise index only.
BASE_FEATURE(kEventTimingMatchPresentationIndex,
             "EventTimingMatchPresentationIndex",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables reporting Event Timing entries with a smaller presentation index on
// resolved painted presentation.
BASE_FEATURE(kEventTimingReportAllEarlyEntriesOnPaintedPresentation,
             "EventTimingReportAllEarlyEntriesOnPaintedPresentation",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// A list of hosts for which deprecation of unload is allowed. If it's empty
// the all hosts are allowed.
const base::FeatureParam<std::string> kDeprecateUnloadAllowlist{
    &kDeprecateUnload, "allowlist", ""};

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

// Enable the <fencedframe> element; see crbug.com/1123606. Note that enabling
// this feature does not automatically expose this element to the web, it only
// allows the element to be enabled by the runtime enabled feature, for origin
// trials.
BASE_FEATURE(kFencedFrames, "FencedFrames", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the new fenced frame-related features in M119. (These are
// conditionally dependent on other fenced frame-related feature flags being
// enabled.)
// * Extra format for ad size macro substitution:
//   ${AD_WIDTH} and ${AD_HEIGHT}, on top of the previous
//   {%AD_WIDTH%} and {%AD_HEIGHT%}.
// * Input validation (no disallowed URI component characters) in
//   registerAdMacro keys and values.
// * Send automatic beacons to all registered destinations without requiring
//   event data to be in place.
BASE_FEATURE(kFencedFramesM119Features,
             "FencedFramesM119Features",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kFixGestureScrollQueuingBug,
             "FixGestureScrollQueuingBug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Enables FLEDGE implementation. See https://crbug.com/1186444.
BASE_FEATURE(kFledge, "Fledge", base::FEATURE_DISABLED_BY_DEFAULT);

// See
// https://github.com/WICG/turtledove/blob/main/FLEDGE_browser_bidding_and_auction_API.md
BASE_FEATURE(kFledgeBiddingAndAuctionServer,
             "FledgeBiddingAndAuctionServer",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kFledgeBiddingAndAuctionKeyURL{
    &kFledgeBiddingAndAuctionServer, "FledgeBiddingAndAuctionKeyURL", ""};

// See in the header.
BASE_FEATURE(kFledgeConsiderKAnonymity,
             "FledgeConsiderKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFledgeEnforceKAnonymity,
             "FledgeEnforceKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgePassKAnonStatusToReportWin,
             "FledgePassKAnonStatusToReportWin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgePassRecencyToGenerateBid,
             "FledgePassRecencyToGenerateBid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kForceDeferScriptIntervention,
             "ForceDeferScriptIntervention",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceHighPerformanceGPUForWebGL,
             "ForceHighPerformanceGPUForWebGL",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceInOrderScript,
             "ForceInOrderScript",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceMajorVersionInMinorPositionInUserAgent,
             "ForceMajorVersionInMinorPositionInUserAgent",
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

BASE_FEATURE(kGainmapHdrImages,
             "GainmapHdrImages",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHiddenSelectionBounds,
             "HiddenSelectionBounds",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kImageLoadingPrioritizationFix,
             "ImageLoadingPrioritizationFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIndexedDBCompressValuesWithSnappy,
             "IndexedDBCompressValuesWithSnappy",
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
             base::FEATURE_DISABLED_BY_DEFAULT);
// TODO(crbug.com/1197209): Adjust these limits in response to usage.
const base::FeatureParam<int> kInterestGroupStorageMaxOwners{
    &kInterestGroupStorage, "max_owners", 1000};
const base::FeatureParam<int> kInterestGroupStorageMaxStoragePerOwner{
    &kInterestGroupStorage, "max_storage_per_owner", 10 * 1024 * 1024};
const base::FeatureParam<int> kInterestGroupStorageMaxGroupsPerOwner{
    &kInterestGroupStorage, "max_groups_per_owner", 1000};
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
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<IsolateSandboxedIframesGrouping>::Option
    isolated_sandboxed_iframes_grouping_types[] = {
        {IsolateSandboxedIframesGrouping::kPerSite, "per-site"},
        {IsolateSandboxedIframesGrouping::kPerOrigin, "per-origin"},
        {IsolateSandboxedIframesGrouping::kPerDocument, "per-document"}};
const base::FeatureParam<IsolateSandboxedIframesGrouping>
    kIsolateSandboxedIframesGroupingParam{
        &kIsolateSandboxedIframes, "grouping",
        IsolateSandboxedIframesGrouping::kPerSite,
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

// Enables reporting as LCP of the time the first frame of an animated image was
// painted.
BASE_FEATURE(kLCPAnimatedImagesReporting,
             "LCPAnimatedImagesReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLCPCriticalPathPredictor,
             "LCPCriticalPathPredictor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kLCPCriticalPathPredictorDryRun{
    &kLCPCriticalPathPredictor, "lcpp_dry_run", false};

const base::FeatureParam<int> kLCPCriticalPathPredictorMaxElementLocatorLength{
    &kLCPCriticalPathPredictor, "lcpp_max_element_locator_length", 1024};

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

const base::FeatureParam<LcppResourceLoadPriority>
    kLCPCriticalPathPredictorInfluencerScriptLoadPriority{
        &kLCPCriticalPathPredictor, "lcpp_script_load_priority",
        LcppResourceLoadPriority::kVeryHigh, &lcpp_resource_load_priorities};

BASE_FEATURE(kLCPScriptObserver,
             "LCPScriptObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLCPScriptObserverMaxUrlLength{
    &kLCPScriptObserver, "lcpp_max_url_length", 1024};

const base::FeatureParam<int> kLCPScriptObserverMaxUrlCountPerOrigin{
    &kLCPScriptObserver, "lcpp_max_url_count_per_origin", 5};

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

// Enables reporting as LCP of the time the first frame of a video was painted.
BASE_FEATURE(kLCPVideoFirstFrame,
             "LCPVideoFirstFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the use of GpuMemoryBuffer images for low latency 2d canvas.
// TODO(khushalsagar): Enable this if we're using SurfaceControl and GMBs allow
// us to overlay these resources.
BASE_FEATURE(kLowLatencyCanvas2dImageChromium,
             "LowLatencyCanvas2dImageChromium",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
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
const base::FeatureParam<base::TimeDelta>
    kLowPriorityAsyncScriptExecutionFeatureLimitParam{
        &kLowPriorityAsyncScriptExecution, "low_pri_async_exec_feature_limit",
        base::Seconds(0)};

// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam{
        &kLowPriorityAsyncScriptExecution, "low_pri_async_exec_cross_site_only",
        false};

const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionMainFrameOnlyParam{
        &kLowPriorityAsyncScriptExecution, "low_pri_async_exec_main_frame_only",
        false};

// kLowPriorityAsyncScriptExecution will exclude scripts that influence LCP
// element.
const base::FeatureParam<bool>
    kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam{
        &kLowPriorityAsyncScriptExecution,
        "low_pri_async_exec_exclude_lcp_influencers", false};

BASE_FEATURE(kLowPriorityScriptLoading,
             "LowPriorityScriptLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kLowPriorityScriptLoadingCrossSiteOnlyParam{
    &kLowPriorityScriptLoading, "low_pri_async_loading_cross_site_only", false};
const base::FeatureParam<base::TimeDelta>
    kLowPriorityScriptLoadingFeatureLimitParam{
        &kLowPriorityScriptLoading, "low_pri_async_loading_feature_limit",
        base::Seconds(0)};
const base::FeatureParam<std::string> kLowPriorityScriptLoadingDenyListParam{
    &kLowPriorityScriptLoading, "low_pri_async_loading_deny_list", ""};
const base::FeatureParam<bool> kLowPriorityScriptLoadingMainFrameOnlyParam{
    &kLowPriorityScriptLoading, "low_pri_async_loading_main_frame_only", false};

BASE_FEATURE(kMainThreadHighPriorityImageLoading,
             "MainThreadHighPriorityImageLoading",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the setTimeout(..., 0) will clamp to 4ms after a custom `nesting`
// level.
// Tracking bug: https://crbug.com/1108877.
BASE_FEATURE(kMaxUnthrottledTimeoutNestingLevel,
             "MaxUnthrottledTimeoutNestingLevel",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kMaxUnthrottledTimeoutNestingLevelParam{
    &kMaxUnthrottledTimeoutNestingLevel, "nesting", 15};
BASE_FEATURE(kMixedContentAutoupgrade,
             "AutoupgradeMixedContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReferenceFilterImages,
             "MemoryCacheStrongReferenceFilterImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReferenceFilterScripts,
             "MemoryCacheStrongReferenceFilterScripts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReferenceFilterCrossOriginScripts,
             "MemoryCacheStrongReferenceFilterCrossOriginScripts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReference,
             "MemoryCacheStrongReference",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kMemoryCacheStrongReferenceTotalSizeThresholdParam{
        &kMemoryCacheStrongReference,
        "memory_cache_strong_ref_total_size_threshold", 10 * 1024 * 1024};
const base::FeatureParam<int>
    kMemoryCacheStrongReferenceResourceSizeThresholdParam{
        &kMemoryCacheStrongReference,
        "memory_cache_strong_ref_resource_size_threshold", 3 * 1024 * 1024};

BASE_FEATURE(kMemoryCacheStrongReferenceSingleUnload,
             "MemoryCacheStrongReferenceSingleUnload",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded.
BASE_FEATURE(kNavigationPredictor,
             "NavigationPredictor",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kNewBaseUrlInheritanceBehavior,
             "NewBaseUrlInheritanceBehavior",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoForcedFrameUpdatesForWebTests,
             "NoForcedFrameUpdatesForWebTests",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOriginAgentClusterDefaultEnabled,
             "OriginAgentClusterDefaultEnable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOriginAgentClusterDefaultWarning,
             "OriginAgentClusterDefaultWarning",
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

#if BUILDFLAG(IS_ANDROID)
// A parameter to exclude or not exclude CanvasFontCache from
// PartialLowModeOnMidRangeDevices. This is used to see how
// CanvasFontCache affects graphics smoothness and renderer memory usage.
const base::FeatureParam<bool> kPartialLowEndModeExcludeCanvasFontCache{
    &base::features::kPartialLowEndModeOnMidRangeDevices,
    "exclude-canvas-font-cache", false};
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the use of the PaintCache for Path2D objects that are rasterized
// out of process.  Has no effect when kCanvasOopRasterization is disabled.
BASE_FEATURE(kPath2DPaintCache,
             "Path2DPaintCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPendingBeaconAPI,
             "PendingBeaconAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPendingBeaconAPIRequiresOriginTrial = {
    &kPendingBeaconAPI, "requires_origin_trial", false};
const base::FeatureParam<bool> kPendingBeaconAPIForcesSendingOnNavigation = {
    &blink::features::kPendingBeaconAPI, "send_on_navigation", true};

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
BASE_FEATURE(kPlzDedicatedWorker,
             "PlzDedicatedWorker",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When kPortals is enabled, allow portals to load content that is third-party
// (cross-origin) to the hosting page. Otherwise has no effect.
//
// https://crbug.com/1013389
BASE_FEATURE(kPortalsCrossOrigin,
             "PortalsCrossOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlocked,
    "PostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlocked",
    base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned,
    "PostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorage"
    "IsPartitioned",
    base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlocked,
    "PostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlocked",
    base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned,
    "PostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlockedIfStorage"
    "IsPartitioned",
    base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlocked,
    "PostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlocked",
    base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(
    kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned,
    "PostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorage"
    "IsPartitioned",
    base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kPrivacySandboxAdsAPIs,
             "PrivacySandboxAdsAPIs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Private Aggregation API. Note that this API also requires the
// `kPrivacySandboxAggregationService` to be enabled to successfully send
// reports.
BASE_FEATURE(kPrivateAggregationApi,
             "PrivateAggregationApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kProcessHtmlDataImmediately,
             "ProcessHtmlDataImmediately",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kProcessHtmlDataImmediatelyChildFrame{
    &kProcessHtmlDataImmediately, "child", false};

const base::FeatureParam<bool> kProcessHtmlDataImmediatelyFirstChunk{
    &kProcessHtmlDataImmediately, "first", false};

const base::FeatureParam<bool> kProcessHtmlDataImmediatelyMainFrame{
    &kProcessHtmlDataImmediately, "main", false};

const base::FeatureParam<bool> kProcessHtmlDataImmediatelySubsequentChunks{
    &kProcessHtmlDataImmediately, "rest", false};

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
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQueueBlockingGestureScrolls,
             "QueueBlockingGestureScrolls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuoteEmptySecChUaStringHeadersConsistently,
             "QuoteEmptySecChUaStringHeadersConsistently",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determines if the SDP attrbute extmap-allow-mixed should be offered by
// default or not. The default value can be overridden by passing
// {offerExtmapAllowMixed:false} as an argument to the RTCPeerConnection
// constructor.
BASE_FEATURE(kRTCOfferExtmapAllowMixed,
             "RTCOfferExtmapAllowMixed",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables waiting for codec support status notification from GPU factory in RTC
// codec factories.
BASE_FEATURE(kRTCGpuCodecSupportWaiter,
             "kRTCGpuCodecSupportWaiter",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kRTCGpuCodecSupportWaiterTimeoutParam{
    &kRTCGpuCodecSupportWaiter, "timeout_ms", 3000};

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
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRemoteResourceCache,
             "RemoteResourceCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveAuthroizationOnCrossOriginRedirect,
             "RemoveAutorizationOnCrossOriginRedirect",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRenderBlockingFonts,
             "RenderBlockingFonts",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxBlockingTimeMsForRenderBlockingFonts(
    &features::kRenderBlockingFonts,
    "max-blocking-time",
    1500);

const base::FeatureParam<int> kMaxFCPDelayMsForRenderBlockingFonts(
    &features::kRenderBlockingFonts,
    "max-fcp-delay",
    100);

BASE_FEATURE(kReportVisibleLineBounds,
             "ReportVisibleLineBounds",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingInputEvents,
             "ResamplingInputEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingScrollEvents,
             "ResamplingScrollEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRetriggerPreloadingOnBFCacheRestoration,
             "RetriggerPreloadingOnBFCacheRestoration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRunTextInputUpdatePostLifecycle,
             "RunTextInputUpdatePostLifecycle",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOriginTrialStateHostApplyFeatureDiff,
             "OriginTrialStateHostApplyFeatureDiff",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistFTPToRegisterProtocolHandler,
             "SafelistFTPToRegisterProtocolHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSSVTrailerEnforceExposureAssertion,
             "SSVTrailerEnforceExposureAssertion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSSVTrailerWriteExposureAssertion,
             "SSVTrailerWriteExposureAssertion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSSVTrailerWriteNewVersion,
             "SSVTrailerWriteNewVersion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A feature to enable using the smallest image specified within image srcset
// for users with Save Data enabled.
BASE_FEATURE(kSaveDataImgSrcset,
             "SaveDataImgSrcset",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Scopes the memory cache to a fetcher i.e. document/frame. Any resource cached
// in the blink cache will only be reused if the most recent fetcher that
// fetched it was the same as the current document.
BASE_FEATURE(kScopeMemoryCachePerContext,
             "ScopeMemoryCachePerContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPausePagesPerBrowsingContextGroup,
             "PausePagesPerBrowsingContextGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls script streaming.
BASE_FEATURE(kScriptStreaming,
             "ScriptStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kSerializeAccessibilityPostLifecycle,
             "SerializeAccessibilityPostLifecycle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
BASE_FEATURE(kServiceWorkerUpdateDelay,
             "ServiceWorkerUpdateDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit{
        &kSharedStorageAPI, "url_selection_operation_input_url_size_limit", 8};
const base::FeatureParam<int> kMaxSharedStorageStringLength = {
    &kSharedStorageAPI, "MaxSharedStorageStringLength", 1024};
const base::FeatureParam<int> kMaxSharedStorageEntriesPerOrigin = {
    &kSharedStorageAPI, "MaxSharedStorageEntriesPerOrigin", 10000};
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

BASE_FEATURE(kSharedStorageAPIM118,
             "SharedStorageAPIM118",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimulateClickOnAXFocus,
             "SimulateClickOnAXFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

// Allow streaming small (<30kB) scripts.
BASE_FEATURE(kSmallScriptStreaming,
             "SmallScriptStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpeculationRulesHeaderEnableThirdPartyOriginTrial,
             "SpeculationRulesHeaderEnableThirdPartyOriginTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpeculationRulesPrefetchFuture,
             "SpeculationRulesPrefetchFuture",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable service worker warming-up feature. (https://crbug.com/1431792)
BASE_FEATURE(kSpeculativeServiceWorkerWarmUp,
             "SpeculativeServiceWorkerWarmUp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, do not actually warm-up service workers.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpDryRun{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_dry_run", false};

// If true, warm-up immediately without waiting for load event.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpWaitForLoad{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_wait_for_load", false};

// kSpeculativeServiceWorkerWarmUp observes anchor events such as visibility,
// pointerover, and pointerdown. These events could be triggered very often. To
// reduce the frequency of processing, kSpeculativeServiceWorkerWarmUp uses a
// timer to batch URL candidates together for this amount of duration.
const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpBatchTimer{&kSpeculativeServiceWorkerWarmUp,
                                              "sw_warm_up_batch_timer",
                                              base::Milliseconds(300)};

// Similar to 'kSpeculativeServiceWorkerWarmUpBatchTimer`. But this is used for
// the first batch in the page.
const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpFirstBatchTimer{
        &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_first_batch_timer",
        base::Seconds(1)};

// The maximum URL candidate count (batch size) to notify URL candidates
// from renderer process to browser process. If URL candidate count
// exceeds batch size, the remaining URL candidate will be sent later.
const base::FeatureParam<int> kSpeculativeServiceWorkerWarmUpBatchSize{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_batch_size", 10};

// kSpeculativeServiceWorkerWarmUp warms up service workers up to this max
// count.
const base::FeatureParam<int> kSpeculativeServiceWorkerWarmUpMaxCount{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_max_count", 10};

// kSpeculativeServiceWorkerWarmUp enqueues navigation candidate URLs. This is
// the queue length of the candidate URLs.
const base::FeatureParam<int> kSpeculativeServiceWorkerWarmUpRequestQueueLength{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_request_queue_length", 1000};

// kSpeculativeServiceWorkerWarmUp accept requests of navigation candidate URLs.
// This is the request count limit per document.
const base::FeatureParam<int> kSpeculativeServiceWorkerWarmUpRequestLimit{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_request_limit", 1000};

// Duration to keep worker warmed-up.
const base::FeatureParam<base::TimeDelta>
    kSpeculativeServiceWorkerWarmUpDuration{&kSpeculativeServiceWorkerWarmUp,
                                            "sw_warm_up_duration",
                                            base::Minutes(10)};

// Enable IntersectionObserver to detect anchor's visibility.
const base::FeatureParam<bool>
    kSpeculativeServiceWorkerWarmUpIntersectionObserver{
        &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_intersection_observer",
        true};

// Duration from previous IntersectionObserver event to the next event.
const base::FeatureParam<int>
    kSpeculativeServiceWorkerWarmUpIntersectionObserverDelay{
        &kSpeculativeServiceWorkerWarmUp,
        "sw_warm_up_intersection_observer_delay", 100};

// Warms up service workers when the anchor becomes visible.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnVisible{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_visible", true};

// Warms up service workers when the anchor is inserted into DOM.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnInsertedIntoDom{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_inserted_into_dom", false};

// Warms up service workers when a pointerover event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerover{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerover", true};

// Warms up service workers when a pointerdown event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerdown{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerdown", true};

BASE_FEATURE(kSplitUserMediaQueues,
             "SplitUserMediaQueues",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStartMediaStreamCaptureIndicatorInBrowser,
             "StartMediaStreamCaptureIndicatorInBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
BASE_FEATURE(kStopInBackground,
             "stop-in-background",
// b/248036988 - Disable this for Chromecast on Android builds to prevent apps
// that play audio in the background from stopping.
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<int> kStorageAccessAPIImplicitGrantLimit{
    &kStorageAccessAPI, "storage-access-api-implicit-grant-limit", 0};
const base::FeatureParam<bool> kStorageAccessAPIAutoGrantInFPS{
    &kStorageAccessAPI, "storage_access_api_auto_grant_in_fps", true};
const base::FeatureParam<bool> kStorageAccessAPIAutoDenyOutsideFPS{
    &kStorageAccessAPI, "storage_access_api_auto_deny_outside_fps", true};
const base::FeatureParam<bool> kStorageAccessAPIRefreshGrantsOnUserInteraction{
    &kStorageAccessAPI, "storage_access_api_refresh_grants_on_user_interaction",
    true};
const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPITopLevelUserInteractionBound{
        &kStorageAccessAPI,
        "storage_access_api_top_level_user_interaction_bound", base::Days(30)};
const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPIImplicitPermissionLifetime{
        &kStorageAccessAPI, "storage_access_api_implicit_permission_lifetime",
        base::Hours(24)};
const base::FeatureParam<base::TimeDelta>
    kStorageAccessAPIExplicitPermissionLifetime{
        &kStorageAccessAPI, "storage_access_api_explicit_permission_lifetime",
        base::Days(30)};

BASE_FEATURE(kStylusPointerAdjustment,
             "StylusPointerAdjustment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStylusRichGestures,
             "StylusRichGestures",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(mahesh.ma): Enable for supported Android versions once feature is ready.
BASE_FEATURE(kStylusWritingToInput,
             "StylusWritingToInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSystemColorChooser,
             "SystemColorChooser",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTextCodecCJKEnabled,
             "TextCodecCJKEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGb18030_2022Enabled,
             "Gb18030_2022Enabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedBodyLoader,
             "ThreadedBodyLoader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedPreloadScanner,
             "ThreadedPreloadScanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes,
             "ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Throttles Javascript timer wake ups on foreground pages.
BASE_FEATURE(kThrottleForegroundTimers,
             "ThrottleForegroundTimers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable throttling of fetch() requests from service workers in the
// installing state.  The limit of 3 was chosen to match the limit
// in background main frames.  In addition, trials showed that this
// did not cause excessive timeouts and resulted in a net improvement
// in successful install rate on some platforms.
BASE_FEATURE(kThrottleInstallingServiceWorker,
             "ThrottleInstallingServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kInstallingServiceWorkerOutstandingThrottledLimit{
    &kThrottleInstallingServiceWorker, "limit", 3};

BASE_FEATURE(kTimedHTMLParserBudget,
             "TimedHTMLParserBudget",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes behavior of User-Agent Client Hints to send blank headers when the
// User-Agent string is overridden, instead of disabling the headers altogether.
BASE_FEATURE(kUACHOverrideBlank,
             "UACHOverrideBlank",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kURLSetPortCheckOverflow,
             "URLSetPortCheckOverflow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseBlinkSchedulerTaskRunnerWithCustomDeleter,
             "UseBlinkSchedulerTaskRunnerWithCustomDeleter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFileBackedBlobFactory,
             "EnableFileBackedBlobFactory",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kUseThreadPoolForMediaStreamVideoTaskRunner,
             "UseThreadPoolForMediaStreamVideoTaskRunner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables user level memory pressure signal generation on Android.
BASE_FEATURE(kUserLevelMemoryPressureSignal,
             "UserLevelMemoryPressureSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVSyncDecoding,
             "VSyncDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kVSyncDecodingHiddenOccludedTickDuration{
        &kVSyncDecoding, "occluded_tick_duration", base::Hertz(10)};

// Enable borderless mode for desktop PWAs. go/borderless-mode
BASE_FEATURE(kWebAppBorderless,
             "WebAppBorderless",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

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

// A server-side switch for the output device (sink) selection in Web Audio API.
// This enables the selection via the AudioContext constructor and also via
// AudioContext.setSinkId() method.
BASE_FEATURE(kWebAudioSinkSelection,
             "kWebAudioSinkSelection",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Controls whether the implementation of the performance.measureMemory
// web API uses PerformanceManager or not.
BASE_FEATURE(kWebMeasureMemoryViaPerformanceManager,
             "WebMeasureMemoryViaPerformanceManager",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread,
             "WebRtcCombinedNetworkAndWorkerThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
BASE_FEATURE(kWebRtcH264WithOpenH264FFmpeg,
             "WebRTC-H264WithOpenH264FFmpeg",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

// Exposes non-standard stats in the WebRTC getStats() API.
BASE_FEATURE(kWebRtcExposeNonStandardStats,
             "WebRtc-ExposeNonStandardStats",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kWebRtcInitializeEncoderOnFirstFrame,
             "WebRtcInitializeEncoderOnFirstFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcMetronome,
             "WebRtcMetronome",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables negotiation of experimental multiplex codec in SDP.
BASE_FEATURE(kWebRtcMultiplexCodec,
             "WebRTC-MultiplexCodec",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcSendPacketBatch,
             "WebRtcSendPacketBatch",
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

// TODO(https://crbug.com/1331187): Delete the function.
int GetMaxUnthrottledTimeoutNestingLevel() {
  return kMaxUnthrottledTimeoutNestingLevelParam.Get();
}

bool IsAllowPageWithIDBConnectionAndTransactionInBFCacheEnabled() {
  return base::FeatureList::IsEnabled(kAllowPageWithIDBConnectionInBFCache) &&
         base::FeatureList::IsEnabled(kAllowPageWithIDBTransactionInBFCache);
}

bool IsAllowURNsInIframeEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kAllowURNsInIframes);
}

bool IsFencedFramesEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kFencedFrames);
}

bool IsMaxUnthrottledTimeoutNestingLevelEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kMaxUnthrottledTimeoutNestingLevel);
}

bool IsNewBaseUrlInheritanceBehaviorEnabled() {
  // The kIsolateSandboxedIframes feature depends on the new base URL behavior,
  // so it enables the new behavior even if kNewBaseUrlInheritanceBehavior
  // isn't enabled.
  return (base::FeatureList::IsEnabled(kNewBaseUrlInheritanceBehavior) ||
          base::FeatureList::IsEnabled(kIsolateSandboxedIframes)) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableNewBaseUrlInheritanceBehavior);
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

bool IsThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesEnabled() {
  static bool throttling_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableThrottleNonVisibleCrossOriginIframes);

  return !throttling_disabled &&
         base::FeatureList::IsEnabled(
             features::
                 kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes);
}

bool ParkableStringsUseSnappy() {
  return base::FeatureList::IsEnabled(kUseSnappyForParkableStrings);
}

bool IsKeepAliveURLLoaderServiceEnabled() {
  return base::FeatureList::IsEnabled(kKeepAliveInBrowserMigration) ||
         base::FeatureList::IsEnabled(kFetchLaterAPI);
}

}  // namespace features
}  // namespace blink
