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

// The following two features, when enabled, result in the browser process only
// asking the renderer process to run beforeunload handlers if it knows such
// handlers are registered. The two slightly differ in what they do and how
// they behave:
// . `kAvoidUnnecessaryBeforeUnloadCheckPostTask` in this case content continues
//   to report a beforeunload handler is present (even though it isn't). When
//   asked to dispatch the beforeunload handler, a post task is used (rather
//   than going to the renderer).
// . `kAvoidUnnecessaryBeforeUnloadCheckSync` in this case content does not
//   report a beforeunload handler is present. A ramification of this is
//   navigations that would normally check beforeunload handlers before
//   continuing will not, and navigation will synchronously continue.
// Only one should be used (if both are set, the second takes precedence). The
// second is unsafe for Android WebView (and thus entirely disabled via
// ContentBrowserClient::SupportsAvoidUnnecessaryBeforeUnloadCheckSync()),
// because the embedder may trigger reentrancy, which cannot be avoided.
BASE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync,
             "AvoidUnnecessaryBeforeUnloadCheckSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables controlling the time to live for pages in the BackForwardCache.
// The time to live is defined by the param 'time_to_live_seconds'; if this
// param is not specified then this feature is ignored and the default is used.
BASE_FEATURE(kBackForwardCacheTimeToLiveControl,
             "BackForwardCacheTimeToLiveControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sets moderate binding to background renderers playing media, when enabled.
// Else the renderer will have strong binding.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBackgroundMediaRendererHasModerateBinding,
             "BackgroundMediaRendererHasModerateBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, the browser will schedule before unload tasks that continue
// navigation network responses in a kHigh priority queue.
BASE_FEATURE(kBeforeUnloadBrowserResponseQueue,
             "BeforeUnloadBrowserResponseQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, requests to localhost initiated from non-secure
// contexts in the `unknown` IP address space are blocked.
//
// See also:
//  - kBlockInsecurePrivateNetworkRequests
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsFromUnknown,
             "BlockInsecurePrivateNetworkRequestsFromUnknown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, keyboard user activation will be verified by the browser side.
BASE_FEATURE(kBrowserVerifiedUserActivationKeyboard,
             "BrowserVerifiedUserActivationKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kCompositeClipPathAnimation,
             "CompositeClipPathAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, code cache does not use a browsing_data filter for deletions.
BASE_FEATURE(kCodeCacheDeletionWithoutFilter,
             "CodeCacheDeletionWithoutFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, when creating new proxies for all nodes in a `FrameTree`, one
// IPC is sent to create all child frame proxies instead of sending one IPC per
// proxy.
BASE_FEATURE(kConsolidatedIPCForProxyCreation,
             "ConsolidatedIPCForProxyCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, event.movement is calculated in blink instead of in browser.
BASE_FEATURE(kConsolidatedMovementXY,
             "ConsolidatedMovementXY",
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

// Enables error reporting for JS errors inside DevTools frontend host
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnableDevToolsJsErrorReporting,
             "EnableDevToolsJsErrorReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// TODO(https://crbug.com/1442346): Feature flag to guard extra CHECKs put in
// place to ensure that the AllowBindings API on RenderFrameHost is not called
// for documents outside of WebUI ones.
BASE_FEATURE(kEnsureAllowBindingsIsAlwaysForWebUI,
             "EnsureAllowBindingsIsAlwaysForWebUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is enabled and device permission is not granted by the user,
// media-device enumeration will provide at most one device per type and the
// device IDs will not be available.
BASE_FEATURE(kEnumerateDevicesHideDeviceIDs,
             "EnumerateDevicesHideDeviceIDs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Content counterpart of ExperimentalContentSecurityPolicyFeatures in
// third_party/blink/renderer/platform/runtime_enabled_features.json5. Enables
// experimental Content Security Policy features ('navigate-to').
BASE_FEATURE(kExperimentalContentSecurityPolicyFeatures,
             "ExperimentalContentSecurityPolicyFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables metrics collection for signin status mismatches. Also enables
// parsing the signin status HTTP headers.
// kFedCmIdpSigninStatusEnabled takes precedence over this feature flag.
BASE_FEATURE(kFedCmIdpSigninStatusMetrics,
             "FedCmIdpSigninStatusMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, limits the number of FLEDGE auctions that can be run between page
// load and unload -- any attempt to run more than this number of auctions will
// fail (return null to JavaScript).
BASE_FEATURE(kFledgeLimitNumAuctions,
             "LimitNumFledgeAuctions",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The number of allowed auctions for each page load (load to unload).
const base::FeatureParam<int> kFledgeLimitNumAuctionsParam{
    &kFledgeLimitNumAuctions, "max_auctions_per_page", 8};

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
BASE_FEATURE(kFontSrcLocalMatching,
             "FontSrcLocalMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature controlling whether or not memory pressure signals will be forwarded
// to the GPU process.
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kForwardMemoryPressureEventsToGpuProcess,
             "ForwardMemoryPressureEventsToGpuProcess",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Adds "/prefetch:8" (which is the "other" category of process - i.e. not
// browser, gpu, crashpad, etc.) to the info collection GPU process' command
// line, in order to keep from polluting the GPU prefetch history.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kGpuInfoCollectionSeparatePrefetch,
             "GpuInfoCollectionSeparatePrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Supports proxying thread type changes of renderer processes to browser
// process and having browser process handle adjusting thread properties (nice
// value, c-group, latency sensitivity...) for renderers which have sandbox
// restrictions.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kHandleRendererThreadTypeChangesInBrowser,
             "HandleRendererThreadTypeChangesInBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Tells the RenderFrameHost to send beforeunload messages on a different
// local frame interface which will handle the messages at a higher priority.
BASE_FEATURE(kHighPriorityBeforeUnload,
             "HighPriorityBeforeUnload",
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

// Enable IOSurface based screen capturer.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kIOSurfaceCapturer,
             "IOSurfaceCapturer",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables the TC39 Array grouping proposal.
BASE_FEATURE(kJavaScriptArrayGrouping,
             "JavaScriptArrayGrouping",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLazyFrameLoading,
             "LazyFrameLoading",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a fix for a macOS IME Live Conversion issue. crbug.com/1328530 and
// crbug.com/1342551
BASE_FEATURE(kMacImeLiveConversionFix,
             "MacImeLiveConversionFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature that controls whether WebContentsOcclusionChecker should handle
// occlusion notifications.
#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kMacWebContentsOcclusion,
             "MacWebContentsOcclusion",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// Preload cookie database on NetworkContext creation.
BASE_FEATURE(kPreloadCookies,
             "PreloadCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Preloading holdback feature disables preloading (e.g., preconnect, prefetch,
// and prerender) on all predictors. This is useful in comparing the impact of
// blink::features::kPrerender2 experiment with and without them.

// This Feature allows configuring preloading features via a parameter string.
// See content/browser/preloading/preloading_config.cc to see how to use this
// feature.
BASE_FEATURE(kPreloadingConfig,
             "PreloadingConfig",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables exposure of the core milestone 1 (M1) APIs in the renderer without an
// origin trial token: Attribution Reporting, FLEDGE, Topics.
BASE_FEATURE(kPrivacySandboxAdsAPIsM1Override,
             "PrivacySandboxAdsAPIsM1Override",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Private Network Access checks in warning mode for iframe navigations.
//
// Does nothing if `kPrivateNetworkAccessForIframes` is disabled.
//
// If both this and `kPrivateNetworkAccessForIframes` are enabled, then PNA
// preflight requests for iframe navigations are not required to succeed. If
// one fails, a warning is simply displayed in DevTools.
BASE_FEATURE(kPrivateNetworkAccessForIframesWarningOnly,
             "PrivateNetworkAccessForIframesWarningOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables reporting ResourceTiming entries for document, who initiated a
// cancelled navigation in one of their <iframe>.
BASE_FEATURE(kResourceTimingForCancelledNavigationInFrame,
             "ResourceTimingForCancelledNavigationInFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the ProactivelySwapBrowsingInstance experiment. A browsing instance
// represents a set of frames that can script each other. Currently, Chrome does
// not always switch BrowsingInstance when navigating in between two unrelated
// pages. This experiment makes Chrome swap BrowsingInstances for cross-site
// HTTP(S) navigations when the BrowsingInstance doesn't contain any other
// windows.
BASE_FEATURE(kProactivelySwapBrowsingInstance,
             "ProactivelySwapBrowsingInstance",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, CanAccessDataForOrigin can only be called from the UI thread.
// This is related to Citadel desktop protections. See
// https://crbug.com/1286501.
BASE_FEATURE(kRestrictCanAccessDataForOriginToUIThread,
             "RestrictCanAccessDataForOriginToUIThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Make sendBeacon throw for a Blob with a non simple type.
BASE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType,
             "SendBeaconThrowForBlobWithNonSimpleType",
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

// (crbug.com/1371756): When enabled, the static routing API starts
// ServiceWorker when the routing result of a main resource request was network
// fallback.
BASE_FEATURE(kServiceWorkerStaticRouterStartServiceWorker,
             "ServiceWorkerStaticRouterStartServiceWorker",
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

// Signed Exchange Reporting for distributors
// https://www.chromestatus.com/feature/5687904902840320
BASE_FEATURE(kSignedExchangeReportingForDistributors,
             "SignedExchangeReportingForDistributors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, ensures that an unlocked process cannot access data for
// sites that require a dedicated process.
BASE_FEATURE(kSiteIsolationCitadelEnforcement,
             "kSiteIsolationCitadelEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
BASE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame,
             "SkipEarlyCommitPendingForCrashedFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// (crbug/1377753): Speculatively start service worker before BeforeUnload runs.
BASE_FEATURE(kSpeculativeServiceWorkerStartup,
             "SpeculativeServiceWorkerStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStopVideoCaptureOnScreenLock,
             "StopVideoCaptureOnScreenLock",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kTextInputClient,
             "TextInputClient",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kTextInputClientIPCTimeout{
    &kTextInputClient, "ipc_timeout", base::Milliseconds(1500)};
#endif

// Enables async touchpad pinch zoom events. We check the ACK of the first
// synthetic wheel event in a pinch sequence, then send the rest of the
// synthetic wheel events of the pinch sequence as non-blocking if the first
// event’s ACK is not canceled.
BASE_FEATURE(kTouchpadAsyncPinchEvents,
             "TouchpadAsyncPinchEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Controls whether the <video>.getVideoPlaybackQuality() API is enabled.
BASE_FEATURE(kVideoPlaybackQuality,
             "VideoPlaybackQuality",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
BASE_FEATURE(kWebGLImageChromium,
             "WebGLImageChromium",
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

// Flag guard for fix for crbug.com/1414936.
BASE_FEATURE(kWindowOpenFileSelectFix,
             "WindowOpenFileSelectFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

}  // namespace features
