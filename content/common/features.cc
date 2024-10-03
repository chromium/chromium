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

// The fix to crbug.com/1248529 will be behind this default-enabled flag, in
// case it breaks any applications in the wild.
BASE_FEATURE(kHistoryInterventionSameDocumentFix,
             "HistoryInterventionSameDocumentFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, keyboard user activation will be verified by the browser side.
BASE_FEATURE(kBrowserVerifiedUserActivationKeyboard,
             "BrowserVerifiedUserActivationKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, changes to the zoom level are temporary and are forgotten when
// the tab is closed. If disabled, changes to the zoom level persist, as though
// the user affected them through the browser's UX.
BASE_FEATURE(kCapturedSurfaceControlTemporaryZoom,
             "CapturedSurfaceControlTemporaryZoom",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
BASE_FEATURE(kCanvas2DImageChromium,
             "Canvas2DImageChromium",
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// When enabled, code cache does not use a browsing_data filter for deletions.
BASE_FEATURE(kCodeCacheDeletionWithoutFilter,
             "CodeCacheDeletionWithoutFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables support for the `Critical-CH` response header.
// https://github.com/WICG/client-hints-infrastructure/blob/master/reliability.md#critical-ch
BASE_FEATURE(kCriticalClientHint,
             "CriticalClientHint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable changing source dynamically for desktop capture.
BASE_FEATURE(kDesktopCaptureChangeSource,
             "DesktopCaptureChangeSource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables caching of media devices for the purpose of enumerating them.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kDeviceMonitorMac,
             "DeviceMonitorMac",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enable document policy negotiation mechanism.
BASE_FEATURE(kDocumentPolicyNegotiation,
             "DocumentPolicyNegotiation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Requires documents embedded via <iframe>, etc, to explicitly opt-into the
// embedding: https://github.com/mikewest/embedding-requires-opt-in.
BASE_FEATURE(kEmbeddingRequiresOptIn,
             "EmbeddingRequiresOptIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable back/forward cache for screen reader users. This flag should be
// removed once the https://crbug.com/1271450 is resolved.
BASE_FEATURE(kEnableBackForwardCacheForScreenReader,
             "EnableBackForwardCacheForScreenReader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable back/forward cache when a page which has subframe(s) with ongoing
// navigation(s) is navigated. Currently, this is only for navigations which
// don't need URLLoaders or haven't yet sent network requests. This flag should
// be removed once the https://crbug.com/1511153 is resolved.
BASE_FEATURE(kEnableBackForwardCacheForOngoingSubframeNavigation,
             "EnableBackForwardCacheForOngoingSubframeNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Allow specifying subsets of "name", "picture", "email" in the fields API.
// Requires FedCmAuthz to be enabled.
BASE_FEATURE(kFedCmFlexibleFields,
             "FedCmFlexibleFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables CORS checks on the ID assertion endpoint of the FedCM API.
BASE_FEATURE(kFedCmIdAssertionCORS,
             "FedCmIdAssertionCORS",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending SameSite=Lax cookies in credentialed FedCM requests
// (accounts endpoint, ID assertion endpoint and disconnect endpoint).
BASE_FEATURE(kFedCmSameSiteLax,
             "FedCmSameSiteLax",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending only SameSite=None cookies in credentialed FedCM requests
// (accounts endpoint and ID assertion endpoint). If kFedCmIdAssertionCORS
// is enabled, this is a no-op for the ID assertion endpoint.
BASE_FEATURE(kFedCmSameSiteNone,
             "FedCmSameSiteNone",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables caching when loading interest groups for a bidder in an auction.
BASE_FEATURE(kFledgeUseInterestGroupCache,
             "FledgeUseInterestGroupCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// The scaling factor for calculating the number of bidder worklet threads based
// on the number of Interest Groups.
// Formula: #threads = 1 + scaling_factor * log10(#IGs)
const base::FeatureParam<double>
    kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor{
        &kFledgeBidderWorkletThreadPool,
        "bidder_worklet_thread_pool_size_logarithmic_scaling_factor", 0};

// This is a kill switch for focusing the RenderWidgetHostViewAndroid on
// ActionDown on every touch sequence if not focused already, please see
// b/340824076. We are adding this to confirm the hypothesis that root view,
// RWHVA, is always focused.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFocusRenderWidgetHostViewAndroidOnActionDown,
             "FocusRenderWidgetHostViewAndroidOnActionDown",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
BASE_FEATURE(kFontSrcLocalMatching,
             "FontSrcLocalMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Controls whether building a database of unique font names is performed
// using the Fontations library. If off, FreeType is used instead.
// Used as a kill switch, expected to be removed after one stable cycle
// of using Fontations. See https://crbug.com/349952802
BASE_FEATURE(kFontIndexingFontations,
             "FontIndexingFontations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Feature controlling whether or not memory pressure signals will be forwarded
// to the GPU process.
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kForwardMemoryPressureEventsToGpuProcess,
             "ForwardMemoryPressureEventsToGpuProcess",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether to use the Frame Routing Cache to avoid synchronous IPCs from the
// renderer side for iframe creation.
BASE_FEATURE(kFrameRoutingCache,
             "FrameRoutingCache",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kFrameRoutingCacheResponseSize{
    &kFrameRoutingCache, "responseSize", 4};

// Adds "/prefetch:8" (which is the "other" category of process - i.e. not
// browser, gpu, crashpad, etc.) to the info collection GPU process' command
// line, in order to keep from polluting the GPU prefetch history.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kGpuInfoCollectionSeparatePrefetch,
             "GpuInfoCollectionSeparatePrefetch",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Group network isolation key(NIK) by storage interest group joining origin to
// improve privacy and performance -- IGs of the same joining origin can reuse
// sockets, so we don't need to renegotiate those connections.
BASE_FEATURE(kGroupNIKByJoiningOrigin,
             "GroupNIKByJoiningOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Supports proxying thread type changes of renderer processes to browser
// process and having browser process handle adjusting thread properties (nice
// value, c-group, latency sensitivity...) for children which have sandbox
// restrictions.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kHandleChildThreadTypeChangesInBrowser,
             "HandleChildThreadTypeChangesInBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A feature to experiment with removing the soft process limit. See
// https://crbug.com/369342694.
BASE_FEATURE(kRemoveRendererProcessLimit,
             "RemoveRendererProcessLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature flag for the memory-backed code cache.
BASE_FEATURE(kInMemoryCodeCache,
             "InMemoryCodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// During compositor frame eviction, collect not only the surfaces that are
// reachable from the main frame tree, but also recurse into inner
// frames. Otherwise only toplevel frames and OOPIF are handled, and other
// cases, e.g. PDF tiles are ignored. See https://crbug.com/1360351 for details.
BASE_FEATURE(kInnerFrameCompositorSurfaceEviction,
             "InnerFrameCompositorSurfaceEviction",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Feature that controls whether WebContentsOcclusionChecker should handle
// occlusion notifications.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kMacWebContentsOcclusion,
             "MacWebContentsOcclusion",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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

// Enables skipping of calls to hideSoftInputFromWindow when there is not a
// keyboard currently visible.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOptimizeImmHideCalls,
             "OptimizeImmHideCalls",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

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

// This feature makes it so that having pending views increase the priority of a
// RenderProcessHost even when there is a priority override.
BASE_FEATURE(kPriorityOverridePendingViews,
             "PriorityOverridePendingViews",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables reporting ResourceTiming entries for document, who initiated a
// cancelled navigation in one of their <iframe>.
BASE_FEATURE(kResourceTimingForCancelledNavigationInFrame,
             "ResourceTimingForCancelledNavigationInFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);


// Make sendBeacon throw for a Blob with a non simple type.
BASE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType,
             "SendBeaconThrowForBlobWithNonSimpleType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, try to reuse an unlocked renderer process when COOP swap is
// happening on prerender initial navigation. Please see crbug.com/1519131 for
// more details.
BASE_FEATURE(kProcessReuseOnPrerenderCOOPSwap,
             "ProcessReuseOnPrerenderCOOPSwap",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables process sharing for sites that do not require a dedicated process
// by using a default SiteInstance. Default SiteInstances will only be used
// on platforms that do not use full site isolation.
// Note: This feature is mutally exclusive with
// kProcessSharingWithStrictSiteInstances. Only one of these should be enabled
// at a time.
BASE_FEATURE(kProcessSharingWithDefaultSiteInstances,
             "ProcessSharingWithDefaultSiteInstances",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether cross-site frames should get their own SiteInstance even when
// strict site isolation is disabled. These SiteInstances will still be
// grouped into a shared default process based on BrowsingInstance.
BASE_FEATURE(kProcessSharingWithStrictSiteInstances,
             "ProcessSharingWithStrictSiteInstances",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// When enabled, allow reusing an initial RenderFrameHost with an unused process
// for a subsequent WebUI navigation.  WebUI navigations typically trigger a
// BrowsingInstance swap, but the swap is not necessary in that case: see
// https://crbug.com/1485586.  This is intended to be used as a kill switch.
BASE_FEATURE(kReuseInitialRenderFrameHostForWebUI,
             "ReuseInitialRenderFrameHostForWebUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Run StableVideoDecoderFactoryProcessService and StableVideoDecoderService on
// the IO thread in the video decoder process. If it is disabled, they run on
// the main thread in the process.
BASE_FEATURE(kRunStableVideoDecoderFactoryProcessServiceOnIOThread,
             "RunStableVideoDecoderFactoryProcessServiceOnIOThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables auto preloading for fetch requests before invoking the fetch handler
// in ServiceWorker. The fetch request inside the fetch handler is resolved with
// this preload response. If the fetch handler result is fallback, uses this
// preload request as a fallback network request.
//
// Unlike navigation preload, this preloading is applied to subresources. Also,
// it doesn't require a developer opt-in.
//
// crbug.com/1472634 for more details.
BASE_FEATURE(kServiceWorkerAutoPreload,
             "ServiceWorkerAutoPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kServiceWorkerAvoidMainThreadForInitialization,
             "ServiceWorkerAvoidMainThreadForInitialization",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// (crbug.com/1371756): When enabled, the static routing API starts
// ServiceWorker when the routing result of a main resource request was network
// fallback.
BASE_FEATURE(kServiceWorkerStaticRouterStartServiceWorker,
             "ServiceWorkerStaticRouterStartServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

// (crbug.com/340949948): Killswitch for the fix to address the ServiceWorker
// main and subreosurce loader lifetime issue, which introduces fetch() failure
// in the sw fetch handler.
BASE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix,
             "kServiceWorkerStaticRouterRaceRequestFix",
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

// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
BASE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame,
             "SkipEarlyCommitPendingForCrashedFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// (crbug/1377753): Speculatively start service worker before BeforeUnload runs.
BASE_FEATURE(kSpeculativeServiceWorkerStartup,
             "SpeculativeServiceWorkerStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kTextInputClient,
             "TextInputClient",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kTextInputClientIPCTimeout{
    &kTextInputClient, "ipc_timeout", base::Milliseconds(1500)};
#endif

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS, LaCrOS and Windows.
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

// Use GpuMemoryBuffer backed VideoFrames in media streams.
BASE_FEATURE(kWebRtcUseGpuMemoryBufferVideoFrames,
             "WebRTC-UseGpuMemoryBufferVideoFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables WebOTP calls in cross-origin iframes if allowed by Permissions
// Policy.
BASE_FEATURE(kWebOTPAssertionFeaturePolicy,
             "WebOTPAssertionFeaturePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/1504324.
BASE_FEATURE(kWindowOpenFileSelectFix,
             "WindowOpenFileSelectFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/346629231.
BASE_FEATURE(kScrollBubblingFix,
             "ScrollBubblingFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/40942531.
BASE_FEATURE(kLimitCrossOriginNonActivatedPaintHolding,
             "LimitCrossOriginNonActivatedPaintHolding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

}  // namespace features
