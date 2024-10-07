// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "content/common/buildflags.h"
#include "content/public/common/dips_utils.h"

namespace features {

// All features in alphabetical order.

// Kill switch to guard additional security checks performed by the browser
// process on opaque origins, such as when verifying source origins for
// postMessage. See https://crbug.com/40109437.
BASE_FEATURE(kAdditionalOpaqueOriginEnforcements,
             "AdditionalOpaqueOriginEnforcements",
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

// Launches the audio service on the browser startup.
BASE_FEATURE(kAudioServiceLaunchOnStartup,
             "AudioServiceLaunchOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Runs the audio service in a separate process.
BASE_FEATURE(kAudioServiceOutOfProcess,
             "AudioServiceOutOfProcess",
// TODO(crbug.com/40118868): Remove !IS_CHROMEOS_LACROS once lacros starts being
// built with OS_CHROMEOS instead of OS_LINUX.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
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

// Allows pages that created a MediaSession service to stay eligible for the
// back/forward cache.
BASE_FEATURE(kBackForwardCacheMediaSessionService,
             "BackForwardCacheMediaSessionService",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// When enabled, mouse user activation will be verified by the browser side.
BASE_FEATURE(kBrowserVerifiedUserActivationMouse,
             "BrowserVerifiedUserActivationMouse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows pages with cache-control:no-store to enter the back/forward cache.
// Feature params can specify whether pages with cache-control:no-store can be
// restored if cookies change / if HTTPOnly cookies change.
// TODO(crbug.com/40189625): Enable this feature.
BASE_FEATURE(kCacheControlNoStoreEnterBackForwardCache,
             "CacheControlNoStoreEnterBackForwardCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This killswitch is distinct from the OT.
// It allows us to remotely disable the feature, and get it to stop working even
// on sites that are in possession of a valid token. When that happens, all API
// calls gated by the killswitch will fail graceully.
BASE_FEATURE(kCapturedSurfaceControlKillswitch,
             "CapturedSurfaceControlKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, CSC permissions are sticky - as all other permissions.
// If disabled, CSC permissions are scoped to the capture session's duration.
BASE_FEATURE(kCapturedSurfaceControlStickyPermissions,
             "CapturedSurfaceControlStickyPermissions",
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

// When enabled, the DevTools Privacy UI is displayed.
BASE_FEATURE(kDevToolsPrivacyUI,
             "DevToolsPrivacyUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Blink cooperative scheduling.
BASE_FEATURE(kCooperativeScheduling,
             "CooperativeScheduling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables crash reporting via Reporting API.
// https://www.w3.org/TR/reporting/#crash-report
BASE_FEATURE(kCrashReporting,
             "CrashReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables the DIPS (Detect Incidental Party State) feature.
// On by default to allow for collecting metrics. All potentially dangerous
// behavior (database persistence, DIPS deletion) will be gated by params.
BASE_FEATURE(kDIPS, "DIPS", base::FEATURE_ENABLED_BY_DEFAULT);

// Set whether DIPS persists its database to disk.
const base::FeatureParam<bool> kDIPSPersistedDatabaseEnabled{
    &kDIPS, "persist_database", true};

// Set whether DIPS performs deletion.
const base::FeatureParam<bool> kDIPSDeletionEnabled{&kDIPS, "delete", true};

// Set the time period that Chrome will wait for before clearing storage for a
// site after it performs some action (e.g. bouncing the user or using storage)
// without user interaction.
const base::FeatureParam<base::TimeDelta> kDIPSGracePeriod{
    &kDIPS, "grace_period", base::Hours(1)};

// Set the cadence at which Chrome will attempt to clear incidental state
// repeatedly.
const base::FeatureParam<base::TimeDelta> kDIPSTimerDelay{&kDIPS, "timer_delay",
                                                          base::Hours(1)};

// Sets how long DIPS maintains interactions and Web Authn Assertions (WAA) for
// a site.
//
// If a site in the DIPS database has an interaction or WAA within the grace
// period a DIPS-triggering action, then that action and all ensuing actions are
// protected from DIPS clearing until the interaction and WAA "expire" as set
// by this param.
// NOTE: Updating this param name (to reflect WAA) is deemed unnecessary as far
// as readability is concerned.
const base::FeatureParam<base::TimeDelta> kDIPSInteractionTtl{
    &kDIPS, "interaction_ttl", base::Days(45)};

constexpr base::FeatureParam<content::DIPSTriggeringAction>::Option
    kDIPSTriggeringActionOptions[] = {
        {content::DIPSTriggeringAction::kNone, "none"},
        {content::DIPSTriggeringAction::kStorage, "storage"},
        {content::DIPSTriggeringAction::kBounce, "bounce"},
        {content::DIPSTriggeringAction::kStatefulBounce, "stateful_bounce"}};

// Sets the actions which will trigger DIPS clearing for a site. The default is
// to set to kBounce, but can be overridden by Finch experiment groups,
// command-line flags, or chrome flags.
//
// Note: Maintain a matching nomenclature of the options with the feature flag
// entries at about_flags.cc.
const base::FeatureParam<content::DIPSTriggeringAction> kDIPSTriggeringAction{
    &kDIPS, "triggering_action", content::DIPSTriggeringAction::kStatefulBounce,
    &kDIPSTriggeringActionOptions};

// Denotes the length of a time interval within which any client-side redirect
// is viewed as a bounce (provided all other criteria are equally met). The
// interval starts every time a page finishes a navigation (a.k.a. a commit is
// registered).
const base::FeatureParam<base::TimeDelta> kDIPSClientBounceDetectionTimeout{
    &kDIPS, "client_bounce_detection_timeout", base::Seconds(10)};

// Whether DIPS deletes Privacy Sandbox data.
BASE_FEATURE(kDIPSPreservePSData,
             "DIPSPreservePSData",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables disconnecting the `ExtensionMessagePort` when the page using the port
// enters BFCache.
BASE_FEATURE(kDisconnectExtensionMessagePortWhenPageEntersBFCache,
             "DisconnectExtensionMessagePortWhenPageEntersBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable drawing under System Bars within DisplayCutout.
BASE_FEATURE(kDrawCutoutEdgeToEdge,
             "DrawCutoutEdgeToEdge",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the "Use a different account" button in the FedCM account chooser to
// log in to another IDP account, if the IDP opts in.
BASE_FEATURE(kFedCmUseOtherAccount,
             "FedCmUseOtherAccount",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM Authz API.
BASE_FEATURE(kFedCmAuthz, "FedCmAuthz", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM ButtonMode feature.
// Note that actual exposure of the API to web content is controlled by
// the flag in RuntimeEnabledFeatures on the blink side. See also the use
// of kSetOnlyIfOverridden in content/child/runtime_features.cc. We enable
// it here by default to support use in origin trials.
BASE_FEATURE(kFedCmButtonMode,
             "FedCmButtonMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM IdP Registration API.
BASE_FEATURE(kFedCmIdPRegistration,
             "FedCmIdPregistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the IDP signin status API for use with FedCM, including avoiding
// network requests when not signed in and mismatch handling.
// When turned off, Login-Status headers are still parsed and processed
// and FedCM mismatch metrics are collected.
BASE_FEATURE(kFedCmIdpSigninStatusEnabled,
             "FedCmIdpSigninStatusEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM API with metrics endpoint at the same time.
BASE_FEATURE(kFedCmMetricsEndpoint,
             "FedCmMetricsEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with multiple identity providers at the same
// time.
BASE_FEATURE(kFedCmMultipleIdentityProviders,
             "FedCmMultipleIdentityProviders",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM API with the Selective Disclosure API at the same
// time.
BASE_FEATURE(kFedCmSelectiveDisclosure,
             "FedCmSelectiveDisclosure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables bypassing the well-known file enforcement.
BASE_FEATURE(kFedCmWithoutWellKnownEnforcement,
             "FedCmWithoutWellKnownEnforcement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables browser-side focus verification when crossing fenced boundaries.
BASE_FEATURE(kFencedFramesEnforceFocus,
             "FencedFramesEnforceFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Digital Credential API.
BASE_FEATURE(kWebIdentityDigitalCredentials,
             "WebIdentityDigitalCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Determines if an extra brand version pair containing possibly escaped double
// quotes and escaped backslashed should be added to the Sec-CH-UA header
// (activated by kUserAgentClientHint)
BASE_FEATURE(kGreaseUACH, "GreaseUACH", base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enables reporting of Cookie Issues for Legacy Technology Report.
BASE_FEATURE(kLegacyTechReportEnableCookieIssueReports,
             "LegacyTechReportEnableCookieIssueReports",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Configures whether Blink on Windows 8.0 and below should use out of process
// API font fallback calls to retrieve a fallback font family name as opposed to
// using a hard-coded font lookup table.
BASE_FEATURE(kLegacyWindowsDWriteFontFallback,
             "LegacyWindowsDWriteFontFallback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLogJsConsoleMessages,
             "LogJsConsoleMessages",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Uses ThreadType::kDisplayCritical for the main thread
BASE_FEATURE(kMainThreadCompositingPriority,
             "MainThreadCompositingPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Kill switch for Web Notification content images.
BASE_FEATURE(kNotificationContentImage,
             "NotificationContentImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the notification trigger API.
BASE_FEATURE(kNotificationTriggers,
             "NotificationTriggers",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Kill-switch to introduce a compatibility breaking restriction.
BASE_FEATURE(kPepperCrossOriginRedirectRestriction,
             "PepperCrossOriginRedirectRestriction",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Persistent Origin Trials. It causes tokens for an origin to be stored
// and persisted for the next navigation. This way, an origin trial can affect
// things before receiving the response, for instance it can affect the next
// navigation's network request.
BASE_FEATURE(kPersistentOriginTrials,
             "PersistentOriginTrials",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, browser-initiated prefetch is allowed.
// Please see crbug.com/40946257 for more details.
BASE_FEATURE(kPrefetchBrowserInitiatedTriggers,
             "PrefetchBrowserInitiatedTriggers",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kPushSubscriptionChangeEvent,
             "PushSubscriptionChangeEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, queues navigations instead of cancelling the previous
// navigation if the previous navigation is already waiting for commit.
// See https://crbug.com/838348 and https://crbug.com/1220337.
BASE_FEATURE(kQueueNavigationsWhileWaitingForCommit,
             "QueueNavigationsWhileWaitingForCommit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, sends SubresourceResponseStarted IPC only when the user has
// allowed any HTTPS-related warning exceptions. From field data, (see
// `SSL.Experimental.SubresourceResponse`), ~100% of subresource notifications
// are not required, since allowing certificate exceptions by users is a rare
// event. Hence, if user has never allowed any certificate or HTTP exceptions,
// notifications are not sent to the browser. Once we start sending these
// messages, we keep sending them until all exceptions are revoked and browser
// restart occurs.
BASE_FEATURE(kReduceSubresourceResponseStartedIPC,
             "ReduceSubresourceResponseStartedIPC",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables ServiceWorker static routing API.
// https://github.com/WICG/service-worker-static-routing-api
BASE_FEATURE(kServiceWorkerStaticRouter,
             "ServiceWorkerStaticRouter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Run video capture service in the Browser process as opposed to a dedicated
// utility process.
// Camera requests from Lacros are forwarded to Ash via a Mojo connection
// established through cros-api. Since cros-api isn't available in utility
// processes, Lacros's video capture service has to run within the browser
// process.
BASE_FEATURE(kRunVideoCaptureServiceInBrowserProcess,
             "RunVideoCaptureServiceInBrowserProcess",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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

// http://tc39.github.io/ecmascript_sharedmem/shmem.html
// This feature is also enabled independently of this flag for cross-origin
// isolated renderers.
BASE_FEATURE(kSharedArrayBuffer,
             "SharedArrayBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);
// If enabled, SharedArrayBuffer is present and can be transferred on desktop
// platforms. This flag is used only as a "kill switch" as we migrate towards
// requiring 'crossOriginIsolated'.
BASE_FEATURE(kSharedArrayBufferOnDesktop,
             "SharedArrayBufferOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for creating first-party StorageKeys in
// RenderFrameHostImpl::CalculateStorageKey for frames with extension URLs.
BASE_FEATURE(kShouldAllowFirstPartyStorageKeyOverrideFromEmbedder,
             "ShouldAllowFirstPartyStorageKeyOverrideFromEmbedder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, GetUserMedia API will only work when the concerned tab is in
// focus
BASE_FEATURE(kUserMediaCaptureOnFocus,
             "UserMediaCaptureOnFocus",
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

// This feature controls whether the renderer should use SkiaFontManager to
// fetch fonts from the Browser's SkiaFontService. It is currently scoped to
// just Windows. See crbug.com/335680565.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kSkiaFontService,
             "SkiaFontService",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<SkiaFontServiceTypefaceType>::Option
    skia_font_service_typeface[] = {
        {SkiaFontServiceTypefaceType::kDwrite, "DWrite"},
        {SkiaFontServiceTypefaceType::kFreetype, "FreeType"}};
const base::FeatureParam<SkiaFontServiceTypefaceType>
    kSkiaFontServiceTypefaceType{&kSkiaFontService, "typeface",
                                 SkiaFontServiceTypefaceType::kDwrite,
                                 &skia_font_service_typeface};

// Whether a utility process configured to use a "UI" message pump should also
// initialize COM.
BASE_FEATURE(kUtilityWithUiPumpInitializesCom,
             "UtilityWithUiPumpInitializesCom",
             base::FEATURE_DISABLED_BY_DEFAULT);
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies the memory threshold for the `kSubframeProcessReuseThresholds`
// feature, which only allows a process to be reused for another subframe if the
// process's memory footprint stays below this threshold. Similar to
// `kProcessPerSiteMainFrameTotalMemoryLimit`, and only provided as a separate
// knob so that it can be independently controlled in subframe and main frame
// process reuse experiments.
constexpr base::FeatureParam<double> kSubframeProcessReuseMemoryThreshold{
    &kSubframeProcessReuseThresholds, "SubframeProcessReuseMemoryThreshold",
    2 * 1024 * 1024 * 1024u};

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

// This feature is for a reverse Origin Trial, enabling SharedArrayBuffer for
// sites as they migrate towards requiring cross-origin isolation for these
// features.
// TODO(bbudge): Remove when the deprecation is complete.
// https://developer.chrome.com/origintrials/#/view_trial/303992974847508481
// https://crbug.com/1144104
BASE_FEATURE(kUnrestrictedSharedArrayBuffer,
             "UnrestrictedSharedArrayBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable using browser-calculated origins on navigations, instead of
// renderer-calculated ones.
BASE_FEATURE(kUseBrowserCalculatedOrigin,
             "UseBrowserCalculatedOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
// If enabled, blink's context snapshot is used rather than the v8 snapshot.
BASE_FEATURE(kUseContextSnapshot,
             "UseContextSnapshot",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allows user activation propagation to all frames having the same origin as
// the activation notifier frame.  This is an intermediate measure before we
// have an iframe attribute to declaratively allow user activation propagation
// to subframes.
BASE_FEATURE(kUserActivationSameOriginVisibility,
             "UserActivationSameOriginVisibility",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enable WebAssembly JSPI.
BASE_FEATURE(kEnableExperimentalWebAssemblyJSPI,
             "WebAssemblyExperimentalJSPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebAssembly lazy compilation (JIT on first call).
BASE_FEATURE(kWebAssemblyLazyCompilation,
             "WebAssemblyLazyCompilation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly Memory64.
BASE_FEATURE(kWebAssemblyMemory64,
             "WebAssemblyMemory64",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// When enabled, includes the ACTION_LONG_CLICK action to all relevant nodes in
// the web contents accessibility tree.
BASE_FEATURE(kAccessibilityIncludeLongClickAction,
             "AccessibilityIncludeLongClickAction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the use of page zoom in place of accessibility text autosizing, and
// updated UI to replace existing Chrome Accessibility Settings.
BASE_FEATURE(kAccessibilityPageZoom,
             "AccessibilityPageZoom",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the OS-level font setting is adjusted for.
const base::FeatureParam<bool> kAccessibilityPageZoomOSLevelAdjustment{
    &kAccessibilityPageZoom, "AdjustForOSLevel", false};

// Enables the use of enhancements to the Page Zoom feature based on user
// feedback from the v1 version (e.g. reset button, Site Settings, etc).
// This flag is the fast-follow for the AccessibilityPageZoom experiment.
BASE_FEATURE(kAccessibilityPageZoomEnhancements,
             "AccessibilityPageZoomEnhancements",
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the use of "Smart Zoom", an alternative form of page zoom, and
// enables the associated UI.
BASE_FEATURE(kSmartZoom, "SmartZoom", base::FEATURE_DISABLED_BY_DEFAULT);

// Skips clearing objects on main document ready. Only has an impact
// when gin java bridge is enabled.
BASE_FEATURE(kGinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady,
             "GinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reduce the priority of GPU process when in background so it is more likely
// to be killed first if the OS needs more memory.
BASE_FEATURE(kReduceGpuPriorityOnBackground,
             "ReduceGpuPriorityOnBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, shows a dropdown menu for mouse and trackpad secondary
// clicks (i.e. right click) with respect to text selection.
BASE_FEATURE(kMouseAndTrackpadDropdownMenu,
             "MouseAndTrackpadDropdownMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Apply text selection menu order correction logic for Android.
// TODO(crbug.com/40947146) This is a kill switch landed in M122.
// Please remove after M124.
BASE_FEATURE(kSelectionMenuItemModification,
             "SelectionMenuItemModification",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Screen Capture API support for Android
BASE_FEATURE(kUserMediaScreenCapturing,
             "UserMediaScreenCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for the WebNFC feature. This feature can be enabled for all sites
// using the kEnableExperimentalWebPlatformFeatures flag.
// https://w3c.github.io/web-nfc/
BASE_FEATURE(kWebNfc, "WebNFC", base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for allowing webview to suppress tap immediately after fling,
// matching chrome behavior.
BASE_FEATURE(kWebViewSuppressTapDuringFling,
             "WebViewSuppressTapDuringFling",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables backgrounding hidden renderers on Mac.
BASE_FEATURE(kMacAllowBackgroundingRenderProcesses,
             "MacAllowBackgroundingRenderProcesses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a fix for a macOS IME Live Conversion issue. crbug.com/40226470 and
// crbug.com/40060200
BASE_FEATURE(kMacImeLiveConversionFix,
             "MacImeLiveConversionFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMacSyscallSandbox,
             "MacSyscallSandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Changes how Chrome responds to accessibility activation signals on macOS
// Sonoma, to avoid unnecessary changes to the screen reader state.
BASE_FEATURE(kSonomaAccessibilityActivationRefinements,
             "SonomaAccessibilityActivationRefinements",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_MAC)

#if defined(WEBRTC_USE_PIPEWIRE)
// Controls whether the PipeWire support for screen capturing is enabled on the
// Wayland display server.
BASE_FEATURE(kWebRtcPipeWireCapturer,
             "WebRTCPipeWireCapturer",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // defined(WEBRTC_USE_PIPEWIRE)

// Default amount of days after which the global navigation capturing IPH
// guardrails are cleared from storage.
const base::FeatureParam<int> kNavigationCapturingIPHGuardrailStorageDuration{
    &kPwaNavigationCapturing, "link_capturing_guardrail_storage_duration", 30};

BASE_FEATURE(kPwaNavigationCapturing,
             "PwaNavigationCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<CapturingState>::Option kNavigationCapturingParams[] =
    {{CapturingState::kDefaultOn, "on_by_default"},
     {CapturingState::kDefaultOff, "off_by_default"},
     {CapturingState::kReimplDefaultOn, "reimpl_default_on"},
     {CapturingState::kReimplDefaultOff, "reimpl_default_off"}};

const base::FeatureParam<CapturingState> kNavigationCapturingDefaultState{
    &kPwaNavigationCapturing, "link_capturing_state",
    CapturingState::kDefaultOn, &kNavigationCapturingParams};

namespace {
enum class VideoCaptureServiceConfiguration {
  kEnabledForOutOfProcess,
  kEnabledForBrowserProcess,
  kDisabled
};

VideoCaptureServiceConfiguration GetVideoCaptureServiceConfiguration() {
#if BUILDFLAG(IS_ANDROID)
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

}  // namespace features
