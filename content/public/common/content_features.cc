// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "content/common/buildflags.h"
#include "content/public/common/btm_utils.h"
#include "content/public/common/buildflags.h"

namespace features {

// All features in alphabetical order.

// Marks navigations as aborted when the NavigationHandle is destroyed mid
// navigation, likely due to a tab closure. This is a kill switch.
BASE_FEATURE(kAbortNavigationsFromTabClosures,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch to guard additional security checks performed by the browser
// process on opaque origins, such as when verifying source origins for
// postMessage. See https://crbug.com/40109437.
BASE_FEATURE(kAdditionalOpaqueOriginEnforcements,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Capture Android key event objects to send them to the web contents when the
// IME sends composition texts.
BASE_FEATURE(kAndroidCaptureKeyEvents, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the caret browsing a11y feature - can use arrow keys to navigate
// through web pages.
BASE_FEATURE(kAndroidCaretBrowsing, base::FEATURE_DISABLED_BY_DEFAULT);

// DevTools frontend for Android.
BASE_FEATURE(kAndroidDevToolsFrontend, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables media to continue playing in the background.
BASE_FEATURE(kAndroidEnableBackgroundMediaLargeFormFactors,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fallback to next named service slot if launching a privileged service process
// hangs. In practice, this means if GPU launch hanges, then retry it once.
BASE_FEATURE(kAndroidFallbackToNextSlot, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables IMEs to insert media content such as images, gifs and stickers.
BASE_FEATURE(kAndroidMediaInsertion, base::FEATURE_DISABLED_BY_DEFAULT);

// Warm up a spare renderer after each navigation on Android.
BASE_FEATURE(kAndroidWarmUpSpareRendererWithTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Create the spare renderer in DidStopLoading rather than in
// SpareRenderProcessHostManager::PrepareForFutureRequests.
const base::FeatureParam<std::string> kAndroidSpareRendererCreationTiming{
    &kAndroidWarmUpSpareRendererWithTimeout, "spare_renderer_creation_timing",
    kAndroidSpareRendererCreationAfterLoading};

// Whether to add a navigation throttle on Android to wait for the
// priority of the spare renderer to be graduated before starting
// the network request.
const base::FeatureParam<bool> kAndroidSpareRendererAddNavigationThrottle{
    &kAndroidWarmUpSpareRendererWithTimeout, "add_navigation_throttle", false};

// The delay for creating the Android spare renderer in
// SpareRenderProcessHostManager::PrepareForFutureRequests.
// The parameter will not be effective if
// `spare_renderer_creation_after_stop_loading` is enabled.
// Since the function is called during loading, a delay is introduced to avoid
// interfering with critical loading procedures.
const base::FeatureParam<int> kAndroidSpareRendererCreationDelayMs{
    &kAndroidWarmUpSpareRendererWithTimeout, "spare_renderer_creation_delay_ms",
    2000};

// The timeout for the created spare renderer after each navigation on Android.
// The created renderer will be destroyed after the timeout.
// A negative value indicates that no timeout will be set for the spare
// renderer.
const base::FeatureParam<int> kAndroidSpareRendererTimeoutSeconds{
    &kAndroidWarmUpSpareRendererWithTimeout, "spare_renderer_timeout_seconds",
    60};

// The lower memory limit to create a spare renderer after each navigation on
// Android.
const base::FeatureParam<int> kAndroidSpareRendererMemoryThreshold{
    &kAndroidWarmUpSpareRendererWithTimeout, "spare_renderer_memory_threshold",
    1077};

// Kill the spare renderer when the browser goes to the background to free
// resources.
const base::FeatureParam<bool> kAndroidSpareRendererKillWhenBackgrounded{
    &kAndroidWarmUpSpareRendererWithTimeout, "kill_when_backgrounded", false};

// Only allow the navigation related allocation to use the spare renderer.
const base::FeatureParam<bool> kAndroidSpareRendererOnlyForNavigation{
    &kAndroidWarmUpSpareRendererWithTimeout, "only_for_navigation", false};

// Only allow the navigation related allocation to use the spare renderer.
const base::FeatureParam<bool>
    kAndroidSpareRendererOnlyWarmupAfterWebPageLoaded{
        &kAndroidWarmUpSpareRendererWithTimeout,
        "only_warmup_after_web_page_loaded", false};

// Whether to allow attaching an inner WebContents not owned by the outer
// WebContents. This is for prototyping purposes and should not be enabled in
// production.
BASE_FEATURE(kAttachUnownedInnerWebContents, base::FEATURE_DISABLED_BY_DEFAULT);

// Launches the audio service on the browser startup.
BASE_FEATURE(kAudioServiceLaunchOnStartup, base::FEATURE_DISABLED_BY_DEFAULT);

// Runs the audio service in a separate process.
BASE_FEATURE(kAudioServiceOutOfProcess,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the audio-service sandbox. This feature has an effect only when the
// kAudioServiceOutOfProcess feature is enabled.
BASE_FEATURE(kAudioServiceSandbox,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for Background Fetch.
BASE_FEATURE(kBackgroundFetch, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable using the BackForwardCache.
BASE_FEATURE(kBackForwardCache, base::FEATURE_ENABLED_BY_DEFAULT);

// Set a time limit for the page to enter the cache. Disabling this prevents
// flakes during testing.
BASE_FEATURE(kBackForwardCacheEntryTimeout, base::FEATURE_ENABLED_BY_DEFAULT);

// BackForwardCache is disabled on low memory devices. The threshold is defined
// via a field trial param: "memory_threshold_for_back_forward_cache_in_mb"
// It is compared against base::SysInfo::AmountOfPhysicalMemoryMB().

// "BackForwardCacheMemoryControls" is checked before "BackForwardCache". It
// means the low memory devices will activate neither the control group nor the
// experimental group of the BackForwardCache field trial.

// BackForwardCacheMemoryControls is enabled only on Android to disable
// BackForwardCache for lower memory devices due to memory limitations.
BASE_FEATURE(kBackForwardCacheMemoryControls,

#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// Enables getting screenshots as shared images for back forward transitions
// in cross-document navigations.
BASE_FEATURE(kBackForwardTransitionsCrossDocSharedImage,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables getting screenshots as shared images for back forward transitions
// to native pages.
BASE_FEATURE(kBackForwardTransitionsNativePageSharedImage,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// If enabled, makes battery saver request heavy align wake ups.
BASE_FEATURE(kBatterySaverModeAlignWakeUps, base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, private network requests initiated from
// non-secure contexts in the `public` address space  are blocked.
//
// See also:
//  - https://wicg.github.io/private-network-access/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequestsFromPrivate
//  - kBlockInsecurePrivateNetworkRequestsFromUnknown
BASE_FEATURE(kBlockInsecurePrivateNetworkRequests,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, requests to localhost initiated from non-secure
// contexts in the `private` IP address space are blocked.
//
// See also:
//  - https://wicg.github.io/private-network-access/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequests
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsFromPrivate,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Broker file operations on disk cache in the Network Service.
// This is no-op if the network service is hosted in the browser process.
BASE_FEATURE(kBrokerFileOperationsOnDiskCacheInNetworkService,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the decision to bypass redirect checks to be made based on the
// specific request.
BASE_FEATURE(kBypassRedirectChecksPerRequest, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows pages with cache-control:no-store to enter the back/forward cache.
// Feature params can specify whether pages with cache-control:no-store can be
// restored if cookies change / if HTTPOnly cookies change.
// TODO(crbug.com/40189625): Remove this feature and clean up.
BASE_FEATURE(kCacheControlNoStoreEnterBackForwardCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This killswitch is distinct from the OT.
// It allows us to remotely disable the feature, and get it to stop working even
// on sites that are in possession of a valid token. When that happens, all API
// calls gated by the killswitch will fail graceully.
BASE_FEATURE(kCapturedSurfaceControlKillswitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Clear the window.name property for the top-level cross-site navigations that
// swap BrowsingContextGroups(BrowsingInstances).
BASE_FEATURE(kClearCrossSiteCrossBrowsingContextGroupWindowName,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCompositeBGColorAnimation, base::FEATURE_DISABLED_BY_DEFAULT);

// Gate access to cookie deprecation API which allows developers to opt in
// server side testing without cookies.
// (See https://developer.chrome.com/en/docs/privacy-sandbox/chrome-testing)
BASE_FEATURE(kCookieDeprecationFacilitatedTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set whether to enable cookie deprecation API for off-the-record profiles.
const base::FeatureParam<bool>
    kCookieDeprecationFacilitatedTestingEnableOTRProfiles{
        &kCookieDeprecationFacilitatedTesting, "enable_otr_profiles", false};

// The experiment label for the cookie deprecation (Mode A/B) study.
const base::FeatureParam<std::string> kCookieDeprecationLabel{
    &kCookieDeprecationFacilitatedTesting, kCookieDeprecationLabelName, ""};
// Set whether Ads APIs should be disabled for third-party cookie deprecation.
const base::FeatureParam<bool> kCookieDeprecationTestingDisableAdsAPIs{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kCookieDeprecationTestingDisableAdsAPIsName,
    /*default_value=*/false};

const char kCookieDeprecationLabelName[] = "label";

const char kCookieDeprecationTestingDisableAdsAPIsName[] = "disable_ads_apis";

// Adiitional FeatureParams for CookieDeprecationFacilitatedTesting are defined
// in chrome/browser/tpcd/experiment/tpcd_experiment_features.cc.

// Enables deferring the creation of the speculative RFH when the navigation
// starts. The creation of a speculative RFH consumes about 2ms and is blocking
// the network request. With this feature the creation will be deferred until
// the browser initializes the network request. The speculative RFH will be
// created while the network service is sending the request in parallel.
BASE_FEATURE(kDeferSpeculativeRFHCreation, base::FEATURE_DISABLED_BY_DEFAULT);
// When enabled, the browser will create the render process if necessary even
// if the speculative render frame host creation is deferred by feature
// DeferSpeculativeRFHCreation.
const base::FeatureParam<bool> kWarmupSpareProcessCreationWhenDeferRFH{
    &kDeferSpeculativeRFHCreation, "warmup_spare_process", false};
// When enabled, the browser will not try to create a speculative RFH after
// loading starts for BFCache restore and prerender activation. The
// `OnResponseStarted` function will be called immediately and the RFH will be
// created there.
const base::FeatureParam<bool> kCreateSpeculativeRFHFilterRestore{
    &kDeferSpeculativeRFHCreation, "create_speculative_rfh_filter_restore",
    false};
// When enabled, the creation of the speculative RFH will be delayed for
// a short time after the loading starts. The loading start functions are
// critical for performance. We try not to interfere with it.
// Zero or negative value will disable the delay and create the speculative
// RFH instantly.
const base::FeatureParam<int> kCreateSpeculativeRFHDelayMs{
    &kDeferSpeculativeRFHCreation, "create_speculative_rfh_delay_ms", 0};

// Delay the destructions of RenderFrameHostImpls during a navigation (on
// Unload) or frame Detach, by delaying the call to
// PendingDeletionCheckCompletedOnSubTree.
BASE_FEATURE(kDelayRfhDestructionsOnUnloadAndDetach,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kRfhDestructionsOnUnloadAndDetachTaskDelay{
        &kDelayRfhDestructionsOnUnloadAndDetach, "task_delay",
        base::TimeDelta()};

// When a device bound session
// (https://github.com/w3c/webappsec-dbsc/blob/main/README.md) is
// terminated, evict pages with cache-control:no-store from the
// BFCache. Note that if `kCacheControlNoStoreEnterBackForwardCache` is
// disabled, no such pages will be in the cache.
BASE_FEATURE(kDeviceBoundSessionTerminationEvictBackForwardCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the DevTools Privacy UI is displayed.
BASE_FEATURE(kDevToolsPrivacyUI, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Digital Goods API is enabled.
// https://github.com/WICG/digital-goods/
BASE_FEATURE(kDigitalGoodsApi,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the BTM (Bounce Tracking Mitigation) feature.
// On by default to allow for collecting metrics. All potentially dangerous
// behavior (database persistence, storage deletion) will be gated by params.
BASE_FEATURE(kBtm, "DIPS", base::FEATURE_ENABLED_BY_DEFAULT);

// Flag used to control the TTL for user interactions (separately from the
// |kBtm| feature flag).
BASE_FEATURE(kBtmTtl, "DIPSTtl", base::FEATURE_ENABLED_BY_DEFAULT);

// Set the time period that Chrome will wait for before clearing storage for a
// site after it performs some action (e.g. bouncing the user or using storage)
// without user interaction.
const base::FeatureParam<base::TimeDelta> kBtmGracePeriod{&kBtm, "grace_period",
                                                          base::Hours(1)};

// Set the cadence at which Chrome will attempt to clear incidental state
// repeatedly.
const base::FeatureParam<base::TimeDelta> kBtmTimerDelay{&kBtm, "timer_delay",
                                                         base::Hours(1)};

// Sets how long BTM maintains interactions and Web Authn Assertions (WAA) for
// a site.
//
// If a site in the BTM database has an interaction or WAA within the grace
// period a BTM-triggering action, then that action and all ensuing actions are
// protected from BTM clearing until the interaction and WAA "expire" as set
// by this param.
// NOTE: Updating this param name (to reflect WAA) is deemed unnecessary as far
// as readability is concerned.
const base::FeatureParam<base::TimeDelta> kBtmInteractionTtl{
    &kBtmTtl, "interaction_ttl", base::Days(45)};

constexpr base::FeatureParam<content::BtmTriggeringAction>::Option
    kBtmTriggeringActionOptions[] = {
        {content::BtmTriggeringAction::kNone, "none"},
        {content::BtmTriggeringAction::kBounce, "bounce"}};

// Sets the actions which will trigger BTM clearing for a site. The default is
// to set to |kBounce|, but can be overridden by Finch experiment groups,
// command-line flags, or chrome flags.
const base::FeatureParam<content::BtmTriggeringAction> kBtmTriggeringAction{
    &kBtm, "triggering_action", content::BtmTriggeringAction::kBounce,
    &kBtmTriggeringActionOptions};

// Denotes the length of a time interval within which any client-side redirect
// is viewed as a bounce (provided all other criteria are equally met). The
// interval starts every time a page finishes a navigation (a.k.a. a commit is
// registered).
const base::FeatureParam<base::TimeDelta> kBtmClientBounceDetectionTimeout{
    &kBtm, "client_bounce_detection_timeout", base::Seconds(10)};

// Enables Bounce Tracking Mitigations for Dual Use sites.
BASE_FEATURE(kBtmDualUse, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables HW decode acceleration for WebRTC.
BASE_FEATURE(kWebRtcHWDecoding,
             "webrtc-hw-decoding",
// TODO: b/336314537 Re enable HW Decoding once the GPU Hang is resolved
#if BUILDFLAG(PLATFORM_CFM)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
// Enables HW encode acceleration for WebRTC.
BASE_FEATURE(kWebRtcHWEncoding,
             "webrtc-hw-encoding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a discard operation on WebContents to free associated resources.
// Eliminates the need to destroy the WebContents object to free its resources.
BASE_FEATURE(kWebContentsDiscard,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables fast-shutdown to ignore workers during urgent discards on certain
// platforms.
const base::FeatureParam<bool> kUrgentDiscardIgnoreWorkers{
    &kWebContentsDiscard, "urgent_discard_ignore_workers", false};

// When this feature is enabled, partial storage cleanup will be
// disabled for the GPU disk cache. (Performance improvement)
BASE_FEATURE(kDisablePartialStorageCleanupForGPUDiskCache,
             "PerformStorageCleanupForGPUDiskCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable drawing under System Bars within DisplayCutout.
BASE_FEATURE(kDrawCutoutEdgeToEdge, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable establishing the GPU channel early in renderer startup.
BASE_FEATURE(kEarlyEstablishGpuChannel, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables canvas 2d methods BeginLayer and EndLayer.
BASE_FEATURE(kEnableCanvas2DLayers, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables javaless renderers.
BASE_FEATURE(kEnableJavalessRenderers,
             "JavalessRenderers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome-untrusted:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeUntrusted,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeScheme,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Ensures the renderer is not dead when getting the process host for a site
// instance.
BASE_FEATURE(kEnsureExistingRendererAlive, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables JavaScript API to intermediate federated identity requests.
// Note that actual exposure of the FedCM API to web content is controlled
// by the flag in RuntimeEnabledFeatures on the blink side. See also
// the use of kSetOnlyIfOverridden in content/child/runtime_features.cc.
// We enable it here by default to support use in origin trials.
BASE_FEATURE(kFedCm, base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for checking if there is an ongoing embedder task in the auto
// re-authn flow.
BASE_FEATURE(kFedCmEmbedderCheck, base::FEATURE_ENABLED_BY_DEFAULT);

// Support usernames and phone numbers to identify users, instead of
// (or in addition to) names and emails.
BASE_FEATURE(kFedCmAlternativeIdentifiers, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables RPs to enhance autofill with federated accounts fetched by the FedCM
// API.
BASE_FEATURE(kFedCmAutofill, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM Delegation API.
BASE_FEATURE(kFedCmDelegation, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the spec-compliant 'error' attribute in IdentityCredentialError while
// deprecating the legacy 'code' attribute.
BASE_FEATURE(kFedCmErrorAttribute, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM IdP Registration API.
BASE_FEATURE(kFedCmIdPRegistration, base::FEATURE_DISABLED_BY_DEFAULT);

// For cross-site iframes, sends the top-level origin to the IDP and parses
// an optional returned boolean indicating whether it is part of the same
// client to allow for UI decisions based on the boolean.
BASE_FEATURE(kFedCmIframeOrigin, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Lightweight FedCM Mode
BASE_FEATURE(kFedCmLightweightMode, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with metrics endpoint at the same time.
BASE_FEATURE(kFedCmMetricsEndpoint, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Nonce usage in Params
BASE_FEATURE(kFedCmNonceInParams, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether FedCM requires explicit endpoint declaration in well-known
// files when client_metadata is used. When enabled, accounts_endpoint and
// login_url must be present in .well-known/web-identity for privacy validation.
BASE_FEATURE(kFedCmWellKnownEndpointValidation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables bypassing the well-known file enforcement.
BASE_FEATURE(kFedCmWithoutWellKnownEnforcement,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM IdP-Initiation API.
BASE_FEATURE(kFedCmNavigationInterception, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables browser-side focus verification when crossing fenced boundaries.
BASE_FEATURE(kFencedFramesEnforceFocus, base::FEATURE_DISABLED_BY_DEFAULT);

// This is a kill switch for focusing the RenderWidgetHostViewAndroid on
// ActionDown on every touch sequence if not focused already, please see
// crbug.com/381820236. The root view, RWHVA, is always focused in Chrome,
// however this might not be true on WebView, see crbug.com/378779896 for more
// details.
#if BUILDFLAG(IS_ANDROID)
// Enable AL device fluid resize.
BASE_FEATURE(kFluidResize, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFocusRenderWidgetHostViewAndroidOnActionDown,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether a memory pressure signal in a renderer should be forwarded to Blink
// isolates. Forwarding the signal triggers a GC (critical) or starts
// incremental marking (moderate), see `v8::Heap::CheckMemoryPressure`.
BASE_FEATURE(kForwardMemoryPressureToBlinkIsolates,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Digital Credential API.
BASE_FEATURE(kWebIdentityDigitalCredentials, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Digital Credentials Creation API.
BASE_FEATURE(kWebIdentityDigitalCredentialsCreation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables scrollers inside Blink to store scroll offsets in fractional
// floating-point numbers rather than truncating to integers.
BASE_FEATURE(kFractionalScrollOffsets, base::FEATURE_DISABLED_BY_DEFAULT);

// Puts network quality estimate related Web APIs in the holdback mode. When the
// holdback is enabled the related Web APIs return network quality estimate
// set by the experiment (regardless of the actual quality).
BASE_FEATURE(kNetworkQualityEstimatorWebHoldback,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether GuestViews (see components/guest_view/README.md) are implemented
// using MPArch inner pages. See https://crbug.com/40202416
BASE_FEATURE(kGuestViewMPArch, base::FEATURE_DISABLED_BY_DEFAULT);

// See crbug.com/359623664
BASE_FEATURE(kIdbPrioritizeForegroundClients,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we ignore duplicate navigations or not, in favor of
// preserving the already ongoing navigation.
BASE_FEATURE(kIgnoreDuplicateNavs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDuplicateNavThreshold,
                   &kIgnoreDuplicateNavs,
                   "duplicate_nav_threshold",
                   base::Milliseconds(2000));
BASE_FEATURE_PARAM(bool,
                   kSkipIgnoreRendererInitiatedNavs,
                   &kIgnoreDuplicateNavs,
                   "skip_ignore_renderer_initiated_navs",
                   false);
// Comma-separated list of origins for which we ignore duplicate navigations.
// An empty list means all origins are affected.
BASE_FEATURE_PARAM(std::string,
                   kIgnoreDuplicateNavsOrigins,
                   &kIgnoreDuplicateNavs,
                   "ignore_duplicate_navs_origins",
                   "");

// Kill switch for the GetInstalledRelatedApps API.
BASE_FEATURE(kInstalledApp, base::FEATURE_ENABLED_BY_DEFAULT);

// Allow Windows specific implementation for the GetInstalledRelatedApps API.
BASE_FEATURE(kInstalledAppProvider, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, derives isolate priority from the more granular process
// priority (user-blocking, user-visible, best-effort) instead of renderer
// visibility (visible, hidden).
//
// Subtlety: A renderer hosting a hidden frame playing audio will have
// user-blocking priority. Without this feature, an isolate in this renderer
// would have best-effort priority (derived from the visibility), whereas with
// the feature it would be user-blocking. To keep isolates in hidden renderers
// at best-effort priority, but otherwise use the process priority, enable this
// feature along with "IsolatesPriorityBestEffortWhenHidden".
BASE_FEATURE(kIsolatesPriorityUseProcessPriority,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, forces the priority of isolates in hidden renderers to
// best-effort, overriding the effect of kIsolatesPriorityUseProcessPriority
// (isolates in visible renderer will still get their priority derived from
// process priority).
BASE_FEATURE(kIsolatesPriorityBestEffortWhenHidden,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable support for isolated web apps. This will guard features like serving
// isolated web apps via the isolated-app:// scheme, and other advanced isolated
// app functionality. See https://github.com/reillyeon/isolated-web-apps for a
// general overview.
// Please don't use this feature flag directly to guard the IWA code.  Use
// IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled() in the browser process or
// check kEnableIsolatedWebAppsInRenderer command line flag in the renderer
// process.
BASE_FEATURE(kIsolatedWebApps,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables process isolation of fenced content (content inside fenced frames)
// from non-fenced content. See
// https://github.com/WICG/fenced-frame/blob/master/explainer/process_isolation.md
// for rationale and more details.
BASE_FEATURE(kIsolateFencedFrames, base::FEATURE_DISABLED_BY_DEFAULT);

// Alternative to switches::kIsolateOrigins, for turning on origin isolation.
// List of origins to isolate has to be specified via
// kIsolateOriginsFieldTrialParamName.
BASE_FEATURE(kIsolateOrigins, base::FEATURE_DISABLED_BY_DEFAULT);
const char kIsolateOriginsFieldTrialParamName[] = "OriginsList";

// When enabled, creation of the BrowserInterfaceBroker on RenderFrameHostImpls
// becomes lazy. i.e. the BrowserInterfaceBroker is constructed only when it is
// needed, typically when a renderer process becomes associated with the frame.
// See https://crbug.com/450912216 for more details.
BASE_FEATURE(kLazyBrowserInterfaceBroker, base::FEATURE_DISABLED_BY_DEFAULT);

// If this is enabled, LoadingPredictor restricts the number of preconnects for
// the same destination to one.
BASE_FEATURE(kLoadingPredictorLimitPreconnectSocketCount,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLogJsConsoleMessages,
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// The MBI mode controls whether or not communication over the
// AgentSchedulingGroup is ordered with respect to the render-process-global
// legacy IPC channel, as well as the granularity of AgentSchedulingGroup
// creation. This will break ordering guarantees between different agent
// scheduling groups (ordering withing a group is still preserved).
// DO NOT USE! The feature is not yet fully implemented. See crbug.com/1111231.
BASE_FEATURE(kMBIMode,
#if BUILDFLAG(MBI_MODE_PER_RENDER_PROCESS_HOST) || \
    BUILDFLAG(MBI_MODE_PER_SITE_INSTANCE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const base::FeatureParam<MBIMode>::Option mbi_mode_types[] = {
    {MBIMode::kLegacy, "legacy"},
    {MBIMode::kEnabledPerRenderProcessHost, "per_render_process_host"},
    {MBIMode::kEnabledPerSiteInstance, "per_site_instance"}};
const base::FeatureParam<MBIMode> kMBIModeParam{
    &kMBIMode, "mode",
#if BUILDFLAG(MBI_MODE_PER_RENDER_PROCESS_HOST)
    MBIMode::kEnabledPerRenderProcessHost,
#elif BUILDFLAG(MBI_MODE_PER_SITE_INSTANCE)
    MBIMode::kEnabledPerSiteInstance,
#else
      MBIMode::kLegacy,
#endif
    &mbi_mode_types};

// Controls the configurablity of the navigation confidence noise level.
// If the feature is not enabled, then the epsilon value will be 1.1.
BASE_FEATURE(kNavigationConfidenceEpsilon, base::FEATURE_DISABLED_BY_DEFAULT);
// The epsilon value returned if `kNavigationConfidenceNoise` is enabled.
const base::FeatureParam<double> kNavigationConfidenceEpsilonValue{
    &kNavigationConfidenceEpsilon, "navigation-confidence-epsilon-value", 1.1};

// When NavigationNetworkResponseQueue is enabled, the browser will schedule
// some tasks related to navigation network responses in a kHigh priority
// queue.
BASE_FEATURE(kNavigationNetworkResponseQueue,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// If the network service is enabled, runs it in process.
BASE_FEATURE(kNetworkServiceInProcess,
             "NetworkServiceInProcess2",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Feature which holdbacks NoStatePrefetch on all surfaces.
BASE_FEATURE(kNoStatePrefetchHoldback, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the Origin-Agent-Cluster header. Tracking bug
// https://crbug.com/1042415; flag removal bug (for when this is fully launched)
// https://crbug.com/1148057.
//
// The name is "OriginIsolationHeader" because that was the old name when the
// feature was under development.
BASE_FEATURE(kOriginIsolationHeader, base::FEATURE_ENABLED_BY_DEFAULT);

// History navigation in response to horizontal overscroll (aka gesture-nav).
BASE_FEATURE(kOverscrollHistoryNavigation, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables additional ChildProcessSecurityPolicy enforcements for PDF renderer
// processes, including blocking storage and cookie access for them.
//
// TODO(https://crbug.com/40205612): Remove this kill switch once the PDF
// enforcements are verified not to cause problems.
BASE_FEATURE(kPdfEnforcements, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether web apps can run periodic tasks upon network connectivity.
BASE_FEATURE(kPeriodicBackgroundSync, base::FEATURE_DISABLED_BY_DEFAULT);

// Use code paths for prefetch/prerender integration.
// See also `kPrerender2FallbackPrefetchSpecRules`.
BASE_FEATURE(kPrefetchPrerenderIntegration, base::FEATURE_DISABLED_BY_DEFAULT);

// If explicitly disabled, prefetch proxy is not used.
BASE_FEATURE(kPrefetchProxy, base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch for UA override issue fix (crbug.com/441612842) in preloading.
BASE_FEATURE(kPreloadingRespectUserAgentOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the feature allows the prerender host to be reused for the
// future same-site page prerender if marked as reusable.
BASE_FEATURE(kPrerender2ReuseHost, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the feature parameter allows all the prerender hosts for
// DSE search results to be reused.
BASE_FEATURE_PARAM(bool,
                   kPrerender2ReuseSearchResultHost,
                   &features::kPrerender2ReuseHost,
                   "reuse_search_host",
                   false);

// Enables exposure of ads APIs in the renderer: Attribution Reporting,
// FLEDGE, Topics, along with a number of other features actively in development
// within these APIs.
BASE_FEATURE(kPrivacySandboxAdsAPIsOverride, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ProcessSelectionDeferringConditions will be run. This allows
// the embedder to provide conditions that may delay the final process selection
// until the conditions have their results.
BASE_FEATURE(kProcessSelectionDeferringConditions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables origin-keyed processes by default, unless origins opt out using
// Origin-Agent-Cluster: ?0. This feature only takes effect if the Blink feature
// OriginAgentClusterDefaultEnable is enabled, since origin-keyed processes
// require origin-agent-clusters.
BASE_FEATURE(kOriginKeyedProcessesByDefault, base::FEATURE_DISABLED_BY_DEFAULT);

// Fires the `pushsubscriptionchange` event defined here:
// https://w3c.github.io/push-api/#the-pushsubscriptionchange-event
// for subscription refreshes, revoked permissions or subscription losses
BASE_FEATURE(kPushSubscriptionChangeEventOnInvalidation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Fires the `pushsubscriptionchange` event defined here:
// https://w3c.github.io/push-api/#the-pushsubscriptionchange-event
// upon manual resubscription to previously unsubscribed notifications.
BASE_FEATURE(kPushSubscriptionChangeEventOnResubscribe,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, queues navigations instead of cancelling the previous
// navigation if the previous navigation is already waiting for commit.
// See https://crbug.com/838348 and https://crbug.com/1220337.
BASE_FEATURE(kQueueNavigationsWhileWaitingForCommit,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, sends SubresourceResponseStarted IPC only when the user has
// allowed any HTTPS-related warning exceptions. From field data, ~100% of
// subresource notifications are not required, since allowing certificate
// exceptions by users is a rare event. Hence, if user has never allowed any
// certificate or HTTP exceptions, notifications are not sent to the browser.
// Once we start sending these messages, we keep sending them until all
// exceptions are revoked and browser restart occurs.
BASE_FEATURE(kReduceSubresourceResponseStartedIPC,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
// When a Web application is video-capturing a tab, it can use the Region
// Capture API to crop the resulting video.
// - If `kRegionCaptureOfOtherTabs` is disabled, the Web application can only
// crop self-capture tracks. (That is, cropping is only possible when the
// application is capturing its own tab.)
// - If `kRegionCaptureOfOtherTabs` is enabled, the Web application  can crop
// video-captures of any tab (so long as that other tab collaborates by sending
// a CropTarget).
BASE_FEATURE(kRegionCaptureOfOtherTabs, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

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
BASE_FEATURE(kRenderDocument, base::FEATURE_ENABLED_BY_DEFAULT);

// Restrict the maximum number of concurrent ThreadPool tasks when a renderer is
// low priority.
BASE_FEATURE(kRestrictThreadPoolInBackground,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature that stops broadcasting the history index and length when
// CreateRenderViewForRenderManager() is invoked, and instead passes the
// information in the CreateViewParams, saving some IPC calls.
BASE_FEATURE(kSetHistoryInfoOnViewCreation, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, sends the spare renderer information when setting the
// priority of renderers. Currently only Android handles the spare renderer
// information in priority.
// The target priority of a spare renderer in Android is decided by the feature
// parameters in ContentFeatureList.java.
BASE_FEATURE(kSpareRendererProcessPriority, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, set a soft limit on the number of renderer processes on
// Android, after which Chrome will reuse existing processes when possible.
// This diverges from current Clank behavior, where we do not set any upper
// bound and instead delegate that to the system. 42 is approximated from
// 8GBs ((8192 - 1024) / (16384 / 96)), and has nothing to do with Douglas
// Adams' book. 1GB is a carve-out for integrated GPU VRAM.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRendererProcessLimitOnAndroid, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kRendererProcessLimitOnAndroidCount,
                   &kRendererProcessLimitOnAndroid,
                   "count",
                   42u);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables retrying to obtain list of available cameras after restarting the
// video capture service if a previous attempt failed, which could be caused
// by a service crash.
BASE_FEATURE(kRetryGetVideoCaptureDeviceInfos,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Reuses RenderProcessHost up to a certain threshold. This mode ignores the
// soft process limit and behaves just like a process-per-site policy for all
// sites, with an additional restriction that a process may only be reused while
// the number of main frames in that process stays below a threshold.
BASE_FEATURE(kProcessPerSiteUpToMainFrameThreshold,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies the threshold for `kProcessPerSiteUpToMainFrameThreshold` feature.
constexpr base::FeatureParam<int> kProcessPerSiteMainFrameThreshold{
    &kProcessPerSiteUpToMainFrameThreshold, "ProcessPerSiteMainFrameThreshold",
    2};

// Specifies the scaling factor for `kProcessPerSiteUpToMainFrameThreshold`
// feature. This factor will be multiplied to the calculated size of a top
// level frame in the process and ensure there is more than that enough
// space in the process. For example if the expected size of a top level frame
// was 100K, and the factor was 1.5, the process must have 150K left in its
// allocation limit.
constexpr base::FeatureParam<double> kProcessPerSiteMainFrameSiteScalingFactor{
    &kProcessPerSiteUpToMainFrameThreshold,
    "ProcessPerSiteMainFrameSiteScalingFactor", 1.5f};

// Specifies the total memory limit for `kProcessPerSiteUpToMainFrameThreshold`
// feature. This is a limit of the private memory footprint calculation, if
// adding an additional top level frame would take us over this limit the
// addition will be denied. An application may indeed allocate more than this
// but we use this limit as a heuristic only.
constexpr base::FeatureParam<double> kProcessPerSiteMainFrameTotalMemoryLimit{
    &kProcessPerSiteUpToMainFrameThreshold,
    "ProcessPerSiteMainFrameTotalMemoryLimit", 2 * 1024 * 1024 * 1024u};

// Enables auto preloading for fetch requests before invoking the fetch handler
// in ServiceWorker. The fetch request inside the fetch handler is resolved with
// this preload response. If the fetch handler result is fallback, uses this
// preload request as a fallback network request.
//
// Unlike navigation preload, this preloading is applied to subresources. Also,
// it doesn't require a developer opt-in.
//
// crbug.com/1472634 for more details.
BASE_FEATURE(kServiceWorkerAutoPreload, base::FEATURE_ENABLED_BY_DEFAULT);

// crbug.com/374606637: When this is enabled, race-network-and-fetch-hander will
// prioritize the response processing for the network request over the
// processing for the fetch handler.
BASE_FEATURE(kServiceWorkerStaticRouterRaceNetworkRequestPerformanceImprovement,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Run video capture service in the Browser process as opposed to a dedicated
// utility process.
BASE_FEATURE(kRunVideoCaptureServiceInBrowserProcess,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Update scheduler settings using resourced on ChromeOS.
BASE_FEATURE(kSchedQoSOnResourcedForChrome, base::FEATURE_DISABLED_BY_DEFAULT);

// Browser-side feature flag for Secure Payment Confirmation (SPC) that also
// controls the render side feature state. SPC is not currently available on
// Linux or ChromeOS, as it requires platform authenticator support.
BASE_FEATURE(kSecurePaymentConfirmation,
             "SecurePaymentConfirmationBrowser",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Used to control whether to remove the restriction that PaymentCredential in
// WebAuthn and secure payment confirmation method in PaymentRequest API must
// use a user verifying platform authenticator. When enabled, this allows using
// such devices as UbiKey on Linux, which can make development easier.
BASE_FEATURE(kSecurePaymentConfirmationDebug,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
// TODO(rouslan): Remove this.
BASE_FEATURE(kServiceWorkerPaymentApps, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, UI thread tasks can check ServiceWorker registration information
// from the thread pool without waiting for running the receiving task. Please
// see crbug.com/421530699 for more details.
BASE_FEATURE(kServiceWorkerBackgroundUpdateForRegisteredStorageKeys,
             base::FEATURE_ENABLED_BY_DEFAULT);

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
// This feature is also enabled independently of this flag for cross-origin
// isolated renderers.
BASE_FEATURE(
    kSharedArrayBuffer,
#if BUILDFLAG(PLATFORM_CFM)
    // Supports x-on-meet-coop interop implementation.
    // TODO: crbug.com/398741358 - clean up this temporary workaround after
    // https://crbug.com/333029146 replaces COOP restrict-properties.
    base::FEATURE_ENABLED_BY_DEFAULT
#else
    base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, GetUserMedia API will only work when the concerned tab is in
// focus
BASE_FEATURE(kUserMediaCaptureOnFocus, base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to enabled updating installed PWAs more predictably by considering
// changes in icon urls.
BASE_FEATURE(kWebAppPredictableAppUpdating, base::FEATURE_DISABLED_BY_DEFAULT);

// This is intended as a kill switch for the WebOTP Service feature. To enable
// this feature, the experimental web platform features flag should be set.
BASE_FEATURE(kWebOTP, base::FEATURE_ENABLED_BY_DEFAULT);

// Trial to disable synchronous draw for synchronous compositor (ie Android
// WebView).
BASE_FEATURE(kWebViewAsyncDrawOnly, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the web lockscreen API implementation
// (https://github.com/WICG/lock-screen) in Chrome.
BASE_FEATURE(kWebLockScreenApi, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, puts subframe data: URLs in a separate SiteInstance in the same
// SiteInstanceGroup as the initiator.
BASE_FEATURE(kSiteInstanceGroupsForDataUrls, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, puts non-isolated sites in separate SiteInstances in a default
// SiteInstanceGroup (per BrowsingInstance), rather than sharing a default
// SiteInstance.
BASE_FEATURE(kDefaultSiteInstanceGroups, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to isolate sites of documents that specify an eligible
// Cross-Origin-Opener-Policy header.  Note that this is only intended to be
// used on Android, which does not use strict site isolation. See
// https://crbug.com/1018656.
BASE_FEATURE(kSiteIsolationForCrossOriginOpenerPolicy,
// Enabled by default on Android only; see https://crbug.com/1206770.
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
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
        base::Days(7)};

// When enabled, OOPIFs will not try to reuse compatible processes from
// unrelated tabs.
BASE_FEATURE(kDisableProcessReuse, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether SpareRenderProcessHostManager tries to always have a warm
// spare renderer process around for the most recently requested BrowserContext.
// This feature is only consulted in site-per-process mode.
BASE_FEATURE(kSpareRendererForSitePerProcess, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether site isolation should use origins instead of scheme and
// eTLD+1.
BASE_FEATURE(kStrictOriginIsolation, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, RenderWidgetHost in BFCache doesn't contribute to the priority
// of the renderer process.
BASE_FEATURE(kSubframePriorityContribution, base::FEATURE_ENABLED_BY_DEFAULT);

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
BASE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dispatch touch events to "SyntheticGestureController" for events from
// Devtool Protocol Input.dispatchTouchEvent to simulate touch events close to
// real OS events.
BASE_FEATURE(kSyntheticPointerActions, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature allows touch dragging and a context menu to occur
// simultaneously, with the assumption that the menu is non-modal.  Without this
// feature, a long-press touch gesture can start either a drag or a context-menu
// in Blink, not both (more precisely, a context menu is shown only if a drag
// cannot be started).
BASE_FEATURE(kTouchDragAndContextMenu,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// When the context menu is triggered, the browser allows motion in a small
// region around the initial touch location menu to allow for finger jittering.
// This param holds the movement threshold in DIPs to consider drag an
// intentional drag, which will dismiss the current context menu and prevent new
//  menu from showing.
const base::FeatureParam<int> kTouchDragMovementThresholdDip{
    &kTouchDragAndContextMenu, "DragAndDropMovementThresholdDipParam", 60};
#endif

// Controls whether the browser should track and reuse free and empty renderer
// processes. When enabled, the browser maintains a list of renderer processes
// that are currently not hosting any frames and are thus eligible for reuse
// when a new renderer process is needed. Currently, only background renderer
// processes are considered for reuse.
BASE_FEATURE(kTrackEmptyRendererProcessesForReuse,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature is for a reverse Origin Trial, enabling SharedArrayBuffer for
// sites as they migrate towards requiring cross-origin isolation for these
// features.
// TODO(bbudge): Remove when the deprecation is complete.
// https://developer.chrome.com/origintrials/#/view_trial/303992974847508481
// https://crbug.com/1144104
BASE_FEATURE(kUnrestrictedSharedArrayBuffer, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
// If enabled, blink's context snapshot is used rather than the v8 snapshot.
BASE_FEATURE(kUseContextSnapshot, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables comparing browser and renderer's DidCommitProvisionalLoadParams in
// RenderFrameHostImpl::VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch.
BASE_FEATURE(kVerifyDidCommitParams, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a CHECK in NavigationRequest::ValidateCommitOrigin() to verify
// that the origin used at commit time matches the expected origin stored
// in the FrameNavigationEntry, whenever PageState is non-empty.
//
// This helps catch session history corruption or stale origin-related state
// being sent to the renderer, which could violate origin isolation and lead
// to security issues (see crbug.com/41492620).
//
// This feature is disabled by default while we diagnose on Canary only.
BASE_FEATURE(kValidateCommitOriginAtCommit, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables future V8 VM features
BASE_FEATURE(kV8VmFuture, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables V8 to use a set of experimental optimizations for Android Desktop.
// This feature flag is intended to control various performance-related
// tweaks.
//
// TODO(crbug.com/425860368): This feature may need to be updated or removed
// based on the evolution of V8's performance features for high-end devices.
BASE_FEATURE(kV8AndroidDesktopHighEndConfig, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables per PWA System Media Controls. Only supported on Windows and macOS.
BASE_FEATURE(kWebAppSystemMediaControls,
#if BUILDFLAG(IS_WIN)
             // Windows enabled since 124.
             base::FEATURE_ENABLED_BY_DEFAULT);
#elif BUILDFLAG(IS_MAC)
             // macOS enabled in 130. If a kill switch is needed, it should be
             // safe to only disable the failing platform (ie. macOS here).
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Enable WebAssembly baseline compilation (Liftoff).
BASE_FEATURE(kWebAssemblyBaseline, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
// When a Web application is video-capturing a tab, it can use the Element
// Capture API to restrict the resulting video.
// - If `kElementCaptureOfOtherTabs` is disabled, the Web application can only
// restrict self-capture tracks. (That is, restrictping is only possible when
// the application is capturing its own tab.)
// - If `kElementCaptureOfOtherTabs` is enabled, the Web application  can
// restrict video-captures of any tab (so long as that other tab collaborates by
// sending a RestrictionTarget).
BASE_FEATURE(kElementCaptureOfOtherTabs, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

// Enables WebAssembly Shared-Everything Threads.
BASE_FEATURE(kEnableExperimentalWebAssemblySharedEverything,
             "WebAssemblyExperimentalSharedEverything",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebAssembly lazy compilation (JIT on first call).
BASE_FEATURE(kWebAssemblyLazyCompilation, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly tiering (Liftoff -> TurboFan).
BASE_FEATURE(kWebAssemblyTiering, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly trap handler.
BASE_FEATURE(kWebAssemblyTrapHandler,
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) ||  \
      BUILDFLAG(IS_MAC)) &&                                                  \
     defined(ARCH_CPU_X86_64)) ||                                            \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)) && \
     defined(ARCH_CPU_ARM64))
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether the Web Bluetooth API is enabled:
// https://webbluetoothcg.github.io/web-bluetooth/
BASE_FEATURE(kWebBluetooth, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Web Bluetooth should use the new permissions backend. The
// new permissions backend uses ObjectPermissionContextBase, which is used by
// other device APIs, such as WebUSB. When enabled,
// WebBluetoothWatchAdvertisements and WebBluetoothGetDevices blink features are
// also enabled.
BASE_FEATURE(kWebBluetoothNewPermissionsBackend,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls which backend is used to retrieve OTP on Android. When disabled
// we use User Consent API.
BASE_FEATURE(kWebOtpBackendAuto, base::FEATURE_DISABLED_BY_DEFAULT);

// The JavaScript API for payments on the web.
BASE_FEATURE(kWebPayments, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables code caching for scripts used on WebUI pages.
BASE_FEATURE(kWebUICodeCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables build-time generated resource-bundled code caches for WebUI pages.
// See crbug.com/375509504 for details.
BASE_FEATURE(kWebUIBundledCodeCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables populating the WebUI URL to code cache resource map.
const base::FeatureParam<bool> kWebUIBundledCodeCacheGenerateResourceMap{
    &kWebUIBundledCodeCache, "WebUIBundledCodeCacheGenerateResourceMap", true};

#if !BUILDFLAG(IS_ANDROID)
// Reports WebUI Javascript errors to the crash server on all desktop platforms.
// Previously, this was only supported on ChromeOS and Linux.
// Intentionally enabled by default and will be used as a kill switch in case
// of regressions.
BASE_FEATURE(kWebUIJSErrorReportingExtended, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
BASE_FEATURE(kWebUsb, "WebUSB", base::FEATURE_ENABLED_BY_DEFAULT);

// Apply `PrefetchPriority::kHighest` for Webview Prefetch API.
BASE_FEATURE(kWebViewPrefetchHighestPrefetchPriority,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set an additional `PrefetchScheduler` burst limit for
// `PrefetchPriority::kHighest` prefetches.
constexpr base::FeatureParam<size_t>
    kWebViewPrefetchHighestPrefetchPriorityBurstLimit{
        &kWebViewPrefetchHighestPrefetchPriority,
        "WebViewPrefetchHighestPrefetchPriorityBurstLimit", 1};

// Controls whether the WebXR Device API is enabled.
BASE_FEATURE(kWebXr, "WebXR", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the navigator.permissions API.
// Used for launch in WebView, but exposed in content to map to runtime-enabled
// feature.
BASE_FEATURE(kWebPermissionsApi, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAccessibilityDeprecateJavaNodeCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, will optimize scrolling.
const base::FeatureParam<bool>
    kAccessibilityDeprecateJavaNodeCacheOptimizeScroll{
        &kAccessibilityDeprecateJavaNodeCache, "optimize_scroll", false};

// When enabled, will no longer cache java side AccessibilityNodeInfo objects.
const base::FeatureParam<bool> kAccessibilityDeprecateJavaNodeCacheDisableCache{
    &kAccessibilityDeprecateJavaNodeCache, "disable_cache", false};

// When enabled, TYPE_ANNOUNCE events will no longer be sent for live regions in
// the web contents.
BASE_FEATURE(kAccessibilityDeprecateTypeAnnounce,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, WINDOW_CONTENT_CHANGED events will be sent for each
// LIVE_REGION_NODE_CHANGED rather than TYPE_ANNOUNCEMENT.
// kAccessibilityDeprecateTypeAnnounce also encompasses ariaNotify, whereas this
// flag does not. This flag focuses solely on the LIVE_REGION_NODE_CHANGED
// generated events.
BASE_FEATURE(kAccessibilityImproveLiveRegionAnnounce,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, the accessibility tree will be requested to
// layout based on the actions that are performed on the renderer side. In
// particular this will be used to determine whether or not a node is clickable
// or not.
BASE_FEATURE(kAccessibilityRequestLayoutBasedActions,
             "AccessibilityRequestLayoutBasedActions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the second iteration of AccessibilityPageZoom, which continues
// the work completed in the first experiment and the subsequent fast-follow.
// This version of the experiment explores enabling OS-level adjustments.
BASE_FEATURE(kAccessibilityPageZoomV2, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables populating the supplemental description information via the
// Android supplemental description API.
BASE_FEATURE(kAccessibilityPopulateSupplementalDescriptionApi,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the reactive synchronization of accessibility and keyboard focus,
// relying on new Android framework behavior.
BASE_FEATURE(kAccessibilitySequentialFocus, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, set selectable on all nodes with text, and support
// ACTION_SET_SELECTION.
BASE_FEATURE(kAccessibilitySetSelectableOnAllNodesWithText,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of a unified code path for AXTree snapshots.
BASE_FEATURE(kAccessibilityUnifiedSnapshots, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables posting registering, unregistering the broadcast receiver to the
// background thread.
BASE_FEATURE(kAccessibilityManageBroadcastReceiverOnBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the ability to specify a platform-specific zoom scaling that will
// apply transparently to all pages.
BASE_FEATURE(kAndroidDesktopZoomScaling, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAndroidDesktopZoomScalingFactor{
    &kAndroidDesktopZoomScaling, "desktop-zoom-scaling-factor", 100};
const base::FeatureParam<int> kAndroidMonitorZoomScalingFactor{
    &kAndroidDesktopZoomScaling, "monitor-zoom-scaling-factor", 100};

// Enable open PDF inline on Android.
BASE_FEATURE(kAndroidOpenPdfInline, base::FEATURE_ENABLED_BY_DEFAULT);

// A feature to enable launch handler and file handler api for Chrome on Android
BASE_FEATURE(kAndroidWebAppLaunchHandler, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows the use of "Smart Zoom", an alternative form of page zoom, and
// enables the associated UI.
BASE_FEATURE(kSmartZoom, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the importance for subframes in WebContents.
BASE_FEATURE(kSubframeImportance, base::FEATURE_DISABLED_BY_DEFAULT);

// Skips clearing objects on main document ready. Only has an impact
// when gin java bridge is enabled.
BASE_FEATURE(kGinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reduce the priority of GPU process when in background so it is more likely
// to be killed first if the OS needs more memory.
BASE_FEATURE(kReduceGpuPriorityOnBackground, base::FEATURE_DISABLED_BY_DEFAULT);

// Screen Capture API support for Android.
// This should not be enabled unless ENABLE_SCREEN_CAPTURE is on, otherwise
// it won't work.
BASE_FEATURE(kUserMediaScreenCapturing, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables backgrounding hidden renderers on Mac.
BASE_FEATURE(kMacAllowBackgroundingRenderProcesses,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes how Chrome responds to accessibility activation signals on macOS
// Sonoma, to avoid unnecessary changes to the screen reader state.
BASE_FEATURE(kSonomaAccessibilityActivationRefinements,
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
// Disables WebAuthn on Android Auto. Default enabled in M137, remove in or
// after M140.
BASE_FEATURE(kWebauthnDisabledOnAuto,
             "WebAuthenticationDisabledOnAuto",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables Exclusive Access Manager on Android platform
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableExclusiveAccessManager, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kKeyboardLockApiOnAndroid, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Sets IO threads to kInteractive all the time.
BASE_FEATURE(kIOThreadInteractiveThreadType, base::FEATURE_DISABLED_BY_DEFAULT);

// Boosts IO threads and Browser main to kInteractive during input scenarios.
BASE_FEATURE(kBoostThreadsPriorityDuringInputScenario,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default amount of days after which the global navigation capturing IPH
// guardrails are cleared from storage.
const base::FeatureParam<int> kNavigationCapturingIPHGuardrailStorageDuration{
    &kPwaNavigationCapturing, "link_capturing_guardrail_storage_duration", 30};

BASE_FEATURE(kPwaNavigationCapturing, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<CapturingState>::Option kNavigationCapturingParams[] =
    {{CapturingState::kDefaultOn, "on_by_default"},
     {CapturingState::kDefaultOff, "off_by_default"},
     {CapturingState::kReimplDefaultOn, "reimpl_default_on"},
     {CapturingState::kReimplDefaultOff, "reimpl_default_off"},
     {CapturingState::kReimplOnViaClientMode, "reimpl_on_via_client_mode"}};

const base::FeatureParam<CapturingState> kNavigationCapturingDefaultState{
    &kPwaNavigationCapturing, "link_capturing_state",
#if BUILDFLAG(IS_CHROMEOS)
    CapturingState::kReimplDefaultOff,
#else
    CapturingState::kReimplDefaultOn,
#endif
    &kNavigationCapturingParams};

const base::FeatureParam<std::string> kForcedOffCapturingAppsOnFirstNavigation{
    &kPwaNavigationCapturing, "initial_nav_forced_off_apps", ""};

const base::FeatureParam<std::string> kForcedOffCapturingAppsUserSetting{
    &kPwaNavigationCapturing, "user_settings_forced_off_apps", ""};

// Enables overriding the default subframe process shutdown delay via the
// "delay_seconds" field trial parameter. This allows for experimentation with
// different timeout values for keeping subframe processes alive for potential
// reuse.
BASE_FEATURE(kSubframeProcessShutdownDelay, base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies the custom shutdown delay in seconds to use when the
// kSubframeProcessShutdownDelay feature is enabled.
// Default value, matching the original kSubframeProcessShutdownDelay.
const base::FeatureParam<int> kSubframeProcessShutdownDelaySeconds{
    &kSubframeProcessShutdownDelay, "delay_seconds", 2};

namespace {
enum class VideoCaptureServiceConfiguration {
  kEnabledForOutOfProcess,
  kEnabledForBrowserProcess,
  kDisabled
};

VideoCaptureServiceConfiguration GetVideoCaptureServiceConfiguration() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
#else
  return base::FeatureList::IsEnabled(
             features::kRunVideoCaptureServiceInBrowserProcess)
             ? VideoCaptureServiceConfiguration::kEnabledForBrowserProcess
             : VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
#endif
}

}  // namespace

bool IsVideoCaptureServiceEnabledForOutOfProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForOutOfProcess;
}

bool IsVideoCaptureServiceEnabledForBrowserProcess() {
  return GetVideoCaptureServiceConfiguration() ==
         VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
}

bool IsPushSubscriptionChangeEventEnabled() {
  return base::FeatureList::IsEnabled(
             features::kPushSubscriptionChangeEventOnInvalidation) ||
         base::FeatureList::IsEnabled(
             features::kPushSubscriptionChangeEventOnResubscribe);
}

}  // namespace features
