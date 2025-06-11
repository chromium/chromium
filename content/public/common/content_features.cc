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

// Kill switch to guard additional security checks performed by the browser
// process on opaque origins, such as when verifying source origins for
// postMessage. See https://crbug.com/40109437.
BASE_FEATURE(kAdditionalOpaqueOriginEnforcements,
             "AdditionalOpaqueOriginEnforcements",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Fallback to next named service slot if launching a privileged service process
// hangs. In practice, this means if GPU launch hanges, then retry it once.
BASE_FEATURE(kAndroidFallbackToNextSlot,
             "AndroidFallbackToNextSlot",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Warm up a spare renderer after each navigation on Android.
BASE_FEATURE(kAndroidWarmUpSpareRendererWithTimeout,
             "AndroidWarmUpSpareRendererWithTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Create the spare renderer in DidStopLoading rather than in
// SpareRenderProcessHostManager::PrepareForFutureRequests.
const base::FeatureParam<std::string> kAndroidSpareRendererCreationTiming{
    &kAndroidWarmUpSpareRendererWithTimeout, "spare_renderer_creation_timing",
    kAndroidSpareRendererCreationAfterLoading};

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

// Whether to allow attaching an inner WebContents not owned by the outer
// WebContents. This is for prototyping purposes and should not be enabled in
// production.
BASE_FEATURE(kAttachUnownedInnerWebContents,
    "AttachUnownedInnerWebContents",
    base::FEATURE_DISABLED_BY_DEFAULT);

// Launches the audio service on the browser startup.
BASE_FEATURE(kAudioServiceLaunchOnStartup,
             "AudioServiceLaunchOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Runs the audio service in a separate process.
BASE_FEATURE(kAudioServiceOutOfProcess,
             "AudioServiceOutOfProcess",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the audio-service sandbox. This feature has an effect only when the
// kAudioServiceOutOfProcess feature is enabled.
BASE_FEATURE(kAudioServiceSandbox,
             "AudioServiceSandbox",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for Background Fetch.
BASE_FEATURE(kBackgroundFetch,
             "BackgroundFetch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable using the BackForwardCache.
BASE_FEATURE(kBackForwardCache,
             "BackForwardCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Set a time limit for the page to enter the cache. Disabling this prevents
// flakes during testing.
BASE_FEATURE(kBackForwardCacheEntryTimeout,
             "BackForwardCacheEntryTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// BackForwardCache is disabled on low memory devices. The threshold is defined
// via a field trial param: "memory_threshold_for_back_forward_cache_in_mb"
// It is compared against base::SysInfo::AmountOfPhysicalMemoryMB().

// "BackForwardCacheMemoryControls" is checked before "BackForwardCache". It
// means the low memory devices will activate neither the control group nor the
// experimental group of the BackForwardCache field trial.

// BackForwardCacheMemoryControls is enabled only on Android to disable
// BackForwardCache for lower memory devices due to memory limitations.
BASE_FEATURE(kBackForwardCacheMemoryControls,
             "BackForwardCacheMemoryControls",

#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, makes battery saver request heavy align wake ups.
BASE_FEATURE(kBatterySaverModeAlignWakeUps,
             "BatterySaverModeAlignWakeUps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, private network requests initiated from
// non-secure contexts in the `public` address space  are blocked.
//
// See also:
//  - https://wicg.github.io/private-network-access/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequestsFromPrivate
//  - kBlockInsecurePrivateNetworkRequestsFromUnknown
BASE_FEATURE(kBlockInsecurePrivateNetworkRequests,
             "BlockInsecurePrivateNetworkRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this feature is enabled, requests to localhost initiated from non-secure
// contexts in the `private` IP address space are blocked.
//
// See also:
//  - https://wicg.github.io/private-network-access/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequests
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsFromPrivate,
             "BlockInsecurePrivateNetworkRequestsFromPrivate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of the PrivateNetworkAccessNonSecureContextsAllowed deprecation
// trial. This is a necessary yet insufficient condition: documents that wish to
// make use of the trial must additionally serve a valid origin trial token.
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
             "BlockInsecurePrivateNetworkRequestsDeprecationTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Broker file operations on disk cache in the Network Service.
// This is no-op if the network service is hosted in the browser process.
BASE_FEATURE(kBrokerFileOperationsOnDiskCacheInNetworkService,
             "BrokerFileOperationsOnDiskCacheInNetworkService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows pages with cache-control:no-store to enter the back/forward cache.
// Feature params can specify whether pages with cache-control:no-store can be
// restored if cookies change / if HTTPOnly cookies change.
// TODO(crbug.com/40189625): Remove this feature and clean up.
BASE_FEATURE(kCacheControlNoStoreEnterBackForwardCache,
             "CacheControlNoStoreEnterBackForwardCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This killswitch is distinct from the OT.
// It allows us to remotely disable the feature, and get it to stop working even
// on sites that are in possession of a valid token. When that happens, all API
// calls gated by the killswitch will fail graceully.
BASE_FEATURE(kCapturedSurfaceControlKillswitch,
             "CapturedSurfaceControlKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Clear the window.name property for the top-level cross-site navigations that
// swap BrowsingContextGroups(BrowsingInstances).
BASE_FEATURE(kClearCrossSiteCrossBrowsingContextGroupWindowName,
             "ClearCrossSiteCrossBrowsingContextGroupWindowName",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCompositeBGColorAnimation,
             "CompositeBGColorAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gate access to cookie deprecation API which allows developers to opt in
// server side testing without cookies.
// (See https://developer.chrome.com/en/docs/privacy-sandbox/chrome-testing)
BASE_FEATURE(kCookieDeprecationFacilitatedTesting,
             "CookieDeprecationFacilitatedTesting",
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
BASE_FEATURE(kDeferSpeculativeRFHCreation,
             "DeferSpeculativeRFHCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// When a device bound session
// (https://github.com/w3c/webappsec-dbsc/blob/main/README.md) is
// terminated, evict pages with cache-control:no-store from the
// BFCache. Note that if `kCacheControlNoStoreEnterBackForwardCache` is
// disabled, no such pages will be in the cache.
BASE_FEATURE(kDeviceBoundSessionTerminationEvictBackForwardCache,
             "DeviceBoundSessionTerminationEvictBackForwardCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the DevTools Privacy UI is displayed.
BASE_FEATURE(kDevToolsPrivacyUI,
             "DevToolsPrivacyUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Digital Goods API is enabled.
// https://github.com/WICG/digital-goods/
BASE_FEATURE(kDigitalGoodsApi,
             "DigitalGoodsApi",
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
        {content::BtmTriggeringAction::kStorage, "storage"},
        {content::BtmTriggeringAction::kBounce, "bounce"},
        {content::BtmTriggeringAction::kStatefulBounce, "stateful_bounce"}};

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
BASE_FEATURE(kBtmDualUse, "BtmDualUse", base::FEATURE_DISABLED_BY_DEFAULT);

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
             "WebContentsDiscard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable drawing under System Bars within DisplayCutout.
BASE_FEATURE(kDrawCutoutEdgeToEdge,
             "DrawCutoutEdgeToEdge",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable establishing the GPU channel early in renderer startup.
BASE_FEATURE(kEarlyEstablishGpuChannel,
             "EarlyEstablishGpuChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables canvas 2d methods BeginLayer and EndLayer.
BASE_FEATURE(kEnableCanvas2DLayers,
             "EnableCanvas2DLayers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome-untrusted:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeUntrusted,
             "EnableServiceWorkersForChromeUntrusted",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeScheme,
             "EnableServiceWorkersForChromeScheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Ensures the renderer is not dead when getting the process host for a site
// instance.
BASE_FEATURE(kEnsureExistingRendererAlive,
             "EnsureExistingRendererAlive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables JavaScript API to intermediate federated identity requests.
// Note that actual exposure of the FedCM API to web content is controlled
// by the flag in RuntimeEnabledFeatures on the blink side. See also
// the use of kSetOnlyIfOverridden in content/child/runtime_features.cc.
// We enable it here by default to support use in origin trials.
BASE_FEATURE(kFedCm, "FedCm", base::FEATURE_ENABLED_BY_DEFAULT);

// Support usernames and phone numbers to identify users, instead of
// (or in addition to) names and emails.
BASE_FEATURE(kFedCmAlternativeIdentifiers,
             "FedCmAlternativeIdentifiers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables RPs to enhance autofill with federated accounts fetched by the FedCM
// API.
BASE_FEATURE(kFedCmAutofill,
             "FedCmAutofill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables cooldown on ignore in FedCM API.
BASE_FEATURE(kFedCmCooldownOnIgnore,
             "FedCmCooldownOnIgnore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM Delegation API.
BASE_FEATURE(kFedCmDelegation,
             "FedCmDelegation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM IdP Registration API.
BASE_FEATURE(kFedCmIdPRegistration,
             "FedCmIdPregistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// For cross-site iframes, sends the top-level origin to the IDP and parses
// an optional returned boolean indicating whether it is part of the same
// client to allow for UI decisions based on the boolean.
BASE_FEATURE(kFedCmIframeOrigin,
             "FedCmIframeOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with metrics endpoint at the same time.
BASE_FEATURE(kFedCmMetricsEndpoint,
             "FedCmMetricsEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with multiple identity providers at the same
// time.
BASE_FEATURE(kFedCmMultipleIdentityProviders,
             "FedCmMultipleIdentityProviders",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing filtered out accounts in FedCM UI after the user attempts to
// login to an account. These accounts are shown greyed out.
BASE_FEATURE(kFedCmShowFilteredAccounts,
             "FedCmShowFilteredAccounts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables bypassing the well-known file enforcement.
BASE_FEATURE(kFedCmWithoutWellKnownEnforcement,
             "FedCmWithoutWellKnownEnforcement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Lightweight FedCM Mode
BASE_FEATURE(kFedCmLightweightMode,
             "FedCmLightweightMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables browser-side focus verification when crossing fenced boundaries.
BASE_FEATURE(kFencedFramesEnforceFocus,
             "FencedFramesEnforceFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is a kill switch for focusing the RenderWidgetHostViewAndroid on
// ActionDown on every touch sequence if not focused already, please see
// crbug.com/381820236. The root view, RWHVA, is always focused in Chrome,
// however this might not be true on WebView, see crbug.com/378779896 for more
// details.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFocusRenderWidgetHostViewAndroidOnActionDown,
             "FocusRenderWidgetHostViewAndroidOnActionDown",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether a memory pressure signal in a renderer should be forwarded to Blink
// isolates. Forwarding the signal triggers a GC (critical) or starts
// incremental marking (moderate), see `v8::Heap::CheckMemoryPressure`.
BASE_FEATURE(kForwardMemoryPressureToBlinkIsolates,
             "ForwardMemoryPressureToBlinkIsolates",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Digital Credential API.
BASE_FEATURE(kWebIdentityDigitalCredentials,
             "WebIdentityDigitalCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Digital Credentials Creation API.
BASE_FEATURE(kWebIdentityDigitalCredentialsCreation,
             "WebIdentityDigitalCredentialsCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables scrollers inside Blink to store scroll offsets in fractional
// floating-point numbers rather than truncating to integers.
BASE_FEATURE(kFractionalScrollOffsets,
             "FractionalScrollOffsets",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Puts network quality estimate related Web APIs in the holdback mode. When the
// holdback is enabled the related Web APIs return network quality estimate
// set by the experiment (regardless of the actual quality).
BASE_FEATURE(kNetworkQualityEstimatorWebHoldback,
             "NetworkQualityEstimatorWebHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether GuestViews (see components/guest_view/README.md) are implemented
// using MPArch inner pages. See https://crbug.com/40202416
BASE_FEATURE(kGuestViewMPArch,
             "GuestViewMPArch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// See crbug.com/359623664
BASE_FEATURE(kIdbPrioritizeForegroundClients,
             "IdbPrioritizeForegroundClients",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we ignore duplicate navigations or not, in favor of
// preserving the already ongoing navigation.
BASE_FEATURE(kIgnoreDuplicateNavs,
             "IgnoreDuplicateNavs",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDuplicateNavThreshold,
                   &kIgnoreDuplicateNavs,
                   "duplicate_nav_threshold",
                   base::Milliseconds(2000));

// Kill switch for the GetInstalledRelatedApps API.
BASE_FEATURE(kInstalledApp, "InstalledApp", base::FEATURE_ENABLED_BY_DEFAULT);

// Allow Windows specific implementation for the GetInstalledRelatedApps API.
BASE_FEATURE(kInstalledAppProvider,
             "InstalledAppProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable support for isolated web apps. This will guard features like serving
// isolated web apps via the isolated-app:// scheme, and other advanced isolated
// app functionality. See https://github.com/reillyeon/isolated-web-apps for a
// general overview.
// Please don't use this feature flag directly to guard the IWA code.  Use
// IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled() in the browser process or
// check kEnableIsolatedWebAppsInRenderer command line flag in the renderer
// process.
BASE_FEATURE(kIsolatedWebApps,
             "IsolatedWebApps",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables a new Automatic Fullscreen content setting that lets allowlisted
// origins use the HTML Fullscreen API without transient activation.
// https://chromestatus.com/feature/6218822004768768
BASE_FEATURE(kAutomaticFullscreenContentSetting,
             "AutomaticFullscreenContentSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables process isolation of fenced content (content inside fenced frames)
// from non-fenced content. See
// https://github.com/WICG/fenced-frame/blob/master/explainer/process_isolation.md
// for rationale and more details.
BASE_FEATURE(kIsolateFencedFrames,
             "IsolateFencedFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Alternative to switches::kIsolateOrigins, for turning on origin isolation.
// List of origins to isolate has to be specified via
// kIsolateOriginsFieldTrialParamName.
BASE_FEATURE(kIsolateOrigins,
             "IsolateOrigins",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kIsolateOriginsFieldTrialParamName[] = "OriginsList";

// Enable lazy initialization of the media controls.
BASE_FEATURE(kLazyInitializeMediaControls,
             "LazyInitializeMediaControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogJsConsoleMessages,
             "LogJsConsoleMessages",
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
             "MBIMode",
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

// Controls the configurablity of the navigation confidence noise level.
// If the feature is not enabled, then the epsilon value will be 1.1.
BASE_FEATURE(kNavigationConfidenceEpsilon,
             "NavigationConfidenceEpsilon",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The epsilon value returned if `kNavigationConfidenceNoise` is enabled.
const base::FeatureParam<double> kNavigationConfidenceEpsilonValue{
    &kNavigationConfidenceEpsilon, "navigation-confidence-epsilon-value", 1.1};

// When NavigationNetworkResponseQueue is enabled, the browser will schedule
// some tasks related to navigation network responses in a kHigh priority
// queue.
BASE_FEATURE(kNavigationNetworkResponseQueue,
             "NavigationNetworkResponseQueue",
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
BASE_FEATURE(kNoStatePrefetchHoldback,
             "NoStatePrefetchHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the Origin-Agent-Cluster header. Tracking bug
// https://crbug.com/1042415; flag removal bug (for when this is fully launched)
// https://crbug.com/1148057.
//
// The name is "OriginIsolationHeader" because that was the old name when the
// feature was under development.
BASE_FEATURE(kOriginIsolationHeader,
             "OriginIsolationHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// History navigation in response to horizontal overscroll (aka gesture-nav).
BASE_FEATURE(kOverscrollHistoryNavigation,
             "OverscrollHistoryNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Partitioned Popins must have a Popin-Policy in their top-frame HTTP Response
// that permits the opener origin. This feature disables that check for purposes
// of testing only, this must never be enabled by default in any context.
// See https://explainers-by-googlers.github.io/partitioned-popins/
BASE_FEATURE(kPartitionedPopinsHeaderPolicyBypass,
             "PartitionedPopinsHeaderPolicyBypass",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables additional ChildProcessSecurityPolicy enforcements for PDF renderer
// processes, including blocking storage and cookie access for them.
//
// TODO(https://crbug.com/40205612): Remove this kill switch once the PDF
// enforcements are verified not to cause problems.
BASE_FEATURE(kPdfEnforcements,
             "PdfEnforcements",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether web apps can run periodic tasks upon network connectivity.
BASE_FEATURE(kPeriodicBackgroundSync,
             "PeriodicBackgroundSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use code paths for prefetch/prerender integration.
// See also `kPrerender2FallbackPrefetchSpecRules`.
BASE_FEATURE(kPrefetchPrerenderIntegration,
             "PrefetchPrerenderIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, browser-initiated prefetch is allowed.
// Please see crbug.com/40946257 for more details.
BASE_FEATURE(kPrefetchBrowserInitiatedTriggers,
             "PrefetchBrowserInitiatedTriggers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This was used for enabling a new limit and scheduler for prerender triggers
// (crbug.com/40275452). Now the new implementation is used by default and this
// flag is just for injecting parameters through field trials.
BASE_FEATURE(kPrerender2NewLimitAndScheduler,
             "Prerender2NewLimitAndScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables exposure of ads APIs in the renderer: Attribution Reporting,
// FLEDGE, Topics, along with a number of other features actively in development
// within these APIs.
BASE_FEATURE(kPrivacySandboxAdsAPIsOverride,
             "PrivacySandboxAdsAPIsOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Private Network Access checks for all types of web workers.
//
// This affects initial worker script fetches, fetches initiated by workers
// themselves, and service worker update fetches.
//
// The exact checks run are the same as for other document subresources, and
// depend on the state of other Private Network Access feature flags:
//
//  - `kBlockInsecurePrivateNetworkRequests`
//  - `kPrivateNetworkAccessSendPreflights`
//  - `kPrivateNetworkAccessRespectPreflightResults`
//
BASE_FEATURE(kPrivateNetworkAccessForWorkers,
             "PrivateNetworkAccessForWorkers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Private Network Access checks in warning mode for all types of web
// workers.
//
// Does nothing if `kPrivateNetworkAccessForWorkers` is disabled.
//
// If both this and `kPrivateNetworkAccessForWorkers` are enabled, then PNA
// preflight requests for workers are not required to succeed. If one fails, a
// warning is simply displayed in DevTools.
BASE_FEATURE(kPrivateNetworkAccessForWorkersWarningOnly,
             "PrivateNetworkAccessForWorkersWarningOnly",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Private Network Access checks for navigations.
//
// The exact checks run are the same as for document subresources, and depend on
// the state of other Private Network Access feature flags:
//  - `kBlockInsecurePrivateNetworkRequests`
//  - `kPrivateNetworkAccessSendPreflights`
//  - `kPrivateNetworkAccessRespectPreflightResults`
BASE_FEATURE(kPrivateNetworkAccessForNavigations,
             "PrivateNetworkAccessForNavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Private Network Access checks in warning mode for navigations.
//
// Does nothing if `kPrivateNetworkAccessForNavigations` is disabled.
//
// If both this and `kPrivateNetworkAccessForNavigations` are enabled, then PNA
// preflight requests for navigations are not required to succeed. If
// one fails, a warning is simply displayed in DevTools.
BASE_FEATURE(kPrivateNetworkAccessForNavigationsWarningOnly,
             "PrivateNetworkAccessForNavigationsWarningOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Requires that CORS preflight requests succeed before sending private network
// requests. This flag implies `kPrivateNetworkAccessSendPreflights`.
// See: https://wicg.github.io/private-network-access/#cors-preflight
BASE_FEATURE(kPrivateNetworkAccessRespectPreflightResults,
             "PrivateNetworkAccessRespectPreflightResults",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending CORS preflight requests ahead of private network requests.
// See: https://wicg.github.io/private-network-access/#cors-preflight
BASE_FEATURE(kPrivateNetworkAccessSendPreflights,
             "PrivateNetworkAccessSendPreflights",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables origin-keyed processes by default, unless origins opt out using
// Origin-Agent-Cluster: ?0. This feature only takes effect if the Blink feature
// OriginAgentClusterDefaultEnable is enabled, since origin-keyed processes
// require origin-agent-clusters.
BASE_FEATURE(kOriginKeyedProcessesByDefault,
             "OriginKeyedProcessesByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Fires the `pushsubscriptionchange` event defined here:
// https://w3c.github.io/push-api/#the-pushsubscriptionchange-event
// for subscription refreshes, revoked permissions or subscription losses
BASE_FEATURE(kPushSubscriptionChangeEventOnInvalidation,
             "PushSubscriptionChangeEventOnInvalidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Fires the `pushsubscriptionchange` event defined here:
// https://w3c.github.io/push-api/#the-pushsubscriptionchange-event
// upon manual resubscription to previously unsubscribed notifications.
BASE_FEATURE(kPushSubscriptionChangeEventOnResubscribe,
             "PushSubscriptionChangeEventOnResubscribe",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, queues navigations instead of cancelling the previous
// navigation if the previous navigation is already waiting for commit.
// See https://crbug.com/838348 and https://crbug.com/1220337.
BASE_FEATURE(kQueueNavigationsWhileWaitingForCommit,
             "QueueNavigationsWhileWaitingForCommit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, sends SubresourceResponseStarted IPC only when the user has
// allowed any HTTPS-related warning exceptions. From field data, ~100% of
// subresource notifications are not required, since allowing certificate
// exceptions by users is a rare event. Hence, if user has never allowed any
// certificate or HTTP exceptions, notifications are not sent to the browser.
// Once we start sending these messages, we keep sending them until all
// exceptions are revoked and browser restart occurs.
BASE_FEATURE(kReduceSubresourceResponseStartedIPC,
             "ReduceSubresourceResponseStartedIPC",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// When a Web application is video-capturing a tab, it can use the Region
// Capture API to crop the resulting video.
// - If `kRegionCaptureOfOtherTabs` is disabled, the Web application can only
// crop self-capture tracks. (That is, cropping is only possible when the
// application is capturing its own tab.)
// - If `kRegionCaptureOfOtherTabs` is enabled, the Web application  can crop
// video-captures of any tab (so long as that other tab collaborates by sending
// a CropTarget).
BASE_FEATURE(kRegionCaptureOfOtherTabs,
             "RegionCaptureOfOtherTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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
BASE_FEATURE(kRenderDocument,
             "RenderDocument",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Restrict the maximum number of concurrent ThreadPool tasks when a renderer is
// low priority.
BASE_FEATURE(kRestrictThreadPoolInBackground,
             "RestrictThreadPoolInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set a tri-state priority on v8 isolates reflecting the renderer priority.
BASE_FEATURE(kSetIsolatesPriority,
             "SetIsolatesPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, sends the spare renderer information when setting the
// priority of renderers. Currently only Android handles the spare renderer
// information in priority.
// The target priority of a spare renderer in Android is decided by the feature
// parameters in ContentFeatureList.java.
BASE_FEATURE(kSpareRendererProcessPriority,
             "SpareRendererProcessPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reuse compositor instances with RenderDocument
BASE_FEATURE(kRenderDocumentCompositorReuse,
             "RenderDocumentCompositorReuse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables retrying to obtain list of available cameras after restarting the
// video capture service if a previous attempt failed, which could be caused
// by a service crash.
BASE_FEATURE(kRetryGetVideoCaptureDeviceInfos,
             "RetryGetVideoCaptureDeviceInfos",
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
             "ProcessPerSiteUpToMainFrameThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies the threshold for `kProcessPerSiteUpToMainFrameThreshold` feature.
constexpr base::FeatureParam<int> kProcessPerSiteMainFrameThreshold{
    &kProcessPerSiteUpToMainFrameThreshold, "ProcessPerSiteMainFrameThreshold",
    2};

// Allows process reuse for localhost and IP based hosts when
// `kProcessPerSiteUpToMainFrameThreshold` is enabled.
constexpr base::FeatureParam<bool> kProcessPerSiteMainFrameAllowIPAndLocalhost{
    &kProcessPerSiteUpToMainFrameThreshold,
    "ProcessPerSiteMainFrameAllowIPAndLocalhost", false};

// When `kProcessPerSiteUpToMainFrameThreshold` is enabled, allows process reuse
// even when DevTools was ever attached. This allows developers to test the
// process sharing mode, since DevTools normally disables it for the field
// trial participants.
constexpr base::FeatureParam<bool>
    kProcessPerSiteMainFrameAllowDevToolsAttached{
        &kProcessPerSiteUpToMainFrameThreshold,
        "ProcessPerSiteMainFrameAllowDevToolsAttached", false};

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
BASE_FEATURE(kServiceWorkerAutoPreload,
             "ServiceWorkerAutoPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

// crbug.com/374606637: When this is enabled, race-network-and-fetch-hander will
// prioritize the response processing for the network request over the
// processing for the fetch handler.
BASE_FEATURE(
    kServiceWorkerStaticRouterRaceNetworkRequestPerformanceImprovement,
    "ServiceWorkerStaticRouterRaceNetworkRequestPerformanceImprovement",
    base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Run video capture service in the Browser process as opposed to a dedicated
// utility process.
BASE_FEATURE(kRunVideoCaptureServiceInBrowserProcess,
             "RunVideoCaptureServiceInBrowserProcess",
             base::FEATURE_DISABLED_BY_DEFAULT
);
#endif

// Update scheduler settings using resourced on ChromeOS.
BASE_FEATURE(kSchedQoSOnResourcedForChrome,
             "SchedQoSOnResourcedForChrome",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             "SecurePaymentConfirmationDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Service worker based payment apps as defined by w3c here:
// https://w3c.github.io/webpayments-payment-apps-api/
// TODO(rouslan): Remove this.
BASE_FEATURE(kServiceWorkerPaymentApps,
             "ServiceWorkerPaymentApps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, UI thread tasks can check ServiceWorker registration information
// from the thread pool without waiting for running the receiving task. Please
// see crbug.com/421530699 for more details.
BASE_FEATURE(kServiceWorkerBackgroundUpdateForRegisteredStorageKeys,
             "ServiceWorkerBackgroundUpdateForRegisteredStorageKeys",
             base::FEATURE_DISABLED_BY_DEFAULT);

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
// This feature is also enabled independently of this flag for cross-origin
// isolated renderers.
BASE_FEATURE(kSharedArrayBuffer,
             "SharedArrayBuffer",
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
BASE_FEATURE(kUserMediaCaptureOnFocus,
             "UserMediaCaptureOnFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to enable using the update token in the manifest or icon url
// changes to detect app updates. When this is enabled, automatic icon
// downloading is disabled.
BASE_FEATURE(kWebAppEnableUpdateTokenParsing,
             "WebAppEnableUpdateTokenParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is intended as a kill switch for the WebOTP Service feature. To enable
// this feature, the experimental web platform features flag should be set.
BASE_FEATURE(kWebOTP, "WebOTP", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the web lockscreen API implementation
// (https://github.com/WICG/lock-screen) in Chrome.
BASE_FEATURE(kWebLockScreenApi,
             "WebLockScreenApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, puts subframe data: URLs in a separate SiteInstance in the same
// SiteInstanceGroup as the initiator.
BASE_FEATURE(kSiteInstanceGroupsForDataUrls,
             "SiteInstanceGroupsForDataUrls",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, puts non-isolated sites in separate SiteInstances in a default
// SiteInstanceGroup (per BrowsingInstance), rather than sharing a default
// SiteInstance.
BASE_FEATURE(kDefaultSiteInstanceGroups,
             "DefaultSiteInstanceGroups",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to isolate sites of documents that specify an eligible
// Cross-Origin-Opener-Policy header.  Note that this is only intended to be
// used on Android, which does not use strict site isolation. See
// https://crbug.com/1018656.
BASE_FEATURE(kSiteIsolationForCrossOriginOpenerPolicy,
             "SiteIsolationForCrossOriginOpenerPolicy",
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

// This feature controls whether the renderer should use FontDataManager to
// fetch fonts from the Browser's FontDataService. It is currently scoped to
// just Windows. See crbug.com/335680565.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kFontDataServiceAllWebContents,
             "FontDataServiceAllWebContents",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<FontDataServiceTypefaceType>::Option
    font_data_service_typeface[] = {
        {FontDataServiceTypefaceType::kDwrite, "DWrite"},
        {FontDataServiceTypefaceType::kFreetype, "Freetype"},
        {FontDataServiceTypefaceType::kFontations, "Fontations"}};
const base::FeatureParam<FontDataServiceTypefaceType>
    kFontDataServiceTypefaceType{&kFontDataServiceAllWebContents, "typeface",
                                 FontDataServiceTypefaceType::kDwrite,
                                 &font_data_service_typeface};
#endif  // BUILDFLAG(IS_WIN)

// When enabled, OOPIFs will not try to reuse compatible processes from
// unrelated tabs.
BASE_FEATURE(kDisableProcessReuse,
             "DisableProcessReuse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether SpareRenderProcessHostManager tries to always have a warm
// spare renderer process around for the most recently requested BrowserContext.
// This feature is only consulted in site-per-process mode.
BASE_FEATURE(kSpareRendererForSitePerProcess,
             "SpareRendererForSitePerProcess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether site isolation should use origins instead of scheme and
// eTLD+1.
BASE_FEATURE(kStrictOriginIsolation,
             "StrictOriginIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether subframe process reuse should be restricted according to
// resource usage policies. Namely, a process that is already consuming too
// much memory is not attempted to be reused.
BASE_FEATURE(kSubframeProcessReuseThresholds,
             "SubframeProcessReuseThresholds",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Specifies the memory threshold for the `kSubframeProcessReuseThresholds`
// feature, which only allows a process to be reused for another subframe if the
// process's memory footprint stays below this threshold. Similar to
// `kProcessPerSiteMainFrameTotalMemoryLimit`, and only provided as a separate
// knob so that it can be independently controlled in subframe and main frame
// process reuse experiments.
constexpr base::FeatureParam<double> kSubframeProcessReuseMemoryThreshold{
    &kSubframeProcessReuseThresholds, "SubframeProcessReuseMemoryThreshold",
    512 * 1024 * 1024u};

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
BASE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs,
             "SuppressDifferentOriginSubframeJSDialogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Dispatch touch events to "SyntheticGestureController" for events from
// Devtool Protocol Input.dispatchTouchEvent to simulate touch events close to
// real OS events.
BASE_FEATURE(kSyntheticPointerActions,
             "SyntheticPointerActions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature allows touch dragging and a context menu to occur
// simultaneously, with the assumption that the menu is non-modal.  Without this
// feature, a long-press touch gesture can start either a drag or a context-menu
// in Blink, not both (more precisely, a context menu is shown only if a drag
// cannot be started).
BASE_FEATURE(kTouchDragAndContextMenu,
             "TouchDragAndContextMenu",
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
             "TrackEmptyRendererProcessesForReuse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature is for a reverse Origin Trial, enabling SharedArrayBuffer for
// sites as they migrate towards requiring cross-origin isolation for these
// features.
// TODO(bbudge): Remove when the deprecation is complete.
// https://developer.chrome.com/origintrials/#/view_trial/303992974847508481
// https://crbug.com/1144104
BASE_FEATURE(kUnrestrictedSharedArrayBuffer,
             "UnrestrictedSharedArrayBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
// If enabled, blink's context snapshot is used rather than the v8 snapshot.
BASE_FEATURE(kUseContextSnapshot,
             "UseContextSnapshot",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables comparing browser and renderer's DidCommitProvisionalLoadParams in
// RenderFrameHostImpl::VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch.
BASE_FEATURE(kVerifyDidCommitParams,
             "VerifyDidCommitParams",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables future V8 VM features
BASE_FEATURE(kV8VmFuture, "V8VmFuture", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables per PWA System Media Controls. Only supported on Windows and macOS.
BASE_FEATURE(kWebAppSystemMediaControls,
             "WebAppSystemMediaControls",
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
BASE_FEATURE(kWebAssemblyBaseline,
             "WebAssemblyBaseline",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// When a Web application is video-capturing a tab, it can use the Element
// Capture API to restrict the resulting video.
// - If `kElementCaptureOfOtherTabs` is disabled, the Web application can only
// restrict self-capture tracks. (That is, restrictping is only possible when
// the application is capturing its own tab.)
// - If `kElementCaptureOfOtherTabs` is enabled, the Web application  can
// restrict video-captures of any tab (so long as that other tab collaborates by
// sending a RestrictionTarget).
BASE_FEATURE(kElementCaptureOfOtherTabs,
             "ElementCaptureOfOtherTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enable WebAssembly JSPI.
BASE_FEATURE(kEnableExperimentalWebAssemblyJSPI,
             "WebAssemblyExperimentalJSPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly lazy compilation (JIT on first call).
BASE_FEATURE(kWebAssemblyLazyCompilation,
             "WebAssemblyLazyCompilation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly tiering (Liftoff -> TurboFan).
BASE_FEATURE(kWebAssemblyTiering,
             "WebAssemblyTiering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly trap handler.
BASE_FEATURE(kWebAssemblyTrapHandler,
             "WebAssemblyTrapHandler",
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
BASE_FEATURE(kWebBluetooth, "WebBluetooth", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Web Bluetooth should use the new permissions backend. The
// new permissions backend uses ObjectPermissionContextBase, which is used by
// other device APIs, such as WebUSB. When enabled,
// WebBluetoothWatchAdvertisements and WebBluetoothGetDevices blink features are
// also enabled.
BASE_FEATURE(kWebBluetoothNewPermissionsBackend,
             "WebBluetoothNewPermissionsBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls which backend is used to retrieve OTP on Android. When disabled
// we use User Consent API.
BASE_FEATURE(kWebOtpBackendAuto,
             "WebOtpBackendAuto",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The JavaScript API for payments on the web.
BASE_FEATURE(kWebPayments, "WebPayments", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables code caching for scripts used on WebUI pages.
BASE_FEATURE(kWebUICodeCache,
             "WebUICodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables build-time generated resource-bundled code caches for WebUI pages.
// See crbug.com/375509504 for details.
BASE_FEATURE(kWebUIBundledCodeCache,
             "WebUIBundledCodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables populating the WebUI URL to code cache resource map.
const base::FeatureParam<bool> kWebUIBundledCodeCacheGenerateResourceMap{
    &kWebUIBundledCodeCache, "WebUIBundledCodeCacheGenerateResourceMap", true};

#if !BUILDFLAG(IS_ANDROID)
// Reports WebUI Javascript errors to the crash server on all desktop platforms.
// Previously, this was only supported on ChromeOS and Linux.
// Intentionally enabled by default and will be used as a kill switch in case
// of regressions.
BASE_FEATURE(kWebUIJSErrorReportingExtended,
            "WebUIJSErrorReportingExtended",
            base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
BASE_FEATURE(kWebUsb, "WebUSB", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the WebXR Device API is enabled.
BASE_FEATURE(kWebXr, "WebXR", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the navigator.permissions API.
// Used for launch in WebView, but exposed in content to map to runtime-enabled
// feature.
BASE_FEATURE(kWebPermissionsApi,
             "WebPermissionsApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, will no longer cache java side AccessibilityNodeInfo objects.
BASE_FEATURE(kAccessibilityDeprecateJavaNodeCache,
             "AccessibilityDeprecateJavaNodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, TYPE_ANNOUNCE events will no longer be sent for live regions in
// the web contents.
BASE_FEATURE(kAccessibilityDeprecateTypeAnnounce,
             "AccessibilityDeprecateTypeAnnounce",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, includes the ACTION_LONG_CLICK action to all relevant nodes in
// the web contents accessibility tree.
BASE_FEATURE(kAccessibilityIncludeLongClickAction,
             "AccessibilityIncludeLongClickAction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the second iteration of AccessibilityPageZoom, which continues
// the work completed in the first experiment and the subsequent fast-follow.
// This version of the experiment explores enabling OS-level adjustments.
BASE_FEATURE(kAccessibilityPageZoomV2,
             "AccessibilityPageZoomV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of a unified code path for AXTree snapshots.
BASE_FEATURE(kAccessibilityUnifiedSnapshots,
             "AccessibilityUnifiedSnapshots",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables posting registering, unregistering the broadcast receiver to the
// background thread.
BASE_FEATURE(kAccessibilityManageBroadcastReceiverOnBackground,
             "AccessibilityManageBroadcastReceiverOnBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable open PDF inline on Android.
BASE_FEATURE(kAndroidOpenPdfInline,
             "AndroidOpenPdfInline",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A feature to enable launch handler and file handler api for Chrome on Android
BASE_FEATURE(kAndroidWebAppLaunchHandler,
             "AndroidWebAppLaunchHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the use of "Smart Zoom", an alternative form of page zoom, and
// enables the associated UI.
BASE_FEATURE(kSmartZoom, "SmartZoom", base::FEATURE_DISABLED_BY_DEFAULT);

// Skips clearing objects on main document ready. Only has an impact
// when gin java bridge is enabled.
BASE_FEATURE(kGinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady,
             "GinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Rebind service binding when consecutive Context.updateServiceGroup() call is
// done. If this is disabled, it rebinds the service binding on each
// Context.updateServiceGroup() call.
BASE_FEATURE(kGroupRebindingForGroupImportance,
             "GroupRebindingForGroupImportance",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reduce the priority of GPU process when in background so it is more likely
// to be killed first if the OS needs more memory.
BASE_FEATURE(kReduceGpuPriorityOnBackground,
             "ReduceGpuPriorityOnBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Screen Capture API support for Android.
// This should not be enabled unless ENABLE_SCREEN_CAPTURE is on, otherwise
// it won't work.
BASE_FEATURE(kUserMediaScreenCapturing,
             "UserMediaScreenCapturing",
#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables backgrounding hidden renderers on Mac.
BASE_FEATURE(kMacAllowBackgroundingRenderProcesses,
             "MacAllowBackgroundingRenderProcesses",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a fix for a macOS IME Live Conversion issue. crbug.com/40226470 and
// crbug.com/40060200
BASE_FEATURE(kMacImeLiveConversionFix,
             "MacImeLiveConversionFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes how Chrome responds to accessibility activation signals on macOS
// Sonoma, to avoid unnecessary changes to the screen reader state.
BASE_FEATURE(kSonomaAccessibilityActivationRefinements,
             "SonomaAccessibilityActivationRefinements",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
// Disables WebAuthn on Android Auto. Default enabled in M137, remove in or
// after M140.
BASE_FEATURE(kWebauthnDisabledOnAuto,
             "WebAuthenticationDisabledOnAuto",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Default amount of days after which the global navigation capturing IPH
// guardrails are cleared from storage.
const base::FeatureParam<int> kNavigationCapturingIPHGuardrailStorageDuration{
    &kPwaNavigationCapturing, "link_capturing_guardrail_storage_duration", 30};

BASE_FEATURE(kPwaNavigationCapturing,
             "PwaNavigationCapturing",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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
    CapturingState::kReimplOnViaClientMode,
#endif
    &kNavigationCapturingParams};

const base::FeatureParam<std::string> kForcedOffCapturingAppsOnFirstNavigation{
    &kPwaNavigationCapturing, "initial_nav_forced_off_apps", ""};

const base::FeatureParam<std::string> kForcedOffCapturingAppsUserSetting{
    &kPwaNavigationCapturing, "user_settings_forced_off_apps", ""};

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
