// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"
#include "base/feature_list.h"
#include "build/build_config.h"

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

// Allows popups during page unloading.
// TODO(https://crbug.com/937569): Remove this entirely in Chrome 88.
const base::Feature kAllowPopupsDuringPageUnload{
    "AllowPopupsDuringPageUnload", base::FEATURE_DISABLED_BY_DEFAULT};

// Accepts Origin-Signed HTTP Exchanges to be signed with certificates
// that do not have CanSignHttpExchangesDraft extension.
// TODO(https://crbug.com/862003): Remove when certificates with
// CanSignHttpExchangesDraft extension are available from trusted CAs.
const base::Feature kAllowSignedHTTPExchangeCertsWithoutExtension{
    "AllowSignedHTTPExchangeCertsWithoutExtension",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Blink to request fonts from the Android Downloadable Fonts API through
// the service implemented on the Java side.
const base::Feature kAndroidDownloadableFontsMatching{
    "AndroidDownloadableFontsMatching", base::FEATURE_ENABLED_BY_DEFAULT};

// Launches the audio service on the browser startup.
const base::Feature kAudioServiceLaunchOnStartup{
    "AudioServiceLaunchOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};

// Runs the audio service in a separate process.
const base::Feature kAudioServiceOutOfProcess {
  "AudioServiceOutOfProcess",
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables the audio-service sandbox. This feature has an effect only when the
// kAudioServiceOutOfProcess feature is enabled.
const base::Feature kAudioServiceSandbox {
  "AudioServiceSandbox",
#if defined(OS_WIN) || defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Kill switch for Background Fetch.
const base::Feature kBackgroundFetch{"BackgroundFetch",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable using the BackForwardCache.
const base::Feature kBackForwardCache{"BackForwardCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// BackForwardCache is disabled on low memory devices. The threshold is defined
// via a field trial param: "memory_threshold_for_back_forward_cache_in_mb"
// It is compared against base::SysInfo::AmountOfPhysicalMemoryMB().

// "BackForwardCacheMemoryControls" is checked before "BackForwardCache". It
// means the low memory devices will activate neither the control group nor the
// experimental group of the BackForwardCache field trial.
const base::Feature kBackForwardCacheMemoryControl{
    "BackForwardCacheMemoryControls", base::FEATURE_DISABLED_BY_DEFAULT};

// Block subresource requests whose URLs contain embedded credentials (e.g.
// `https://user:pass@example.com/resource`).
const base::Feature kBlockCredentialedSubresources{
    "BlockCredentialedSubresources", base::FEATURE_ENABLED_BY_DEFAULT};

// When kBlockInsecurePrivateNetworkRequests is enabled, requests initiated
// from a less-private network may only target a more-private network if the
// initiating context is secure.
//
// https://wicg.github.io/cors-rfc1918/#integration-fetch
const base::Feature kBlockInsecurePrivateNetworkRequests{
    "BlockInsecurePrivateNetworkRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// Use ThreadPriority::DISPLAY for browser UI and IO threads.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
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

// Enables support for parallel cache_storage operations via the
// "max_shared_ops" fieldtrial parameter.
const base::Feature kCacheStorageParallelOps{"CacheStorageParallelOps",
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

// Enables the use of out of process rasterization for canvas.
const base::Feature kCanvasOopRasterization{"CanvasOopRasterization",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClickPointerEvent{"ClickPointerEvent",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, code cache does not use a browsing_data filter for deletions.
extern const base::Feature kCodeCacheDeletionWithoutFilter{
    "CodeCacheDeletionWithoutFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, event.movement is calculated in blink instead of in browser.
const base::Feature kConsolidatedMovementXY{"ConsolidatedMovementXY",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the Conversion Measurement API is enabled.
const base::Feature kConversionMeasurement{"ConversionMeasurement",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Show messages in DevTools about upcoming deprecations that would affect
// sent/received cookies.
const base::Feature kCookieDeprecationMessages{
    "CookieDeprecationMessages", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Blink cooperative scheduling.
const base::Feature kCooperativeScheduling{"CooperativeScheduling",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables crash reporting via Reporting API.
// https://www.w3.org/TR/reporting/#crash-report
const base::Feature kCrashReporting{"CrashReporting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Puts save-data header in the holdback mode. This disables sending of
// save-data header to origins, and to the renderer processes within Chrome.
const base::Feature kDataSaverHoldback{"DataSaverHoldback",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable changing source dynamically for desktop capture.
const base::Feature kDesktopCaptureChangeSource{
    "DesktopCaptureChangeSource", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable document policy for configuring and restricting feature behavior.
const base::Feature kDocumentPolicy{"DocumentPolicy",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable document policy negotiation mechanism.
const base::Feature kDocumentPolicyNegotiation{
    "DocumentPolicyNegotiation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new canvas 2d api features. Enabled either with either
// enable-experimental-canvas-features or new-canvas-2d-api runtime flags
const base::Feature kEnableNewCanvas2DAPI{"EnableNewCanvas2DAPI",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If this feature is enabled and device permission is not granted by the user,
// media-device enumeration will provide at most one device per type and the
// device IDs will not be available.
// TODO(crbug.com/1019176): remove the feature in M89.
const base::Feature kEnumerateDevicesHideDeviceIDs{
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
  "ExperimentalAccessibilityLabels",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Throttle tasks in Blink background timer queues based on CPU budgets
// for the background tab. Bug: https://crbug.com/639852.
const base::Feature kExpensiveBackgroundTimerThrottling{
    "ExpensiveBackgroundTimerThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Extra CORS safelisted headers. See https://crbug.com/999054.
const base::Feature kExtraSafelistedRequestHeadersForOutOfBlinkCors{
    "ExtraSafelistedRequestHeadersForOutOfBlinkCors",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Client Hints are guarded by FeaturePolicy.
const base::Feature kFeaturePolicyForClientHints{
    "FeaturePolicyForClientHints", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled Feature Policy propagation is similar to sandbox flags and,
// sandbox flags are implemented on top of Feature Policy.
const base::Feature kFeaturePolicyForSandbox{"FeaturePolicyForSandbox",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not First Scroll Latency will be measured and reported.
// First Scroll Latency (tracking bug: crbug.com/922980) measures, for the
// first scroll of a navigation, the latency between the user's input and the
// display's update.
const base::Feature kFirstScrollLatencyMeasurement{
    "FirstScrollLatencyMeasurement", base::FEATURE_ENABLED_BY_DEFAULT};

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

// If a page does a client side redirect or adds to the history without a user
// gesture, then skip it on back/forward UI.
const base::Feature kHistoryManipulationIntervention{
    "HistoryManipulationIntervention", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables lowering the priority of the resources in iframes.
const base::Feature kLowPriorityIframes{"LowPriorityIframes",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Removes the association between the `AgentSchedulingGroup` interfaces and the
// IPC Channel. This will break ordering guarantees between different agent
// scheduling groups (ordering withing a group is still preserved).
// DO NOT USE! The feature is not yet fully implemented. See crbug.com/1111231.
const base::Feature kMbiDetachAgentSchedulingGroupFromChannel{
    "MbiDetachAgentSchedulingGroupFromChannel",
    base::FEATURE_DISABLED_BY_DEFAULT};

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

// A kill switch for the change for more code sharing between usual prefetch
// and no-state prefetch.
// TODO(yhirano): Remove this if M86 stable looks good.
const base::Feature kNoStatePrefetchUsingPrefetchLoader{
    "NoStatePrefetchUsingPrefetchLoader", base::FEATURE_ENABLED_BY_DEFAULT};

// Kill switch for Web Notification content images.
const base::Feature kNotificationContentImage{"NotificationContentImage",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the notification trigger API.
const base::Feature kNotificationTriggers{"NotificationTriggers",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Origin Isolation, via a header. See https://crbug.com/1042415
// Note that the origin policy-based variant of origin isolation is controlled
// by kOriginPolicy, instead.
const base::Feature kOriginIsolationHeader{"OriginIsolationHeader",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Policy. See https://crbug.com/751996
const base::Feature kOriginPolicy{"OriginPolicy",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// History navigation in response to horizontal overscroll (aka gesture-nav).
const base::Feature kOverscrollHistoryNavigation {
  "OverscrollHistoryNavigation",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Whether web apps can run periodic tasks upon network connectivity.
const base::Feature kPeriodicBackgroundSync{"PeriodicBackgroundSync",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable permission policy for configuring and restricting feature behavior.
const base::Feature kPermissionsPolicyHeader{"PermissionsPolicyHeader",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If Pepper 3D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kPepper3DImageChromium {
  "Pepper3DImageChromium",
#if defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Kill-switch to introduce a compatibility breaking restriction.
const base::Feature kPepperCrossOriginRedirectRestriction{
    "PepperCrossOriginRedirectRestriction", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Controls whether FileURLLoaderFactory can fetch additional files based on the
// isolated world's origin. This feature is disabled by default because we want
// content scripts to have the same permissions as the page they are injected
// into. This feature makes it possible to quickly revert to earlier permissive
// behavior if significant regressions are detected. See
// https://crbug.com/1049604.
const base::Feature kRelaxIsolatedWorldCorsInFileUrlLoaderFactory = {
    "RelaxIsolatedWorldCorsInFileUrlLoaderFactory",
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
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRequestUnbufferedDispatch{
    "RequestUnbufferedDispatch", base::FEATURE_ENABLED_BY_DEFAULT};

// Respect the MacOS system setting for subpixel text anti-aliasing.
// https://crbug.com/1079418.
const base::Feature kRespectMacLCDTextSetting{"RespectMacLCDTextSetting",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Run video capture service in the Browser process as opposed to a dedicated
// utility process
const base::Feature kRunVideoCaptureServiceInBrowserProcess{
    "RunVideoCaptureServiceInBrowserProcess",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables saving pages as Web Bundle.
const base::Feature kSavePageAsWebBundle{"SavePageAsWebBundle",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

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

// If enabled, ServiceWorkerContextCore lives on the UI thread rather than the
// IO thread.
// https://crbug.com/824858
const base::Feature kServiceWorkerOnUI{"ServiceWorkerOnUI",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

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
const base::Feature kSharedArrayBuffer {
  "SharedArrayBuffer",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

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

// This is intended as a kill switch for the SMS Receiver feature. To enable
// this feature, the experimental web platform features flag should be set,
// or the site should obtain an Origin Trial token.
const base::Feature kSmsReceiver{"SmsReceiver",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether Site Isolation protects against spoofing of origin in
// mojom::FileSystemManager::Open IPC from compromised renderer processes.  See
// also https://crbug.com/917457.
const base::Feature kSiteIsolationEnforcementForFileSystemApi{
    "SiteIsolationEnforcementForFileSystemApi",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether SpareRenderProcessHostManager tries to always have a warm
// spare renderer process around for the most recently requested BrowserContext.
// This feature is only consulted in site-per-process mode.
const base::Feature kSpareRendererForSitePerProcess{
    "SpareRendererForSitePerProcess", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Storage Pressure notifications and settings pages.
const base::Feature kStoragePressureUI {
  "StoragePressureUI",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables the out-of-process Storage Service.
const base::Feature kStorageServiceOutOfProcess{
    "StorageServiceOutOfProcess", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether site isolation should use origins instead of scheme and
// eTLD+1.
const base::Feature kStrictOriginIsolation{"StrictOriginIsolation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables subresource loading with Web Bundles.
const base::Feature kSubresourceWebBundles{"SubresourceWebBundles",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Dispatch touch events to "SyntheticGestureController" for events from
// Devtool Protocol Input.dispatchTouchEvent to simulate touch events close to
// real OS events.
const base::Feature kSyntheticPointerActions{"SyntheticPointerActions",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle Blink timers in out-of-view cross origin frames.
const base::Feature kTimerThrottlingForHiddenFrames{
    "TimerThrottlingForHiddenFrames", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables async touchpad pinch zoom events. We check the ACK of the first
// synthetic wheel event in a pinch sequence, then send the rest of the
// synthetic wheel events of the pinch sequence as non-blocking if the first
// event’s ACK is not canceled.
const base::Feature kTouchpadAsyncPinchEvents{"TouchpadAsyncPinchEvents",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Allows swipe left/right from touchpad change browser navigation. Currently
// only enabled by default on CrOS.
const base::Feature kTouchpadOverscrollHistoryNavigation {
  "TouchpadOverscrollHistoryNavigation",
#if defined(OS_CHROMEOS) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls whether the Trusted Types API is available.
const base::Feature kTrustedDOMTypes{"TrustedDOMTypes",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the RenderProcessHost uses its frames' priorities for
// determining if it should be backgrounded. When all frames associated with a
// RenderProcessHost are low priority, that process may be backgrounded even if
// those frames are visible.
const base::Feature kUseFramePriorityInRenderProcessHost{
    "UseFramePriorityInRenderProcessHost", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows user activation propagation to all frames having the same origin as
// the activation notifier frame.  This is an intermediate measure before we
// have an iframe attribute to declaratively allow user activation propagation
// to subframes.
const base::Feature kUserActivationSameOriginVisibility{
    "UserActivationSameOriginVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental replacement for the `User-Agent` header, defined in
// https://tools.ietf.org/html/draft-west-ua-client-hints.
const base::Feature kUserAgentClientHint{"UserAgentClientHint",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the <video>.getVideoPlaybackQuality() API is enabled.
const base::Feature kVideoPlaybackQuality{"VideoPlaybackQuality",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables future V8 VM features
const base::Feature kV8VmFuture{"V8VmFuture",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enable display_override manifest entry for web applications.
const base::Feature kWebAppManifestDisplayOverride{
    "WebAppManifestDisplayOverride", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly baseline compilation (Liftoff).
const base::Feature kWebAssemblyBaseline{"WebAssemblyBaseline",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly lazy compilation (JIT on first call).
const base::Feature kWebAssemblyLazyCompilation{
    "WebAssemblyLazyCompilation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly SIMD.
// https://github.com/WebAssembly/Simd
const base::Feature kWebAssemblySimd{"WebAssemblySimd",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enable WebAssembly tiering (Liftoff -> TurboFan).
const base::Feature kWebAssemblyTiering{"WebAssemblyTiering",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enable WebAssembly threads.
// https://github.com/WebAssembly/threads
const base::Feature kWebAssemblyThreads {
  "WebAssemblyThreads",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

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

// Controls whether the visibility of a WebContents can be OCCLUDED. When
// disabled, an occluded WebContents behaves exactly like a VISIBLE WebContents.
const base::Feature kWebContentsOcclusion {
  "WebContentsOcclusion",
#if defined(OS_MAC) || defined(OS_CHROMEOS) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls whether the WebAuthentication API is enabled:
// https://w3c.github.io/webauthn
const base::Feature kWebAuth{"WebAuthentication",
                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether CTAP2 devices can communicate via the WebAuthentication API
// using pairingless BLE protocol.
// https://w3c.github.io/webauthn
const base::Feature kWebAuthCable {
  "WebAuthenticationCable",
#if !defined(OS_CHROMEOS) && defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

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

// Enable experimental policy-controlled features and LAPIs
const base::Feature kExperimentalProductivityFeatures{
    "ExperimentalProductivityFeatures", base::FEATURE_DISABLED_BY_DEFAULT};

// When this feature is enabled, the Web OTP API will use the User Consent
// API to retrieve SMSes from the Android device.
const base::Feature kWebOtpBackend{"kWebOtpBackend",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// The JavaScript API for payments on the web.
// TODO(rouslan): Remove this.
const base::Feature kWebPayments{"WebPayments",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Minimal user interface experience for payments on the web.
const base::Feature kWebPaymentsMinimalUI{"WebPaymentsMinimalUI",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Makes WebRTC use ECDSA certs by default (i.e., when no cert type was
// specified in JS).
const base::Feature kWebRtcEcdsaDefault{"WebRTC-EnableWebRtcEcdsa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables access to raycasting against estimated XR scene geometry.
const base::Feature kWebXrHitTest{"WebXRHitTest",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables access to experimental WebXR features.
const base::Feature kWebXrIncubations{"WebXRIncubations",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Autofill Accessibility in Android.
// crbug.com/627860
const base::Feature kAndroidAutofillAccessibility{
    "AndroidAutofillAccessibility", base::FEATURE_ENABLED_BY_DEFAULT};

// Sets moderate binding to background renderers playing media, when enabled.
// Else the renderer will have strong binding.
const base::Feature kBackgroundMediaRendererHasModerateBinding{
    "BackgroundMediaRendererHasModerateBinding",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Pre-warm up the network process on browser startup.
const base::Feature kWarmUpNetworkProcess{"WarmUpNetworkProcess",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Force display to tick at ~60Hz refresh rate.
const base::Feature kForce60HzRefreshRate{"Force60HzRefreshRate",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the WebNFC feature. This feature can be enabled for all sites
// using the kEnableExperimentalWebPlatformFeatures flag or by a particular site
// if it includes an Origin Trial key.  https://w3c.github.io/web-nfc/
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

const base::Feature kMacV2GPUSandbox{"MacV2GPUSandbox",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables retrying to obtain list of available cameras on Macbooks after
// restarting the video capture service if a previous attempt delivered zero
// cameras.
const base::Feature kRetryGetVideoCaptureDeviceInfos{
    "RetryGetVideoCaptureDeviceInfos", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MAC)

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
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
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
