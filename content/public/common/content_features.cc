// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/buildflags.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace features {

// All features in alphabetical order.

// Enables the allowActivationDelegation attribute on iframes.
// https://www.chromestatus.com/features/6025124331388928
//
// TODO(mustaq): Deprecated, see kUserActivationPostMessageTransfer.
const base::Feature kAllowActivationDelegationAttr{
    "AllowActivationDelegationAttr", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables content-initiated, main frame navigations to data URLs.
// TODO(meacer): Remove when the deprecation is complete.
//               https://www.chromestatus.com/feature/5669602927312896
const base::Feature kAllowContentInitiatedDataUrlNavigations{
    "AllowContentInitiatedDataUrlNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Blink to request fonts from the Android Downloadable Fonts API through
// the service implemented on the Java side.
const base::Feature kAndroidDownloadableFontsMatching{
    "AndroidDownloadableFontsMatching", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
const base::Feature kAudioProcessHighPriorityWin{
    "AudioProcessHighPriorityWin", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Launches the audio service on the browser startup.
const base::Feature kAudioServiceLaunchOnStartup{
    "AudioServiceLaunchOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};

// Runs the audio service in a separate process.
const base::Feature kAudioServiceOutOfProcess {
  "AudioServiceOutOfProcess",
// TODO(crbug.com/1052397): Remove !IS_CHROMEOS_LACROS once lacros starts being
// built with OS_CHROMEOS instead of OS_LINUX.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables the audio-service sandbox. This feature has an effect only when the
// kAudioServiceOutOfProcess feature is enabled.
const base::Feature kAudioServiceSandbox {
  "AudioServiceSandbox",
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_FUCHSIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Kill switch for Background Fetch.
const base::Feature kBackgroundFetch{"BackgroundFetch",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable using the BackForwardCache.
// BackForwardCache is enabled only on Android for the moment, as some
// desktop-specific features (including extensions) are not compatible with
// bfcache yet.
//
// Tracking bug for enabling bfcache on desktop: https://crbug.com/1171298.
#if defined(OS_ANDROID)
const base::Feature kBackForwardCache{"BackForwardCache",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kBackForwardCache{"BackForwardCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable same-site back-forward cache for trybots. This is here because of
// https://crbug.com/1211818 and should only used for trybots. For normal use
// cases, please set BackForwardCache's enable_same_site param to true.
const base::Feature kBackForwardCacheSameSiteForBots{
    "BackForwardCacheSameSiteForBots", base::FEATURE_DISABLED_BY_DEFAULT};

// BackForwardCache is disabled on low memory devices. The threshold is defined
// via a field trial param: "memory_threshold_for_back_forward_cache_in_mb"
// It is compared against base::SysInfo::AmountOfPhysicalMemoryMB().

// "BackForwardCacheMemoryControls" is checked before "BackForwardCache". It
// means the low memory devices will activate neither the control group nor the
// experimental group of the BackForwardCache field trial.

// BackForwardCacheMemoryControls is enabled only on Android to disable
// BackForwardCache for lower memory devices due to memory limiations.
#if defined(OS_ANDROID)
const base::Feature kBackForwardCacheMemoryControls{
    "BackForwardCacheMemoryControls", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kBackForwardCacheMemoryControls{
    "BackForwardCacheMemoryControls", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Block subresource requests whose URLs contain embedded credentials (e.g.
// `https://user:pass@example.com/resource`).
const base::Feature kBlockCredentialedSubresources{
    "BlockCredentialedSubresources", base::FEATURE_ENABLED_BY_DEFAULT};

// When kBlockInsecurePrivateNetworkRequests is enabled, requests initiated
// from a less-private network may only target a more-private network if the
// initiating context is secure.
//
// See also:
//  - https://wicg.github.io/cors-rfc1918/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequestsForNavigations
const base::Feature kBlockInsecurePrivateNetworkRequests{
    "BlockInsecurePrivateNetworkRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables use of the PrivateNetworkAccessNonSecureContextsAllowed deprecation
// trial. This is a necessary yet insufficient condition: documents that wish to
// make use of the trial must additionally serve a valid origin trial token.
const base::Feature kBlockInsecurePrivateNetworkRequestsDeprecationTrial{
    "BlockInsecurePrivateNetworkRequestsDeprecationTrial",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When both kBlockInsecurePrivateNetworkRequestsForNavigations and
// kBlockInsecurePrivateNetworkRequests are enabled, navigations initiated
// by documents in a less-private network may only target a more-private network
// if the initiating context is secure.
const base::Feature kBlockInsecurePrivateNetworkRequestsForNavigations{
    "BlockInsecurePrivateNetworkRequestsForNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT,
};

// Use ThreadPriority::DISPLAY for browser UI and IO threads.
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kBrowserUseDisplayThreadPriority{
    "BrowserUseDisplayThreadPriority", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kBrowserUseDisplayThreadPriority{
    "BrowserUseDisplayThreadPriority", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// When enabled, keyboard user activation will be verified by the browser side.
const base::Feature kBrowserVerifiedUserActivationKeyboard{
    "BrowserVerifiedUserActivationKeyboard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, mouse user activation will be verified by the browser side.
const base::Feature kBrowserVerifiedUserActivationMouse{
    "BrowserVerifiedUserActivationMouse", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables code caching for inline scripts.
const base::Feature kCacheInlineScriptCode{"CacheInlineScriptCode",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kCanvas2DImageChromium {
  "Canvas2DImageChromium",
#if defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Clear the window.name property for the top-level cross-site navigations that
// swap BrowsingContextGroups(BrowsingInstances).
const base::Feature kClearCrossSiteCrossBrowsingContextGroupWindowName{
    "ClearCrossSiteCrossBrowsingContextGroupWindowName",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCapabilityDelegationPaymentRequest{
    "CapabilityDelegationPaymentRequest", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClickPointerEvent{"ClickPointerEvent",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCompositeBGColorAnimation{
    "CompositeBGColorAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, code cache does not use a browsing_data filter for deletions.
extern const base::Feature kCodeCacheDeletionWithoutFilter{
    "CodeCacheDeletionWithoutFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, event.movement is calculated in blink instead of in browser.
const base::Feature kConsolidatedMovementXY{"ConsolidatedMovementXY",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Blink cooperative scheduling.
const base::Feature kCooperativeScheduling{"CooperativeScheduling",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables crash reporting via Reporting API.
// https://www.w3.org/TR/reporting/#crash-report
const base::Feature kCrashReporting{"CrashReporting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for the `Critical-CH` response header.
// https://github.com/WICG/client-hints-infrastructure/blob/master/reliability.md#critical-ch
const base::Feature kCriticalClientHint{"CriticalClientHint",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Puts save-data header in the holdback mode. This disables sending of
// save-data header to origins, and to the renderer processes within Chrome.
const base::Feature kDataSaverHoldback{"DataSaverHoldback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable debugging the issue crbug.com/1201355
const base::Feature kDebugHistoryInterventionNoUserActivation{
    "DebugHistoryInterventionNoUserActivation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable changing source dynamically for desktop capture.
const base::Feature kDesktopCaptureChangeSource{
    "DesktopCaptureChangeSource", base::FEATURE_ENABLED_BY_DEFAULT};

// Adds a tab strip to PWA windows.
// TODO(crbug.com/897314): Enable this feature.
const base::Feature kDesktopPWAsTabStrip{"DesktopPWAsTabStrip",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable document policy for configuring and restricting feature behavior.
const base::Feature kDocumentPolicy{"DocumentPolicy",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable document policy negotiation mechanism.
const base::Feature kDocumentPolicyNegotiation{
    "DocumentPolicyNegotiation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Early Hints subresource preloads for navigation.
const base::Feature kEarlyHintsPreloadForNavigation{
    "EarlyHintsPreloadForNavigation", base::FEATURE_DISABLED_BY_DEFAULT};

// Requires documents embedded via <iframe>, etc, to explicitly opt-into the
// embedding: https://github.com/mikewest/embedding-requires-opt-in.
const base::Feature kEmbeddingRequiresOptIn{"EmbeddingRequiresOptIn",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables canvas 2d methods BeginLayer and EndLayer.
const base::Feature kEnableCanvas2DLayers{"EnableCanvas2DLayers",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new canvas 2d api features. Enabled either with either
// enable-experimental-canvas-features or new-canvas-2d-api runtime flags
const base::Feature kEnableNewCanvas2DAPI{"EnableNewCanvas2DAPI",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If this feature is enabled and device permission is not granted by the user,
// media-device enumeration will provide at most one device per type and the
// device IDs will not be available.
// TODO(crbug.com/1019176): remove the feature in M89.
const base::Feature kEnumerateDevicesHideDeviceIDs {
  "EnumerateDevicesHideDeviceIDs",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// When a screen reader is detected, allow users the option of letting
// Google provide descriptions for unlabeled images.
const base::Feature kExperimentalAccessibilityLabels{
    "ExperimentalAccessibilityLabels", base::FEATURE_ENABLED_BY_DEFAULT};

// Content counterpart of ExperimentalContentSecurityPolicyFeatures in
// third_party/blink/renderer/platform/runtime_enabled_features.json5. Enables
// experimental Content Security Policy features ('navigate-to' and
// 'prefetch-src').
const base::Feature kExperimentalContentSecurityPolicyFeatures{
    "ExperimentalContentSecurityPolicyFeatures",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle tasks in Blink background timer queues based on CPU budgets
// for the background tab. Bug: https://crbug.com/639852.
const base::Feature kExpensiveBackgroundTimerThrottling{
    "ExpensiveBackgroundTimerThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Extra CORS safelisted headers. See https://crbug.com/999054.
const base::Feature kExtraSafelistedRequestHeadersForOutOfBlinkCors{
    "ExtraSafelistedRequestHeadersForOutOfBlinkCors",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Client Hints are guarded by Permissions Policy.
const base::Feature kFeaturePolicyForClientHints{
    "FeaturePolicyForClientHints", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the AccessHandle surface for File System Access API's Origin Private
// File System. Tracking bug: https://crbug.com/1218431.
const base::Feature kFileSystemAccessAccessHandle{
    "FileSystemAccessAccessHandle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
const base::Feature kFontSrcLocalMatching{"FontSrcLocalMatching",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Feature controlling whether or not memory pressure signals will be forwarded
// to the GPU process.
const base::Feature kForwardMemoryPressureEventsToGpuProcess {
  "ForwardMemoryPressureEventsToGpuProcess",
#if defined(OS_FUCHSIA) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif

// Enables scrollers inside Blink to store scroll offsets in fractional
// floating-point numbers rather than truncating to integers.
const base::Feature kFractionalScrollOffsets{"FractionalScrollOffsets",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Puts network quality estimate related Web APIs in the holdback mode. When the
// holdback is enabled the related Web APIs return network quality estimate
// set by the experiment (regardless of the actual quality).
const base::Feature kNetworkQualityEstimatorWebHoldback{
    "NetworkQualityEstimatorWebHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

// Determines if an extra brand version pair containing possibly escaped double
// quotes and escaped backslashed should be added to the Sec-CH-UA header
// (activated by kUserAgentClientHint)
const base::Feature kGreaseUACH{"GreaseUACH", base::FEATURE_ENABLED_BY_DEFAULT};

// Prevents sandboxed iframes from using the history API to navigate frames
// outside their subttree, if they are restricted from doing top-level
// navigations.
const base::Feature kHistoryPreventSandboxedNavigation{
    "HistoryPreventSandboxedNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// This is intended as a kill switch for the Idle Detection feature. To enable
// this feature, the experimental web platform features flag should be set,
// or the site should obtain an Origin Trial token.
const base::Feature kIdleDetection{"IdleDetection",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Kill switch for the GetInstalledRelatedApps API.
const base::Feature kInstalledApp{"InstalledApp",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Allow Windows specific implementation for the GetInstalledRelatedApps API.
const base::Feature kInstalledAppProvider{"InstalledAppProvider",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Show warning about clearing data from installed apps in the clear browsing
// data flow. The warning will be shown in a second dialog.
const base::Feature kInstalledAppsInCbd{"InstalledAppsInCbd",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Alternative to switches::kIsolateOrigins, for turning on origin isolation.
// List of origins to isolate has to be specified via
// kIsolateOriginsFieldTrialParamName.
const base::Feature kIsolateOrigins{"IsolateOrigins",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const char kIsolateOriginsFieldTrialParamName[] = "OriginsList";

// Experimental handling of accept-language via client hints.
const base::Feature kLangClientHintHeader{"LangClientHintHeader",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLazyFrameLoading{"LazyFrameLoading",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kLazyFrameVisibleLoadTimeMetrics {
  "LazyFrameVisibleLoadTimeMetrics",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
const base::Feature kLazyImageLoading{"LazyImageLoading",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kLazyImageVisibleLoadTimeMetrics {
  "LazyImageVisibleLoadTimeMetrics",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable lazy initialization of the media controls.
const base::Feature kLazyInitializeMediaControls{
    "LazyInitializeMediaControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Configures whether Blink on Windows 8.0 and below should use out of process
// API font fallback calls to retrieve a fallback font family name as opposed to
// using a hard-coded font lookup table.
const base::Feature kLegacyWindowsDWriteFontFallback{
    "LegacyWindowsDWriteFontFallback", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLogJsConsoleMessages {
  "LogJsConsoleMessages",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// The MBI mode controls whether or not communication over the
// AgentSchedulingGroup is ordered with respect to the render-process-global
// legacy IPC channel, as well as the granularity of AgentSchedulingGroup
// creation. This will break ordering guarantees between different agent
// scheduling groups (ordering withing a group is still preserved).
// DO NOT USE! The feature is not yet fully implemented. See crbug.com/1111231.
const base::Feature kMBIMode {
  "MBIMode",
#if BUILDFLAG(MBI_MODE_PER_RENDER_PROCESS_HOST) || \
    BUILDFLAG(MBI_MODE_PER_SITE_INSTANCE)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
const base::FeatureParam<MBIMode>::Option mbi_mode_types[] = {
    {MBIMode::kLegacy, "legacy"},
    {MBIMode::kEnabledPerRenderProcessHost, "per_render_process_host"},
    {MBIMode::kEnabledPerSiteInstance, "per_site_instance"}};
const base::FeatureParam<MBIMode> kMBIModeParam {
  &kMBIMode, "mode",
#if BUILDFLAG(MBI_MODE_PER_RENDER_PROCESS_HOST)
      MBIMode::kEnabledPerRenderProcessHost,
#elif BUILDFLAG(MBI_MODE_PER_SITE_INSTANCE)
      MBIMode::kEnabledPerSiteInstance,
#else
      MBIMode::kLegacy,
#endif
      &mbi_mode_types
};

// If this feature is enabled, media-device enumerations use a cache that is
// invalidated upon notifications sent by base::SystemMonitor. If disabled, the
// cache is considered invalid on every enumeration request.
const base::Feature kMediaDevicesSystemMonitorCache {
  "MediaDevicesSystemMonitorCaching",
#if defined(OS_MAC) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If enabled Mojo uses a dedicated background thread to listen for incoming
// IPCs. Otherwise it's configured to use Content's IO thread for that purpose.
const base::Feature kMojoDedicatedThread{"MojoDedicatedThread",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables/disables the video capture service.
const base::Feature kMojoVideoCapture{"MojoVideoCapture",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// A secondary switch used in combination with kMojoVideoCapture.
// This is intended as a kill switch to allow disabling the service on
// particular groups of devices even if they forcibly enable kMojoVideoCapture
// via a command-line argument.
const base::Feature kMojoVideoCaptureSecondary{
    "MojoVideoCaptureSecondary", base::FEATURE_ENABLED_BY_DEFAULT};

// When enable, iframe does not implicit capture mouse event.
const base::Feature kMouseSubframeNoImplicitCapture{
    "MouseSubframeNoImplicitCapture", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables optimizations for renderer->browser mojo calls to avoid waiting on
// the UI thread during navigation.
const base::Feature kNavigationThreadingOptimizations{
    "NavigationThreadingOptimizations", base::FEATURE_DISABLED_BY_DEFAULT};

// If the network service is enabled, runs it in process.
const base::Feature kNetworkServiceInProcess {
  "NetworkServiceInProcess",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kNeverSlowMode{"NeverSlowMode",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for Web Notification content images.
const base::Feature kNotificationContentImage{"NotificationContentImage",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the notification trigger API.
const base::Feature kNotificationTriggers{"NotificationTriggers",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the Origin-Agent-Cluster header. Tracking bug
// https://crbug.com/1042415; flag removal bug (for when this is fully launched)
// https://crbug.com/1148057.
//
// The name is "OriginIsolationHeader" because that was the old name when the
// feature was under development.
const base::Feature kOriginIsolationHeader{"OriginIsolationHeader",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Origin Policy. See https://crbug.com/751996
const base::Feature kOriginPolicy{"OriginPolicy",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// History navigation in response to horizontal overscroll (aka gesture-nav).
const base::Feature kOverscrollHistoryNavigation{
    "OverscrollHistoryNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether web apps can run periodic tasks upon network connectivity.
const base::Feature kPeriodicBackgroundSync{"PeriodicBackgroundSync",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If Pepper 3D Image Chromium is allowed, this feature controls whether it is
// enabled.
// TODO(https://crbug.com/1196009): Remove this feature, remove the code that
// uses it.
const base::Feature kPepper3DImageChromium{"Pepper3DImageChromium",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Kill-switch to introduce a compatibility breaking restriction.
const base::Feature kPepperCrossOriginRedirectRestriction{
    "PepperCrossOriginRedirectRestriction", base::FEATURE_ENABLED_BY_DEFAULT};

// Handle prefers-color-scheme user preference media feature via client hints.
const base::Feature kPrefersColorSchemeClientHintHeader{
    "PrefersColorSchemeClientHintHeader", base::FEATURE_ENABLED_BY_DEFAULT};

// All ProcessHost objects live on UI thread.
// https://crbug.com/904556
const base::Feature kProcessHostOnUI{"ProcessHostOnUI",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enable in-browser script loading for a brand new service worker.
const base::Feature kPlzServiceWorker{"PlzServiceWorker",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables process sharing for sites that do not require a dedicated process
// by using a default SiteInstance. Default SiteInstances will only be used
// on platforms that do not use full site isolation.
// Note: This feature is mutally exclusive with
// kProcessSharingWithStrictSiteInstances. Only one of these should be enabled
// at a time.
const base::Feature kProcessSharingWithDefaultSiteInstances{
    "ProcessSharingWithDefaultSiteInstances", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether cross-site frames should get their own SiteInstance even when
// strict site isolation is disabled. These SiteInstances will still be
// grouped into a shared default process based on BrowsingInstance.
const base::Feature kProcessSharingWithStrictSiteInstances{
    "ProcessSharingWithStrictSiteInstances", base::FEATURE_DISABLED_BY_DEFAULT};

// Tells the RenderFrameHost to send beforeunload messages on a different
// local frame interface which will handle the messages at a higher priority.
const base::Feature kHighPriorityBeforeUnload{
    "HighPriorityBeforeUnload", base::FEATURE_DISABLED_BY_DEFAULT};

// Under this flag bootstrap (aka startup) tasks will be prioritized. This flag
// is used by various modules to determine whether special scheduling
// arrangements need to be made to prioritize certain tasks.
const base::Feature kPrioritizeBootstrapTasks = {
    "PrioritizeBootstrapTasks", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the ProactivelySwapBrowsingInstance experiment. A browsing instance
// represents a set of frames that can script each other. Currently, Chrome does
// not always switch BrowsingInstance when navigating in between two unrelated
// pages. This experiment makes Chrome swap BrowsingInstances for cross-site
// HTTP(S) navigations when the BrowsingInstance doesn't contain any other
// windows.
const base::Feature kProactivelySwapBrowsingInstance{
    "ProactivelySwapBrowsingInstance", base::FEATURE_DISABLED_BY_DEFAULT};

// Fires the `pushsubscriptionchange` event defined here:
// https://w3c.github.io/push-api/#the-pushsubscriptionchange-event
// for subscription refreshes, revoked permissions or subscription losses
const base::Feature kPushSubscriptionChangeEvent{
    "PushSubscriptionChangeEvent", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Direct Sockets API.
const base::Feature kDirectSockets{"DirectSockets",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Causes hidden tabs with crashed subframes to be marked for reload, meaning
// that if a user later switches to that tab, the current page will be
// reloaded.  This will hide crashed subframes from the user at the cost of
// extra reloads.
const base::Feature kReloadHiddenTabsWithCrashedSubframes {
  "ReloadHiddenTabsWithCrashedSubframes",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Causes RenderAccessibilityHost messages to be handled initially on a thread
// pool before being forwarded to the browser main thread to avoid so the
// deserialization does not block it.
const base::Feature kRenderAccessibilityHostDeserializationOffMainThread{
    "RenderAccessibilityHostDeserializationOffMainThread",
    base::FEATURE_DISABLED_BY_DEFAULT};

// RenderDocument:
//
// Currently, a RenderFrameHost represents neither a frame nor a document, but a
// frame in a given process. A new one is created after a different-process
// navigation. The goal of RenderDocument is to get a new one for each document
// instead.
//
// Design doc: https://bit.ly/renderdocument
// Main bug tracker: https://crbug.com/936696

// Enable using the RenderDocument.
const base::Feature kRenderDocument{"RenderDocument",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
const base::Feature kSkipEarlyCommitPendingForCrashedFrame{
    "SkipEarlyCommitPendingForCrashedFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Run video capture service in the Browser process as opposed to a dedicated
// utility process
const base::Feature kRunVideoCaptureServiceInBrowserProcess{
    "RunVideoCaptureServiceInBrowserProcess",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables saving pages as Web Bundle.
const base::Feature kSavePageAsWebBundle{"SavePageAsWebBundle",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Browser-side feature flag for SecurePaymentConfirmation, which can be used to
// disable the feature. Enabling the browser-side feature by itself does not
// actually enable the feature by default. The feature is also controlled by the
// Blink runtime feature "SecurePaymentConfirmation". Both have to be enabled
// for SecurePaymentConfirmation to be available.
const base::Feature kSecurePaymentConfirmation{
    "SecurePaymentConfirmationBrowser", base::FEATURE_ENABLED_BY_DEFAULT};

// Used to enable API changes for Secure Payment Confirmation.
const base::Feature kSecurePaymentConfirmationAPIV2{
    "SecurePaymentConfirmationAPIV2", base::FEATURE_ENABLED_BY_DEFAULT};

// Used to control whether to remove the restriction that PaymentCredential in
// WebAuthn and secure payment confirmation method in PaymentRequest API must
// use a user verifying platform authenticator. When enabled, this allows using
// such devices as UbiKey on Linux, which can make development easier.
const base::Feature kSecurePaymentConfirmationDebug{
    "SecurePaymentConfirmationDebug", base::FEATURE_DISABLED_BY_DEFAULT};

// Make sendBeacon throw for a Blob with a non simple type.
const base::Feature kSendBeaconThrowForBlobWithNonSimpleType{
    "SendBeaconThrowForBlobWithNonSimpleType",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
// TODO(rouslan): Remove this.
const base::Feature kServiceWorkerPaymentApps{"ServiceWorkerPaymentApps",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, prefer to start service workers in an unused renderer process if
// available. This helps let navigations and service workers use the same
// process when a process was already created for a navigation but not yet
// claimed by it (as is common for navigations from the Android New Tab Page).
const base::Feature kServiceWorkerPrefersUnusedProcess{
    "ServiceWorkerPrefersUnusedProcess", base::FEATURE_DISABLED_BY_DEFAULT};

// Use this feature to experiment terminating a service worker when it doesn't
// control any clients: https://crbug.com/1043845.
const base::Feature kServiceWorkerTerminationOnNoControllee{
    "ServiceWorkerTerminationOnNoControllee",
    base::FEATURE_DISABLED_BY_DEFAULT};

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
// This feature is also enabled independently of this flag for cross-origin
// isolated renderers.
const base::Feature kSharedArrayBuffer{"SharedArrayBuffer",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
// If enabled, SharedArrayBuffer is present and can be transferred on desktop
// platforms. This flag is used only as a "kill switch" as we migrate towards
// requiring 'crossOriginIsolated'.
const base::Feature kSharedArrayBufferOnDesktop{
    "SharedArrayBufferOnDesktop", base::FEATURE_DISABLED_BY_DEFAULT};

// Signed HTTP Exchange prefetch cache for navigations
// https://crbug.com/968427
const base::Feature kSignedExchangePrefetchCacheForNavigations{
    "SignedExchangePrefetchCacheForNavigations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Signed Exchange Reporting for distributors
// https://www.chromestatus.com/features/5687904902840320
const base::Feature kSignedExchangeReportingForDistributors{
    "SignedExchangeReportingForDistributors", base::FEATURE_ENABLED_BY_DEFAULT};

// Subresource prefetching+loading via Signed HTTP Exchange
// https://www.chromestatus.com/features/5126805474246656
const base::Feature kSignedExchangeSubresourcePrefetch{
    "SignedExchangeSubresourcePrefetch", base::FEATURE_ENABLED_BY_DEFAULT};

// Origin-Signed HTTP Exchanges (for WebPackage Loading)
// https://www.chromestatus.com/features/5745285984681984
const base::Feature kSignedHTTPExchange{"SignedHTTPExchange",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Whether to send a ping to the inner URL upon navigation or not.
const base::Feature kSignedHTTPExchangePingValidity{
    "SignedHTTPExchangePingValidity", base::FEATURE_DISABLED_BY_DEFAULT};

// Delays RenderProcessHost shutdown by a few seconds to allow the subframe's
// process to be potentially reused. This aims to reduce process churn in
// navigations where the source and destination share subframes.
const base::Feature kSubframeShutdownDelay{"SubframeShutdownDelay",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<SubframeShutdownDelayType>::Option delay_types[] = {
    {SubframeShutdownDelayType::kConstant, "constant"},
    {SubframeShutdownDelayType::kConstantLong, "constant-long"},
    {SubframeShutdownDelayType::kHistoryBased, "history-based"},
    {SubframeShutdownDelayType::kHistoryBasedLong, "history-based-long"},
    {SubframeShutdownDelayType::kMemoryBased, "memory-based"}};
const base::FeatureParam<SubframeShutdownDelayType>
    kSubframeShutdownDelayTypeParam{&kSubframeShutdownDelay, "type",
                                    SubframeShutdownDelayType::kConstant,
                                    &delay_types};

// This is intended as a kill switch for the WebOTP Service feature. To enable
// this feature, the experimental web platform features flag should be set.
const base::Feature kWebOTP{"WebOTP", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables WebOTP calls in cross-origin iframes if allowed by Permissions
// Policy.
const base::Feature kWebOTPAssertionFeaturePolicy{
    "WebOTPAssertionFeaturePolicy", base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment allowing control over what requests are intercepted by Service
// Worker fetch events. By setting a Service-Worker-Subresource-Filter HTTP
// header on the document to some string, only requests which contain a fragment
// matching the header string will be intercepted. When not set, Service Workers
// will intercept all requests, as normal.
const base::Feature kServiceWorkerSubresourceFilter{
    "ServiceWorkerSubresourceFilter", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to isolate sites of documents that specify an eligible
// Cross-Origin-Opener-Policy header.  Note that this is only intended to be
// used on Android, which does not use strict site isolation. See
// https://crbug.com/1018656.
const base::Feature kSiteIsolationForCrossOriginOpenerPolicy{
    "SiteIsolationForCrossOriginOpenerPolicy",
    base::FEATURE_DISABLED_BY_DEFAULT};
// This feature param (true by default) controls whether sites are persisted
// across restarts.
const base::FeatureParam<bool>
    kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam{
        &kSiteIsolationForCrossOriginOpenerPolicy,
        "should_persist_across_restarts", true};
// This feature param controls the maximum size of stored sites.  Only used
// when persistence is also enabled.
const base::FeatureParam<int>
    kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam{
        &kSiteIsolationForCrossOriginOpenerPolicy, "stored_sites_max_size",
        100};
// This feature param controls the period of time for which the stored sites
// should remain valid. Only used when persistence is also enabled.
const base::FeatureParam<base::TimeDelta>
    kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam{
        &kSiteIsolationForCrossOriginOpenerPolicy, "expiration_timeout",
        base::TimeDelta::FromDays(7)};
// When enabled, OOPIFs will not try to reuse compatible processes from
// unrelated tabs.
const base::Feature kDisableProcessReuse{"DisableProcessReuse",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether SpareRenderProcessHostManager tries to always have a warm
// spare renderer process around for the most recently requested BrowserContext.
// This feature is only consulted in site-per-process mode.
const base::Feature kSpareRendererForSitePerProcess{
    "SpareRendererForSitePerProcess", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kStopVideoCaptureOnScreenLock{
    "StopVideoCaptureOnScreenLock", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the out-of-process Storage Service.
const base::Feature kStorageServiceOutOfProcess{
    "StorageServiceOutOfProcess", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether site isolation should use origins instead of scheme and
// eTLD+1.
const base::Feature kStrictOriginIsolation{"StrictOriginIsolation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables subresource loading with Web Bundles.
const base::Feature kSubresourceWebBundles{"SubresourceWebBundles",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
const base::Feature kSuppressDifferentOriginSubframeJSDialogs{
    "SuppressDifferentOriginSubframeJSDialogs",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Dispatch touch events to "SyntheticGestureController" for events from
// Devtool Protocol Input.dispatchTouchEvent to simulate touch events close to
// real OS events.
const base::Feature kSyntheticPointerActions{"SyntheticPointerActions",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables async touchpad pinch zoom events. We check the ACK of the first
// synthetic wheel event in a pinch sequence, then send the rest of the
// synthetic wheel events of the pinch sequence as non-blocking if the first
// event’s ACK is not canceled.
const base::Feature kTouchpadAsyncPinchEvents{"TouchpadAsyncPinchEvents",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS, LaCrOS and Windows.
const base::Feature kTouchpadOverscrollHistoryNavigation {
  "TouchpadOverscrollHistoryNavigation",
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls whether the Trusted Types API is available.
const base::Feature kTrustedDOMTypes{"TrustedDOMTypes",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// This feature is for a reverse Origin Trial, enabling SharedArrayBuffer for
// sites as they migrate towards requiring cross-origin isolation for these
// features.
// TODO(bbudge): Remove when the deprecation is complete.
// https://developer.chrome.com/origintrials/#/view_trial/303992974847508481
// https://crbug.com/1144104
const base::Feature kUnrestrictedSharedArrayBuffer{
    "UnrestrictedSharedArrayBuffer", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows user activation propagation to all frames having the same origin as
// the activation notifier frame.  This is an intermediate measure before we
// have an iframe attribute to declaratively allow user activation propagation
// to subframes.
const base::Feature kUserActivationSameOriginVisibility{
    "UserActivationSameOriginVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental replacement for the `User-Agent` header, defined in
// https://tools.ietf.org/html/draft-west-ua-client-hints.
const base::Feature kUserAgentClientHint{"UserAgentClientHint",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables comparing browser and renderer's DidCommitProvisionalLoadParams in
// RenderFrameHostImpl::VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch.
const base::Feature kVerifyDidCommitParams{"VerifyDidCommitParams",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the <video>.getVideoPlaybackQuality() API is enabled.
const base::Feature kVideoPlaybackQuality{"VideoPlaybackQuality",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables future V8 VM features
const base::Feature kV8VmFuture{"V8VmFuture",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables DevTools warnings about accesses to private network resources from
// secure contexts. See also feature `kBlockInsecurePrivateNetworkRequests`.
const base::Feature kWarnAboutSecurePrivateNetworkRequests{
    "WarnAboutSecurePrivateNetworkRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable window controls overlays for desktop PWAs
const base::Feature kWebAppWindowControlsOverlay{
    "WebAppWindowControlsOverlay", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly baseline compilation (Liftoff).
const base::Feature kWebAssemblyBaseline{"WebAssemblyBaseline",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable memory protection for code JITed for WebAssembly.
const base::Feature kWebAssemblyCodeProtection{
    "WebAssemblyCodeProtection", base::FEATURE_DISABLED_BY_DEFAULT};

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
// Use memory protection keys in userspace (PKU) (if available) to protect code
// JITed for WebAssembly. Fall back to traditional memory protection if
// WebAssemblyCodeProtection is also enabled.
const base::Feature kWebAssemblyCodeProtectionPku{
    "WebAssemblyCodeProtectionPku", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) &&
        // defined(ARCH_CPU_X86_64)

// Enable WebAssembly lazy compilation (JIT on first call).
const base::Feature kWebAssemblyLazyCompilation{
    "WebAssemblyLazyCompilation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly SIMD.
// https://github.com/WebAssembly/Simd
const base::Feature kWebAssemblySimd{"WebAssemblySimd",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly tiering (Liftoff -> TurboFan).
const base::Feature kWebAssemblyTiering{"WebAssemblyTiering",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly trap handler.
#if (defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN) || \
     defined(OS_MAC)) &&                                             \
    defined(ARCH_CPU_X86_64)
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kWebAssemblyTrapHandler{"WebAssemblyTrapHandler",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Controls whether the WebAuthentication API is enabled:
// https://w3c.github.io/webauthn
const base::Feature kWebAuth{"WebAuthentication",
                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether CTAP2 devices can communicate via the WebAuthentication API
// using pairingless BLE protocol.
// https://w3c.github.io/webauthn
const base::Feature kWebAuthCable {
  "WebAuthenticationCable",
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
// If updating this, also update kWebAuthCableServerLink.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Controls whether WebAuthn conditional UI requests are supported.
const base::Feature kWebAuthConditionalUI{"WebAuthenticationConditionalUI",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Web Bluetooth should use the new permissions backend. The
// new permissions backend uses ChooserContextBase, which is used by other
// device APIs, such as WebUSB.
const base::Feature kWebBluetoothNewPermissionsBackend{
    "WebBluetoothNewPermissionsBackend", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Web Bundles (Bundled HTTP Exchanges) is enabled.
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html
// When this feature is enabled, Chromium can load unsigned Web Bundles local
// file under file:// URL (and content:// URI on Android).
const base::Feature kWebBundles{"WebBundles",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// When this feature is enabled, Chromium will be able to load unsigned Web
// Bundles file under https: URL and localhost http: URL.
// TODO(crbug.com/1018640): Implement this feature.
const base::Feature kWebBundlesFromNetwork{"WebBundlesFromNetwork",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kWebGLImageChromium{"WebGLImageChromium",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebGPU on gpu serivce side only. This is used with origin trial
// before gpu service is enabled by default.
const base::Feature kWebGPUService{"WebGPUService",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable browser mediation API for federated identity interactions.
const base::Feature kWebID{"WebID", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls which backend is used to retrieve OTP on Android. When disabled
// we use User Consent API.
const base::Feature kWebOtpBackendAuto{"WebOtpBackendAuto",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// The JavaScript API for payments on the web.
const base::Feature kWebPayments{"WebPayments",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Minimal user interface experience for payments on the web.
const base::Feature kWebPaymentsMinimalUI{"WebPaymentsMinimalUI",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use GpuMemoryBuffer backed VideoFrames in media streams.
const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames{
    "WebRTC-UseGpuMemoryBufferVideoFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables report-only Trusted Types experiment on WebUIs
const base::Feature kWebUIReportOnlyTrustedTypes{
    "WebUIReportOnlyTrustedTypes", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the WebXR Device API is enabled.
const base::Feature kWebXr{"WebXR", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables access to AR features via the WebXR API.
const base::Feature kWebXrArModule{"WebXRARModule",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Allows the use of page zoom in place of accessibility text autosizing, and
// updated UI to replace existing Chrome Accessibility Settings.
const base::Feature kAccessibilityPageZoom{"AccessibilityPageZoom",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAccessibilityPageZoomUpdatedUI{
    "AccessibilityPageZoomUpdatedUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Sets moderate binding to background renderers playing media, when enabled.
// Else the renderer will have strong binding.
const base::Feature kBackgroundMediaRendererHasModerateBinding{
    "BackgroundMediaRendererHasModerateBinding",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, BindingManager will use Context.BIND_NOT_FOREGROUND to avoid
// affecting cpu scheduling priority.
const base::Feature kBindingManagementWaiveCpu{
    "BindingManagementWaiveCpu", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows the use of an experimental feature to drop any AccessibilityEvents
// that are not relevant to currently enabled accessibility services.
const base::Feature kOnDemandAccessibilityEvents{
    "OnDemandAccessibilityEvents", base::FEATURE_DISABLED_BY_DEFAULT};

// Screen Capture API support for Android
const base::Feature kUserMediaScreenCapturing{
    "UserMediaScreenCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

// Pre-warm up the network process on browser startup.
const base::Feature kWarmUpNetworkProcess{"WarmUpNetworkProcess",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the WebNFC feature. This feature can be enabled for all sites
// using the kEnableExperimentalWebPlatformFeatures flag.
// https://w3c.github.io/web-nfc/
const base::Feature kWebNfc{"WebNFC", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_MAC)
// Enables caching of media devices for the purpose of enumerating them.
const base::Feature kDeviceMonitorMac{"DeviceMonitorMac",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enable IOSurface based screen capturer.
const base::Feature kIOSurfaceCapturer{"IOSurfaceCapturer",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMacSyscallSandbox{"MacSyscallSandbox",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables retrying to obtain list of available cameras on Macbooks after
// restarting the video capture service if a previous attempt delivered zero
// cameras.
const base::Feature kRetryGetVideoCaptureDeviceInfos{
    "RetryGetVideoCaptureDeviceInfos", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDesktopCaptureMacV2{"DesktopCaptureMacV2",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWindowCaptureMacV2{"WindowCaptureMacV2",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // defined(OS_MAC)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// If the JavaScript on a WebUI page has an error (such as an unhandled
// exception), report that error back the crash reporting infrastructure, same
// as we do for program crashes.
const base::Feature kSendWebUIJavaScriptErrorReports{
    "SendWebUIJavaScriptErrorReports", base::FEATURE_DISABLED_BY_DEFAULT};
// Parameter: Should we send the error reports to the production server? If
// false, we send to the staging server, which is useful for developers (doesn't
// pollute the report database).
const char kSendWebUIJavaScriptErrorReportsSendToProductionVariation[] =
    "send_webui_js_errors_to_production";
const base::FeatureParam<bool>
    kWebUIJavaScriptErrorReportsSendToProductionParam{
        &kSendWebUIJavaScriptErrorReports,
        kSendWebUIJavaScriptErrorReportsSendToProductionVariation, true};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Controls whether the new subtree capture path is used for window capturing on
// ChromeOS Ash, instead of the legacy SlowWindowCapturerChromeOS
// implementation.
// TODO(crbug.com/1210549): remove once we have determined the new path is
// stable.
const base::Feature kAuraWindowSubtreeCapture{"AuraWindowSubtreeCapture",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
// Controls whether the PipeWire support for screen capturing is enabled on the
// Wayland display server.
const base::Feature kWebRtcPipeWireCapturer{"WebRTCPipeWireCapturer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(WEBRTC_USE_PIPEWIRE)

enum class VideoCaptureServiceConfiguration {
  kEnabledForOutOfProcess,
  kEnabledForBrowserProcess,
  kDisabled
};

bool ShouldEnableVideoCaptureService() {
  return base::FeatureList::IsEnabled(features::kMojoVideoCapture) &&
         base::FeatureList::IsEnabled(features::kMojoVideoCaptureSecondary);
}

VideoCaptureServiceConfiguration GetVideoCaptureServiceConfiguration() {
  if (!ShouldEnableVideoCaptureService())
    return VideoCaptureServiceConfiguration::kDisabled;

// On ChromeOS the service must run in the browser process, because parts of the
// code depend on global objects that are only available in the Browser process.
// See https://crbug.com/891961.
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  return VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
#else
#if defined(OS_WIN)
  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
#endif
  return base::FeatureList::IsEnabled(
             features::kRunVideoCaptureServiceInBrowserProcess)
             ? VideoCaptureServiceConfiguration::kEnabledForBrowserProcess
             : VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
#endif
}

bool IsVideoCaptureServiceEnabledForOutOfProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
}

bool IsVideoCaptureServiceEnabledForBrowserProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
}

}  // namespace features
