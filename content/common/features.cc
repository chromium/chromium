// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Please keep features in alphabetical order.

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
BASE_FEATURE(kAllowContentInitiatedDataUrlNavigations,
             "AllowContentInitiatedDataUrlNavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows Blink to request fonts from the Android Downloadable Fonts API through
// the service implemented on the Java side.
BASE_FEATURE(kAndroidDownloadableFontsMatching,
             "AndroidDownloadableFontsMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds OOPIF support for android drag and drop.
BASE_FEATURE(kAndroidDragDropOopif,
             "AndroidDragDropOopif",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync,
             "AvoidUnnecessaryBeforeUnloadCheckSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
    AvoidUnnecessaryBeforeUnloadCheckSyncMode::kDumpWithoutCrashing,
    &kAvoidUnnecessaryBeforeUnloadCheckSyncModeOption);

// Enables controlling the time to live for pages in the BackForwardCache.
// The time to live is defined by the param 'time_to_live_seconds'; if this
// param is not specified then this feature is ignored and the default is used.
BASE_FEATURE(kBackForwardCacheTimeToLiveControl,
             "BackForwardCacheTimeToLiveControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the browser will schedule before unload tasks that continue
// navigation network responses in a kHigh priority queue.
// TODO(b/281094330): Run experiment on ChromeOS. Experiment was not run on
// ChromeOS due to try bot issue.
BASE_FEATURE(kBeforeUnloadBrowserResponseQueue,
             "BeforeUnloadBrowserResponseQueue",
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
             "BlockInsecurePrivateNetworkRequestsFromUnknown",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Whether to hide paste popup on GestureScrollBegin or GestureScrollUpdate.
BASE_FEATURE(kHidePastePopupOnGSB,
             "HidePastePopupOnGSB",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Holdback the removal of debug reason strings in crrev.com/c/6312375
// to measure the impact.
BASE_FEATURE(kHoldbackDebugReasonStringRemoval,
             "HoldbackDebugReasonStringRemoval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
BASE_FEATURE(kCanvas2DImageChromium,
             "Canvas2DImageChromium",
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// When enabled, CDP method Page.captureScreenshot will increment
// the LocalSurfaceId instead of waiting for ForceRedraw to complete.
// This should avoid a possible stall due to frames not being presented.
BASE_FEATURE(kCDPScreenshotNewSurface,
             "CDPScreenshotNewSurface",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, code cache does not use a browsing_data filter for deletions.
BASE_FEATURE(kCodeCacheDeletionWithoutFilter,
             "CodeCacheDeletionWithoutFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Turn on enforcements based on tracking the list of committed origins in
// ChildProcessSecurityPolicy::CanAccessMaybeOpaqueOrigin(). Note that this only
// controls whether or not the new security checks take effect; when this is
// off, the security check is still performed and compared to the legacy jail
// and citadel check to collect data about possible mismatches. Requires
// CommittedOriginTracking to also be turned on to take effect. See
// https://crbug.com/40148776.
BASE_FEATURE(kCommittedOriginEnforcements,
             "CommittedOriginEnforcements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Turn on the tracking of origins committed in each renderer process in
// ChildProcessSecurityPolicy. This is required for committed origin
// enforcements, which is gated behind kCommittedOriginEnforcements.
// Temporarily disabled while investigating https://crbug.com/377793089.
BASE_FEATURE(kCommittedOriginTracking,
             "CommittedOriginTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for the `Critical-CH` response header.
// https://github.com/WICG/client-hints-infrastructure/blob/master/reliability.md#critical-ch
BASE_FEATURE(kCriticalClientHint,
             "CriticalClientHint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable document policy negotiation mechanism.
BASE_FEATURE(kDocumentPolicyNegotiation,
             "DocumentPolicyNegotiation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Requires documents embedded via <iframe>, etc, to explicitly opt-into the
// embedding: https://github.com/mikewest/embedding-requires-opt-in.
BASE_FEATURE(kEmbeddingRequiresOptIn,
             "EmbeddingRequiresOptIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables error reporting for JS errors inside DevTools frontend host
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnableDevToolsJsErrorReporting,
             "EnableDevToolsJsErrorReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Content counterpart of ExperimentalContentSecurityPolicyFeatures in
// third_party/blink/renderer/platform/runtime_enabled_features.json5. Enables
// experimental Content Security Policy features ('navigate-to').
BASE_FEATURE(kExperimentalContentSecurityPolicyFeatures,
             "ExperimentalContentSecurityPolicyFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to support the newer syntax for the "Use Other Account"
// and account labels features.
BASE_FEATURE(kFedCmUseOtherAccountAndLabelsNewSyntax,
             "FedCmUseOtherAccountAndLabelsNewSyntax",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending SameSite=Lax cookies in credentialed FedCM requests
// (accounts endpoint, ID assertion endpoint and disconnect endpoint).
BASE_FEATURE(kFedCmSameSiteLax,
             "FedCmSameSiteLax",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables installed web app matching for getInstalledRelatedApps API.
BASE_FEATURE(kFilterInstalledAppsWebAppMatching,
             "FilterInstalledAppsWebAppMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_WIN)
// Enables installed windows app matching for getInstalledRelatedApps API.
// Note: This is enabled by default as a kill switch, since the functionality
// was already implemented but without a related feature flag.
BASE_FEATURE(kFilterInstalledAppsWinMatching,
             "FilterInstalledAppsWinMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
             "FledgeDelayPostAuctionInterestGroupUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables multi-threaded seller worklet.
BASE_FEATURE(kFledgeSellerWorkletThreadPool,
             "FledgeSellerWorkletThreadPool",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The number of seller worklet threads.
const base::FeatureParam<int> kFledgeSellerWorkletThreadPoolSize{
    &kFledgeSellerWorkletThreadPool, "seller_worklet_thread_pool_size", 1};

// Enables multi-threaded bidder worklet.
BASE_FEATURE(kFledgeBidderWorkletThreadPool,
             "FledgeBidderWorkletThreadPool",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Makes FLEDGE worklets on Android not use the main thread for their mojo.
BASE_FEATURE(kFledgeAndroidWorkletOffMainThread,
             "FledgeAndroidWorkletOffMainThread",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// The scaling factor for calculating the number of bidder worklet threads based
// on the number of Interest Groups.
// Formula: #threads = 1 + scaling_factor * log10(#IGs)
const base::FeatureParam<double>
    kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor{
        &kFledgeBidderWorkletThreadPool,
        "bidder_worklet_thread_pool_size_logarithmic_scaling_factor", 2};

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
BASE_FEATURE(kFontSrcLocalMatching,
             "FontSrcLocalMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to use the Frame Routing Cache to avoid synchronous IPCs from the
// renderer side for iframe creation.
BASE_FEATURE(kFrameRoutingCache,
             "FrameRoutingCache",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kFrameRoutingCacheResponseSize{
    &kFrameRoutingCache, "responseSize", 4};

// Group network isolation key(NIK) by storage interest group joining origin to
// improve privacy and performance -- IGs of the same joining origin can reuse
// sockets, so we don't need to renegotiate those connections.
BASE_FEATURE(kGroupNIKByJoiningOrigin,
             "GroupNIKByJoiningOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to experiment with removing the soft process limit. See
// https://crbug.com/369342694.
BASE_FEATURE(kRemoveRendererProcessLimit,
             "RemoveRendererProcessLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A feature flag for the memory-backed code cache.
BASE_FEATURE(kInMemoryCodeCache,
             "InMemoryCodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ability to use the updateIfOlderThanMs field in the trusted
// bidding response to trigger a post-auction update if the group has been
// updated more recently than updateIfOlderThanMs milliseconds, bypassing the
// typical 24 hour wait.
BASE_FEATURE(kInterestGroupUpdateIfOlderThan,
             "InterestGroupUpdateIfOlderThan",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable IOSurface based screen capturer.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kIOSurfaceCapturer,
             "IOSurfaceCapturer",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// If enabled, set a soft limit on the number of renderer processes on
// Android, after which Chrome will reuse existing processes when possible.
// This diverges from current Clank behavior, where we do not set any upper
// bound and instead delegate that to the system. 42 is approximated from
// 8GBs ((8192 - 1024) / (16384 / 96)), and has nothing to do with Douglas
// Adams' book. 1GB is a carve-out for integrated GPU VRAM.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRendererProcessLimitOnAndroid,
             "RendererProcessLimitOnAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kRendererProcessLimitOnAndroidCount,
                   &kRendererProcessLimitOnAndroid,
                   "count",
                   42u);
#endif  // BUILDFLAG(IS_ANDROID)

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
BASE_FEATURE(kMediaStreamTrackTransfer,
             "MediaStreamTrackTransfer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled Mojo uses a dedicated background thread to listen for incoming
// IPCs. Otherwise it's configured to use Content's IO thread for that purpose.
BASE_FEATURE(kMojoDedicatedThread,
             "MojoDedicatedThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, additional spare RPHs will be warmed up when the browser is
// not busy.
BASE_FEATURE(kMultipleSpareRPHs,
             "MultipleSpareRPHs",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMultipleSpareRPHsCount,
                   &kMultipleSpareRPHs,
                   "count",
                   1u);

// This feature enables Permissions Policy verification in the Browser process
// in content/. Additionally only for //chrome Permissions Policy verification
// is enabled in components/permissions/permission_context_base.cc
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPermissionsPolicyVerificationInContent,
             "kPermissionsPolicyVerificationInContent",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

// Preloading holdback feature disables preloading (e.g., preconnect, prefetch,
// and prerender) on all predictors. This is useful in comparing the impact of
// blink::features::kPrerender2 experiment with and without them.

// This Feature allows configuring preloading features via a parameter string.
// See content/browser/preloading/preloading_config.cc to see how to use this
// feature.
BASE_FEATURE(kPreloadingConfig,
             "PreloadingConfig",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A misunderstanding when fixing crbug.com/40076091 meant that non-speculative
// RFHs were being created with a provisional RenderFrame in the renderer. This
// is nominally harmless, but can crash prerenders if devtool's network
// overrides feature is enabled. Guarded by a feature since fixing this new bug
// might reintroduce the previous crashes.
BASE_FEATURE(kPrerenderMoreCorrectSpeculativeRFHCreation,
             "PrerenderMoreCorrectSpeculativeRFHCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature makes it so that having pending views increase the priority of a
// RenderProcessHost even when there is a priority override.
BASE_FEATURE(kPriorityOverridePendingViews,
             "PriorityOverridePendingViews",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables exposure of the core milestone 1 (M1) APIs in the renderer without an
// origin trial token: Attribution Reporting, FLEDGE, Topics.
BASE_FEATURE(kPrivacySandboxAdsAPIsM1Override,
             "PrivacySandboxAdsAPIsM1Override",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When disabled("legacy behavior") it resets ongoing gestures when window loses
// focus. In split screen scenario this means we can't continue scroll on a
// chrome window, when we start interacting with another window.
BASE_FEATURE(kContinueGestureOnLosingFocus,
             "ContinueGestureOnLosingFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif


// Make sendBeacon throw for a Blob with a non simple type.
BASE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType,
             "SendBeaconThrowForBlobWithNonSimpleType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, try to reuse an unlocked renderer process when COOP swap is
// happening on prerender initial navigation. Please see crbug.com/41492112 for
// more details.
BASE_FEATURE(kProcessReuseOnPrerenderCOOPSwap,
             "ProcessReuseOnPrerenderCOOPSwap",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Causes the browser to progressively enable accessibility for WebContents as
// they are unhidden and, optionally, disable accessibility some time after they
// become hidden.
BASE_FEATURE(kProgressiveAccessibility,
             "ProgressiveAccessibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             "ReloadHiddenTabsWithCrashedSubframes",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// If enabled, then orientation lock won't claim to work on anything but phone
// form factors.  Tablets already do unpredictable things, such as letterboxing
// vs rotating and/or (successfully) ignoring the request entirely.  Setting
// this flag turns off those use-cases which nobody should be relying on right
// now anyway; they don't work.
BASE_FEATURE(kRestrictOrientationLockToPhones,
             "RestrictOrientationLockToPhones",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kServiceWorkerAvoidMainThreadForInitialization,
             "ServiceWorkerAvoidMainThreadForInitialization",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The set of ServiceWorker to bypass while making navigation request.
// They are represented by a comma separated list of HEX encoded SHA256 hash of
// the ServiceWorker's scripts.
// e.g.
// 9685C8DE399237BDA6FF3AD0F281E9D522D46BB0ECFACE05E98D2B9AAE51D1EF,
// 20F0D78B280E40C0A17ABB568ACF4BDAFFB9649ADA75B0675F962B3F4FC78EA4
BASE_FEATURE(kServiceWorkerBypassFetchHandlerHashStrings,
             "ServiceWorkerBypassFetchHandlerHashStrings",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings{
        &kServiceWorkerBypassFetchHandlerHashStrings,
        "script_checksum_to_bypass", ""};

// (crbug.com/41411856): When enabled, the srcdoc iframes are controlled by the
// same service worker that controls their parent.
BASE_FEATURE(kServiceWorkerSrcdocSupport,
             "ServiceWorkerSrcdocSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// (crbug.com/340949948): Killswitch for the fix to address the ServiceWorker
// main and subreosurce loader lifetime issue, which introduces fetch() failure
// in the sw fetch handler.
BASE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix,
             "kServiceWorkerStaticRouterRaceRequestFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// (crbug.com/1371756): When enabled, the static routing API starts
// ServiceWorker when the routing result of a main resource request was network
// fallback.
BASE_FEATURE(kServiceWorkerStaticRouterStartServiceWorker,
             "ServiceWorkerStaticRouterStartServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

// (crbug.com/41337436): Enabled feature will have the ServiceWorker Client.url
// property be the creation URL. This means it does not reflect changes to the
// URL made by history.pushState() or similar history APIs.
// When disabled the ServiceWorker Client.url property will be the document URL
// including changes to history.pushState().
BASE_FEATURE(kServiceWorkerClientUrlIsCreationUrl,
             "ServiceWorkerClientUrlIsCreationUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
BASE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame,
             "SkipEarlyCommitPendingForCrashedFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Skip granting access to the data path if it has already been set.
BASE_FEATURE(kSkipGrantAccessToDataPathIfAlreadySet,
             "SkipGrantAccessToDataPathIfAlreadySet",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kTextInputClient,
             "TextInputClient",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kTextInputClientIPCTimeout{
    &kTextInputClient, "ipc_timeout", base::Milliseconds(1500)};
#endif

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS and Windows.
BASE_FEATURE(kTouchpadOverscrollHistoryNavigation,
             "TouchpadOverscrollHistoryNavigation",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable TrustedTypes .fromLiteral support.
BASE_FEATURE(kTrustedTypesFromLiteral,
             "TrustedTypesFromLiteral",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Validate the code signing identity of the network process before establishing
// a Mojo connection with it.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kValidateNetworkServiceProcessIdentity,
             "ValidateNetworkServiceProcessIdentity",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

// Pre-warm up the network process on browser startup.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWarmUpNetworkProcess,
             "WarmUpNetworkProcess",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable WebAssembly dynamic tiering (only tier up hot functions).
BASE_FEATURE(kWebAssemblyDynamicTiering,
             "WebAssemblyDynamicTiering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables in-process resource loading for WebUI renderer processes.
BASE_FEATURE(kWebUIInProcessResourceLoading,
             "WebUIInProcessResourceLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables WebOTP calls in cross-origin iframes if allowed by Permissions
// Policy.
BASE_FEATURE(kWebOTPAssertionFeaturePolicy,
             "WebOTPAssertionFeaturePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/40942531.
BASE_FEATURE(kLimitCrossOriginNonActivatedPaintHolding,
             "LimitCrossOriginNonActivatedPaintHolding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for post OOP-C cleanup crbug.com/391648152
BASE_FEATURE(kDisallowRasterInterfaceWithoutSkiaBackend,
             "DisallowRasterInterfaceWithoutSkiaBackend",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

}  // namespace features
