// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"

namespace features {

// Please keep features in alphabetical order.

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
BASE_FEATURE(kAllowContentInitiatedDataUrlNavigations,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows Blink to request fonts from the Android Downloadable Fonts API through
// the service implemented on the Java side.
BASE_FEATURE(kAndroidDownloadableFontsMatching,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds OOPIF support for android drag and drop.
BASE_FEATURE(kAndroidDragDropOopif, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Flag guard for Windows Arabic digit substitution workaround.
// crbug.com/440381284
BASE_FEATURE(kArabicDigitSubstitution, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Synchronously continuing with navigation can lead to trying to start another
// navigation synchronously while the first navigation is still being processed
// on the stack. This results in re-entrancy which is unsafe and triggers a
// CHECK.
//
// Embedders like Android WebView cannot guarantee that re-entrancy would never
// occur - in particular, there are existing Android WebView apps that do the
// problematic sync navigation. Hence Android WebView entirely disables this
// feature via
// ContentBrowserClient::SupportsAvoidUnnecessaryBeforeUnloadCheckSync().
//
// The eventual goal of this feature flag is to make it possible to continue
// navigation synchronously for some platforms
// (See: https://crbug.com/396998476).
//
// There are several modes that are described in the
// AvoidUnnecessaryBeforeUnloadCheckSyncMode enum in the header file.
//
// The eventual state is to utilize kWithoutSendBeforeUnload mode, as it offers
// the highest performance. However, kWithoutSendBeforeUnload mode causes a
// metrics skew due to the current inaccurate measurement timing of navigation
// start (refer to crbug.com/385170155). Therefore, kWithSendBeforeUnload mode
// is the current default. We would like to update to use
// kWithoutSendBeforeUnload mode once crbug.com/385170155 is resolved.
BASE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<AvoidUnnecessaryBeforeUnloadCheckSyncMode>::Option
    kAvoidUnnecessaryBeforeUnloadCheckSyncModeOption[] = {
        {AvoidUnnecessaryBeforeUnloadCheckSyncMode::kDumpWithoutCrashing,
         "DumpWithoutCrashing"},
        {AvoidUnnecessaryBeforeUnloadCheckSyncMode::kWithSendBeforeUnload,
         "WithSendBeforeUnload"},
        {AvoidUnnecessaryBeforeUnloadCheckSyncMode::kWithoutSendBeforeUnload,
         "WithoutSendBeforeUnload"},
};

BASE_FEATURE_ENUM_PARAM(
    AvoidUnnecessaryBeforeUnloadCheckSyncMode,
    kAvoidUnnecessaryBeforeUnloadCheckSyncMode,
    &kAvoidUnnecessaryBeforeUnloadCheckSync,
    "AvoidUnnecessaryBeforeUnloadCheckSyncMode",
    AvoidUnnecessaryBeforeUnloadCheckSyncMode::kWithSendBeforeUnload,
    &kAvoidUnnecessaryBeforeUnloadCheckSyncModeOption);

// Enables controlling the time to live for pages in the BackForwardCache.
// The time to live is defined by the param 'time_to_live_seconds'; if this
// param is not specified then this feature is ignored and the default is used.
BASE_FEATURE(kBackForwardCacheTimeToLiveControl,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the browser will schedule before unload tasks that continue
// navigation network responses in a kHigh priority queue.
// TODO(b/281094330): Run experiment on ChromeOS. Experiment was not run on
// ChromeOS due to try bot issue.
BASE_FEATURE(kBeforeUnloadBrowserResponseQueue,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// When this feature is enabled, requests to localhost initiated from non-secure
// contexts in the `unknown` IP address space are blocked.
//
// See also:
//  - kBlockInsecurePrivateNetworkRequests
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsFromUnknown,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Whether to hide paste popup on GestureScrollBegin or GestureScrollUpdate.
BASE_FEATURE(kHidePastePopupOnGSB, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Holdback the removal of debug reason strings in crrev.com/c/6312375
// to measure the impact.
BASE_FEATURE(kHoldbackDebugReasonStringRemoval,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kCancelCompositionWhenWindowLosesFocus,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
BASE_FEATURE(kCanvas2DImageChromium,
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// When enabled, CDP method Page.captureScreenshot will increment
// the LocalSurfaceId instead of waiting for ForceRedraw to complete.
// This should avoid a possible stall due to frames not being presented.
BASE_FEATURE(kCDPScreenshotNewSurface, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, code cache does not use a browsing_data filter for deletions.
BASE_FEATURE(kCodeCacheDeletionWithoutFilter, base::FEATURE_ENABLED_BY_DEFAULT);

// Turn on enforcements based on tracking the list of committed origins in
// ChildProcessSecurityPolicy::CanAccessMaybeOpaqueOrigin(). Note that this only
// controls whether or not the new security checks take effect; when this is
// off, the security check is still performed and compared to the legacy jail
// and citadel check to collect data about possible mismatches. Requires
// CommittedOriginTracking to also be turned on to take effect. See
// https://crbug.com/40148776.
//
// TODO(alexmos): Remove this feature flag once committed origin enforcements
// are fully launched. For now, the feature is kept as a kill switch.
BASE_FEATURE(kCommittedOriginEnforcements, base::FEATURE_ENABLED_BY_DEFAULT);

// Turn on the tracking of origins committed in each renderer process in
// ChildProcessSecurityPolicy. This is required for committed origin
// enforcements, which is gated behind kCommittedOriginEnforcements.
BASE_FEATURE(kCommittedOriginTracking, base::FEATURE_ENABLED_BY_DEFAULT);

// Turn on a bug fix for crbug.com/456537756, ensuring that the callback passed
// to RenderWidgetHostView::CopyFromSurface() is always called.
BASE_FEATURE(kCopyFromSurfaceAlwaysCallCallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables support for the `Critical-CH` response header.
// https://github.com/WICG/client-hints-infrastructure/blob/master/reliability.md#critical-ch
BASE_FEATURE(kCriticalClientHint, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable document policy negotiation mechanism.
BASE_FEATURE(kDocumentPolicyNegotiation, base::FEATURE_DISABLED_BY_DEFAULT);

// Requires documents embedded via <iframe>, etc, to explicitly opt-into the
// embedding: https://github.com/mikewest/embedding-requires-opt-in.
BASE_FEATURE(kEmbeddingRequiresOptIn, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables error reporting for JS errors inside DevTools frontend host
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnableDevToolsJsErrorReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// When enabled, enforces that same-document navigations must not change
// the committed origin, insecure request policy, or insecure navigations set.
// Any mismatch will result in a renderer kill via bad_message handling.
//
// This defends against renderer misbehavior and session history corruption,
// and helps catch violations of same-document invariants.
//
// This feature acts as a kill switch for https://crbug.com/40580002.
//
// Note: This feature remains disabled if
// blink::features::kTreatMhtmlInitialDocumentLoadsAsCrossDocument is disabled.
BASE_FEATURE(kEnforceSameDocumentOriginInvariants,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Content counterpart of ExperimentalContentSecurityPolicyFeatures in
// third_party/blink/renderer/platform/runtime_enabled_features.json5. Enables
// experimental Content Security Policy features ('navigate-to').
BASE_FEATURE(kExperimentalContentSecurityPolicyFeatures,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to support the newer syntax for the "Use Other Account"
// and account labels features.
BASE_FEATURE(kFedCmUseOtherAccountAndLabelsNewSyntax,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables NonString Tokens
BASE_FEATURE(kFedCmNonStringToken, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether FedCM preserves ports in well-known URLs during testing.
// When enabled, well-known URLs retain the original port from the provider URL
// instead of stripping it via eTLD+1 extraction. This is primarily used in
// test environments where IdPs run on non-standard ports (e.g., localhost:8080)
// and the well-known endpoint needs to be fetched from the same port.
// Production FedCM strips ports for security reasons to ensure well-known
// files are served from the canonical domain.
BASE_FEATURE(kFedCmPreservePortsForTesting, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables installed web app matching for getInstalledRelatedApps API.
BASE_FEATURE(kFilterInstalledAppsWebAppMatching,
             base::FEATURE_ENABLED_BY_DEFAULT);
#if BUILDFLAG(IS_WIN)
// Enables installed windows app matching for getInstalledRelatedApps API.
// Note: This is enabled by default as a kill switch, since the functionality
// was already implemented but without a related feature flag.
BASE_FEATURE(kFilterInstalledAppsWinMatching, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// If enabled, limits the number of FLEDGE auctions that can be run between page
// load and unload -- any attempt to run more than this number of auctions will
// fail (return null to JavaScript).
BASE_FEATURE(kFledgeLimitNumAuctions,
             "LimitNumFledgeAuctions",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The number of allowed auctions for each page load (load to unload).
const base::FeatureParam<int> kFledgeLimitNumAuctionsParam{
    &kFledgeLimitNumAuctions, "max_auctions_per_page", 8};

// Enables a delay for the post-auction interest group update to avoid
// immediately invalidating cached values.
BASE_FEATURE(kFledgeDelayPostAuctionInterestGroupUpdate,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables multi-threaded seller worklet.
BASE_FEATURE(kFledgeSellerWorkletThreadPool, base::FEATURE_ENABLED_BY_DEFAULT);

// The number of seller worklet threads.
const base::FeatureParam<int> kFledgeSellerWorkletThreadPoolSize{
    &kFledgeSellerWorkletThreadPool, "seller_worklet_thread_pool_size", 1};

// Enables multi-threaded bidder worklet.
BASE_FEATURE(kFledgeBidderWorkletThreadPool, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Makes FLEDGE worklets on Android not use the main thread for their mojo.
BASE_FEATURE(kFledgeAndroidWorkletOffMainThread,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// The scaling factor for calculating the number of bidder worklet threads based
// on the number of Interest Groups.
// Formula: #threads = 1 + scaling_factor * log10(#IGs)
const base::FeatureParam<double>
    kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor{
        &kFledgeBidderWorkletThreadPool,
        "bidder_worklet_thread_pool_size_logarithmic_scaling_factor", 2};

// This feature controls whether the renderer should use FontDataManager to
// fetch fonts from the Browser's FontDataService. It is currently scoped to
// Windows and Linux (via separate features and experiments). See
// crbug.com/335680565.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kFontDataServiceAllWebContents, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<FontDataServiceTypefaceType>::Option
    font_data_service_typeface[] = {
        {FontDataServiceTypefaceType::kDwrite, "DWrite"},
        {FontDataServiceTypefaceType::kFreetype, "Freetype"},
        {FontDataServiceTypefaceType::kFontations, "Fontations"}};
BASE_FEATURE_ENUM_PARAM(FontDataServiceTypefaceType,
                        kFontDataServiceTypefaceType,
                        &kFontDataServiceAllWebContents,
                        "typeface",
                        FontDataServiceTypefaceType::kDwrite,
                        &font_data_service_typeface);
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kFontDataServiceLinux, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<FontDataServiceTypefaceType>::Option
    font_data_service_typeface[] = {
        {FontDataServiceTypefaceType::kFreetype, "Freetype"},
        {FontDataServiceTypefaceType::kFontations, "Fontations"}};
BASE_FEATURE_ENUM_PARAM(FontDataServiceTypefaceType,
                        kFontDataServiceTypefaceType,
                        &kFontDataServiceLinux,
                        "typeface",
                        FontDataServiceTypefaceType::kFontations,
                        &font_data_service_typeface);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
bool IsFontDataServiceEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kFontDataServiceAllWebContents);
#elif BUILDFLAG(IS_LINUX)
  return base::FeatureList::IsEnabled(features::kFontDataServiceLinux);
#else
  return false;
#endif
}
#endif

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
BASE_FEATURE(kFontSrcLocalMatching, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use the Frame Routing Cache to avoid synchronous IPCs from the
// renderer side for iframe creation.
BASE_FEATURE(kFrameRoutingCache, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kFrameRoutingCacheResponseSize{
    &kFrameRoutingCache, "responseSize", 4};

// Group network isolation key(NIK) by storage interest group joining origin to
// improve privacy and performance -- IGs of the same joining origin can reuse
// sockets, so we don't need to renegotiate those connections.
BASE_FEATURE(kGroupNIKByJoiningOrigin, base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to experiment with removing the soft process limit. See
// https://crbug.com/369342694.
BASE_FEATURE(kRemoveRendererProcessLimit, base::FEATURE_ENABLED_BY_DEFAULT);

// Purge PartitionAlloc's Scheduler-Loop quarantine when the UI thread is done
// executing a task. This allow purging memory without scanning the stack. See:
// https://crbug.com/329027914
BASE_FEATURE(
    kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread,
    base::FEATURE_DISABLED_BY_DEFAULT);

// Killswitch for prefetch devtools UA override (crbug.com/422193319).
BASE_FEATURE(kPrefetchDevtoolsUserAgentOverride,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When true, duplicate navigations are ignored only if they are initiated
// with a user gesture.
BASE_FEATURE(kIgnoreDuplicateNavsOnlyWithUserGesture,
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature flag for the memory-backed code cache.
BASE_FEATURE(kInMemoryCodeCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether initial WebUI navigations should synchronously go from navigation
// start to commit, by doing e.g. in-renderer body loading.
BASE_FEATURE(kInitialWebUISyncNavStartToCommit,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ability to use the updateIfOlderThanMs field in the trusted
// bidding response to trigger a post-auction update if the group has been
// updated more recently than updateIfOlderThanMs milliseconds, bypassing the
// typical 24 hour wait.
BASE_FEATURE(kInterestGroupUpdateIfOlderThan, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable IOSurface based screen capturer.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kIOSurfaceCapturer, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, child process will not terminate itself when IPC is reset.
BASE_FEATURE(kKeepChildProcessAfterIPCReset, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks for all types of web workers.
//
// The exact checks run are the same as for other document subresources, and
// depends on the state of the main Local Network Access feature flags
//`kLocalNetworkAccessChecks`.
BASE_FEATURE(kLocalNetworkAccessForWorkers, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Local Network Access checks in warning mode for all types of web
// workers.
//
// Does nothing if `kLocalNetworkAccessForWorkers` is disabled.
//
// If both this and `kLocalNetworkAccessChecksForWorkers` are enabled, then LNA
// requests for workers do not require a permission, but simply display a
// warning in DevTools.
BASE_FEATURE(kLocalNetworkAccessForWorkersWarningOnly,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks for main frame navigations.
//
// The exact checks run are the same as for other document subresources, and
// depends on the state of the main Local Network Access feature flags
//`kLocalNetworkAccessChecks`.
BASE_FEATURE(kLocalNetworkAccessForNavigations,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks in warning mode for main frame
// navigations.
//
// Does nothing if `kLocalNetworkAccessForNavigations` is disabled.
//
// If both this and `kLocalNetworkAccessChecksForNavigations` are enabled, then
// main frame navigations that qualify as LNA requests do not require a
// permission, but simply display a warning in DevTools.
BASE_FEATURE(kLocalNetworkAccessForNavigationsWarningOnly,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks for subframe navigations.
//
// The exact checks run are the same as for other document subresources, and
// depends on the state of the main Local Network Access feature flags
//`kLocalNetworkAccessChecks`.
BASE_FEATURE(kLocalNetworkAccessForSubframeNavigations,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Local Network Access checks in warning mode for subframe navigations.
//
// Does nothing if `kLocalNetworkAccessForSubframeNavigations` is disabled.
//
// If both this and `kLocalNetworkAccessChecksForSubframeNavigations` are
// enabled, then subframe navigations that qualify as LNA requests do not
// require a permission, but simply display a warning in DevTools.
BASE_FEATURE(kLocalNetworkAccessForSubframeNavigationsWarningOnly,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks for subframe navigations.
//
// The same checks are run as for other document subresources, and depends on
// the state of the main Local Network Access feature flags
//`kLocalNetworkAccessChecks`, however when enabled these navigations are
// blocked without triggering a permission prompt.
//
// See crbug.com/409303581 for more discussion of Fenced Frames and LNA.
BASE_FEATURE(kLocalNetworkAccessForFencedFrameNavigations,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Local Network Access checks in warning mode for fenced frame
// navigations.
//
// Does nothing if `kLocalNetworkAccessForFencedFrameNavigations`
// is disabled.
//
// If both this and `kLocalNetworkAccessChecksForFencedFrameNavigations` are
// enabled, then fenced frame navigations that qualify as LNA requests are not
// blocked, but simply display a warning in DevTools.
BASE_FEATURE(kLocalNetworkAccessForFencedFrameNavigationsWarningOnly,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows the ReusePrerenderingProcessForMainFrames feature
// and the ProcessPerSiteUpToMainFrameThreshold feature to reuse processes
// even when DevTools was ever attached.
// This allows developers to test the process sharing mode,
// since DevTools normally disables it for the field trial participants.
BASE_FEATURE(kMainFrameProcessReuseAllowDevToolsAttached,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows the ReusePrerenderingProcessForMainFrames feature
// and the ProcessPerSiteUpToMainFrameThreshold feature to reuse processes
// even for IP and localhost pages.
// These pages are common targets for devtools.
BASE_FEATURE(kMainFrameProcessReuseAllowIPAndLocalhost,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, media-device enumerations use a cache that is
// invalidated upon notifications sent by base::SystemMonitor. If disabled, the
// cache is considered invalid on every enumeration request.
BASE_FEATURE(kMediaDevicesSystemMonitorCache,
             "MediaDevicesSystemMonitorCaching",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Allow cross-context transfer of MediaStreamTracks.
BASE_FEATURE(kMediaStreamTrackTransfer, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled Mojo uses a dedicated background thread to listen for incoming
// IPCs. Otherwise it's configured to use Content's IO thread for that purpose.
BASE_FEATURE(kMojoDedicatedThread, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, additional spare RPHs will be warmed up when the browser is
// not busy.
BASE_FEATURE(kMultipleSpareRPHs, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMultipleSpareRPHsCount,
                   &kMultipleSpareRPHs,
                   "count",
                   1u);

// When enabled, NavigationThrottleRegistry will cache attribute query results
// for the next same query. See https://crbug.com/424460302.
BASE_FEATURE(kNavigationThrottleRegistryAttributeCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, NavigationThrottleRunner2 is used instead of the original
// NavigationThrottleRunner. See https://crbug.com/422003056.
BASE_FEATURE(kNavigationThrottleRunner2, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables Permissions Policy verification in the Browser process
// in content/. Additionally only for //chrome Permissions Policy verification
// is enabled in components/permissions/permission_context_base.cc
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPermissionsPolicyVerificationInContent,
             "kPermissionsPolicyVerificationInContent",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// If enabled, responses with an operative Cookie-Indices will not be used
// if the relevant cookie values have changed.
BASE_FEATURE(kPrefetchCookieIndices, base::FEATURE_DISABLED_BY_DEFAULT);

// Preloading holdback feature disables preloading (e.g., preconnect, prefetch,
// and prerender) on all predictors. This is useful in comparing the impact of
// blink::features::kPrerender2 experiment with and without them.

// This Feature allows configuring preloading features via a parameter string.
// See content/browser/preloading/preloading_config.cc to see how to use this
// feature.
BASE_FEATURE(kPreloadingConfig, base::FEATURE_ENABLED_BY_DEFAULT);

// A misunderstanding when fixing crbug.com/40076091 meant that non-speculative
// RFHs were being created with a provisional RenderFrame in the renderer. This
// is nominally harmless, but can crash prerenders if devtool's network
// overrides feature is enabled. Guarded by a feature since fixing this new bug
// might reintroduce the previous crashes.
BASE_FEATURE(kPrerenderMoreCorrectSpeculativeRFHCreation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature makes it so that having pending views increase the priority of a
// RenderProcessHost even when there is a priority override.
BASE_FEATURE(kPriorityOverridePendingViews, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables exposure of the core milestone 1 (M1) APIs in the renderer without an
// origin trial token: Attribution Reporting, FLEDGE, Topics.
BASE_FEATURE(kPrivacySandboxAdsAPIsM1Override,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When disabled("legacy behavior") it resets ongoing gestures when window loses
// focus. In split screen scenario this means we can't continue scroll on a
// chrome window, when we start interacting with another window.
BASE_FEATURE(kContinueGestureOnLosingFocus, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Make sendBeacon throw for a Blob with a non simple type.
BASE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, try to reuse an unlocked renderer process when COOP swap is
// happening on prerender initial navigation. Please see crbug.com/41492112 for
// more details.
BASE_FEATURE(kProcessReuseOnPrerenderCOOPSwap,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Causes the browser to progressively enable accessibility for WebContents as
// they are unhidden and, optionally, disable accessibility some time after they
// become hidden.
BASE_FEATURE(kProgressiveAccessibility, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

constexpr base::FeatureParam<ProgressiveAccessibilityMode>::Option
    kProgressiveAccessibilityModeOptions[] = {
        {ProgressiveAccessibilityMode::kOnlyEnable, "only_enable"},
        {ProgressiveAccessibilityMode::kDisableOnHide, "disable_on_hide"}};

}  // namespace

BASE_FEATURE_ENUM_PARAM(ProgressiveAccessibilityMode,
                        kProgressiveAccessibilityModeParam,
                        &kProgressiveAccessibility,
                        "progressive_accessibility_mode",
                        ProgressiveAccessibilityMode::kOnlyEnable,
                        &kProgressiveAccessibilityModeOptions);

// Causes hidden tabs with crashed subframes to be marked for reload, meaning
// that if a user later switches to that tab, the current page will be
// reloaded.  This will hide crashed subframes from the user at the cost of
// extra reloads.
BASE_FEATURE(kReloadHiddenTabsWithCrashedSubframes,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kRendererCancellationThrottleImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows a navigation to resume even if the renderer process for
// its speculative RFH is killed. This only works for navigations that have not
// yet received the response and picked the final RFH to commit in.
BASE_FEATURE(kResumeNavigationWithSpeculativeRFHProcessGone,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, try to reuse any same-site process that is hosting
// only prerendered frames for main-frame navigations.
BASE_FEATURE(kReusePrerenderingProcessForMainFrames,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// If enabled, then orientation lock won't claim to work on anything but phone
// form factors.  Tablets already do unpredictable things, such as letterboxing
// vs rotating and/or (successfully) ignoring the request entirely.  Setting
// this flag turns off those use-cases which nobody should be relying on right
// now anyway; they don't work.
BASE_FEATURE(kRestrictOrientationLockToPhones,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kServiceWorkerAvoidMainThreadForInitialization,
             base::FEATURE_ENABLED_BY_DEFAULT);

// The set of ServiceWorker to bypass while making navigation request.
// They are represented by a comma separated list of HEX encoded SHA256 hash of
// the ServiceWorker's scripts.
// e.g.
// 9685C8DE399237BDA6FF3AD0F281E9D522D46BB0ECFACE05E98D2B9AAE51D1EF,
// 20F0D78B280E40C0A17ABB568ACF4BDAFFB9649ADA75B0675F962B3F4FC78EA4
BASE_FEATURE(kServiceWorkerBypassFetchHandlerHashStrings,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings{
        &kServiceWorkerBypassFetchHandlerHashStrings,
        "script_checksum_to_bypass", ""};

// (crbug.com/41411856): When enabled, the srcdoc iframes are controlled by the
// same service worker that controls their parent.
BASE_FEATURE(kServiceWorkerSrcdocSupport, base::FEATURE_ENABLED_BY_DEFAULT);

// When this is enabled, it fixes the object lifetime issue when
// `race-network-and-fetch-handler` is used, the object should be deleted after
// the fetch event completion, regardless of the result of racing.
//
// crbug.com/340949948 for more details.
BASE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix2,
             base::FEATURE_DISABLED_BY_DEFAULT);

// (crbug.com/1371756): When enabled, the static routing API starts
// ServiceWorker when the routing result of a main resource request was network
// fallback.
BASE_FEATURE(kServiceWorkerStaticRouterStartServiceWorker,
             base::FEATURE_ENABLED_BY_DEFAULT);

// (crbug.com/41337436): Enabled feature will have the ServiceWorker Client.url
// property be the creation URL. This means it does not reflect changes to the
// URL made by history.pushState() or similar history APIs.
// When disabled the ServiceWorker Client.url property will be the document URL
// including changes to history.pushState().
BASE_FEATURE(kServiceWorkerClientUrlIsCreationUrl,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Handles blocking the file picker when a visible but inactive tab in a split
// triggers it. This serves as a kill switch for crbug.com/444653104.
BASE_FEATURE(kSideBySideFilePickerCancelling, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
BASE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to skip a redundant NotifyNavigationStateChanged call during
// RendererDidNavigate.
BASE_FEATURE(kSkipRedundantNavigationStateNotification,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, skips registration of RendererCancellationThrottle and instead
// keeps navigation cancellation behavior by reusing the requester
// NavigationClient.
BASE_FEATURE(kSkipRendererCancellationThrottle,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, ensure high-rank processes are on the LRU list while app is in
// background or the effective binding state is in conflict with low rank
// processes.
BASE_FEATURE(kStrictHighRankProcessLRU, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kTextInputClient, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kTextInputClientIPCTimeout{
    &kTextInputClient, "ipc_timeout", base::Milliseconds(1500)};
#endif

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS and Windows.
BASE_FEATURE(kTouchpadOverscrollHistoryNavigation,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable TrustedTypes .fromLiteral support.
BASE_FEATURE(kTrustedTypesFromLiteral, base::FEATURE_DISABLED_BY_DEFAULT);

// Optimize DirectManipulationHelper by updating its event handler when the
// window parent changes instead of tearing down and recreating the whole
// helper. This is a temporary flag to test the performance impact of the
// optimization.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kUpdateDirectManipulationHelperOnParentChange,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Validate the code signing identity of the network process before establishing
// a Mojo connection with it.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kValidateNetworkServiceProcessIdentity,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

// Pre-warm up the network process on browser startup.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWarmUpNetworkProcess, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable WebAssembly dynamic tiering (only tier up hot functions).
BASE_FEATURE(kWebAssemblyDynamicTiering, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables in-process resource loading for WebUI renderer processes.
BASE_FEATURE(kWebUIInProcessResourceLoading,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enables WebOTP calls in cross-origin iframes if allowed by Permissions
// Policy.
BASE_FEATURE(kWebOTPAssertionFeaturePolicy, base::FEATURE_DISABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/40942531.
BASE_FEATURE(kLimitCrossOriginNonActivatedPaintHolding,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

bool IsEnforceSameDocumentOriginInvariantsEnabled() {
  return base::FeatureList::IsEnabled(kEnforceSameDocumentOriginInvariants) &&
         base::FeatureList::IsEnabled(
             blink::features::kTreatMhtmlInitialDocumentLoadsAsCrossDocument);
}

}  // namespace features
