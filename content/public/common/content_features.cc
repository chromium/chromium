// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/buildflags.h"

namespace features {

// All features in alphabetical order.

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

#if BUILDFLAG(IS_ANDROID)
// Use chromim's implementation of selection magnifier built using surface
// control APIs, instead of using the system-provided magnifier.
BASE_FEATURE(kAndroidSurfaceControlMagnifier,
             "AndroidSurfaceControlMagnifier",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables FLEDGE and Attribution Reporting API integration.
BASE_FEATURE(kAttributionFencedFrameReportingBeacon,
             "AttributionFencedFrameReportingBeacon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Launches the audio service on the browser startup.
BASE_FEATURE(kAudioServiceLaunchOnStartup,
             "AudioServiceLaunchOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Runs the audio service in a separate process.
BASE_FEATURE(kAudioServiceOutOfProcess,
             "AudioServiceOutOfProcess",
// TODO(crbug.com/1052397): Remove !IS_CHROMEOS_LACROS once lacros starts being
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

// Kill switch for Background Fetch.
BASE_FEATURE(kBackgroundFetch,
             "BackgroundFetch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable using the BackForwardCache.
BASE_FEATURE(kBackForwardCache,
             "BackForwardCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable showing a page preview during back/forward navigations.
BASE_FEATURE(kBackForwardTransitions,
             "BackForwardTransitions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables reporting ResourceTiming entries for document, who initiated a
// cancelled navigation in one of their <iframe>.
BASE_FEATURE(kResourceTimingForCancelledNavigationInFrame,
             "ResourceTimingForCancelledNavigationInFrame",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows pages that created a MediaSession service to stay eligible for the
// back/forward cache.
BASE_FEATURE(kBackForwardCacheMediaSessionService,
             "BackForwardCacheMediaSessionService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set a time limit for the page to enter the cache. Disabling this prevents
// flakes during testing.
BASE_FEATURE(kBackForwardCacheEntryTimeout,
             "BackForwardCacheEntryTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables controlling the time to live for pages in the BackForwardCache.
// The time to live is defined by the param 'time_to_live_seconds'; if this
// param is not specified then this feature is ignored and the default is used.
BASE_FEATURE(kBackForwardCacheTimeToLiveControl,
             "BackForwardCacheTimeToLiveControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable back/forward cache for screen reader users. This flag should be
// removed once the https://crbug.com/1271450 is resolved.
BASE_FEATURE(kEnableBackForwardCacheForScreenReader,
             "EnableBackForwardCacheForScreenReader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// BackForwardCache is disabled on low memory devices. The threshold is defined
// via a field trial param: "memory_threshold_for_back_forward_cache_in_mb"
// It is compared against base::SysInfo::AmountOfPhysicalMemoryMB().

// "BackForwardCacheMemoryControls" is checked before "BackForwardCache". It
// means the low memory devices will activate neither the control group nor the
// experimental group of the BackForwardCache field trial.

// BackForwardCacheMemoryControls is enabled only on Android to disable
// BackForwardCache for lower memory devices due to memory limiations.
BASE_FEATURE(kBackForwardCacheMemoryControls,
             "BackForwardCacheMemoryControls",

#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// When this feature is enabled, private network requests initiated from
// non-secure contexts in the `public` address space  are blocked.
//
// See also:
//  - https://wicg.github.io/private-network-access/#integration-fetch
//  - kBlockInsecurePrivateNetworkRequestsFromPrivate
//  - kBlockInsecurePrivateNetworkRequestsFromUnknown
//  - kBlockInsecurePrivateNetworkRequestsForNavigations
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

// When this feature is enabled, requests to localhost initiated from non-secure
// contexts in the `unknown` IP address space are blocked.
//
// See also:
//  - kBlockInsecurePrivateNetworkRequests
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsFromUnknown,
             "BlockInsecurePrivateNetworkRequestsFromUnknown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of the PrivateNetworkAccessNonSecureContextsAllowed deprecation
// trial. This is a necessary yet insufficient condition: documents that wish to
// make use of the trial must additionally serve a valid origin trial token.
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
             "BlockInsecurePrivateNetworkRequestsDeprecationTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When both kBlockInsecurePrivateNetworkRequestsForNavigations and
// kBlockInsecurePrivateNetworkRequests are enabled, navigations initiated
// by documents in a less-private network may only target a more-private network
// if the initiating context is secure.
BASE_FEATURE(kBlockInsecurePrivateNetworkRequestsForNavigations,
             "BlockInsecurePrivateNetworkRequestsForNavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When kPrivateNetworkAccessPermissionPrompt is enabled, public secure websites
// are allowed to access private insecure subresources with user's permission.
BASE_FEATURE(kPrivateNetworkAccessPermissionPrompt,
             "PrivateNetworkRequestPermissionPrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Broker file operations on disk cache in the Network Service.
// This is no-op if the network service is hosted in the browser process.
BASE_FEATURE(kBrokerFileOperationsOnDiskCacheInNetworkService,
             "BrokerFileOperationsOnDiskCacheInNetworkService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, keyboard user activation will be verified by the browser side.
BASE_FEATURE(kBrowserVerifiedUserActivationKeyboard,
             "BrowserVerifiedUserActivationKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, mouse user activation will be verified by the browser side.
BASE_FEATURE(kBrowserVerifiedUserActivationMouse,
             "BrowserVerifiedUserActivationMouse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows pages with cache-control:no-store to enter the back/forward cache.
// Feature params can specify whether pages with cache-control:no-store can be
// restored if cookies change / if HTTPOnly cookies change.
// TODO(crbug.com/1228611): Enable this feature.
BASE_FEATURE(kCacheControlNoStoreEnterBackForwardCache,
             "CacheControlNoStoreEnterBackForwardCache",
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

// Clear the window.name property for the top-level cross-site navigations that
// swap BrowsingContextGroups(BrowsingInstances).
BASE_FEATURE(kClearCrossSiteCrossBrowsingContextGroupWindowName,
             "ClearCrossSiteCrossBrowsingContextGroupWindowName",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCompositeBGColorAnimation,
             "CompositeBGColorAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCompositeClipPathAnimation,
             "CompositeClipPathAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, code cache does not use a browsing_data filter for deletions.
BASE_FEATURE(kCodeCacheDeletionWithoutFilter,
             "CodeCacheDeletionWithoutFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, event.movement is calculated in blink instead of in browser.
BASE_FEATURE(kConsolidatedMovementXY,
             "ConsolidatedMovementXY",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Blink cooperative scheduling.
BASE_FEATURE(kCooperativeScheduling,
             "CooperativeScheduling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables crash reporting via Reporting API.
// https://www.w3.org/TR/reporting/#crash-report
BASE_FEATURE(kCrashReporting,
             "CrashReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables support for the `Critical-CH` response header.
// https://github.com/WICG/client-hints-infrastructure/blob/master/reliability.md#critical-ch
BASE_FEATURE(kCriticalClientHint,
             "CriticalClientHint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable debugging the issue crbug.com/1201355
BASE_FEATURE(kDebugHistoryInterventionNoUserActivation,
             "DebugHistoryInterventionNoUserActivation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable changing source dynamically for desktop capture.
BASE_FEATURE(kDesktopCaptureChangeSource,
             "DesktopCaptureChangeSource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adds a tab strip to PWA windows.
// TODO(crbug.com/897314): Enable this feature.
BASE_FEATURE(kDesktopPWAsTabStrip,
             "DesktopPWAsTabStrip",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the device posture API.
// Tracking bug for enabling device posture API: https://crbug.com/1066842.
BASE_FEATURE(kDevicePosture,
             "DevicePosture",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable document policy for configuring and restricting feature behavior.
BASE_FEATURE(kDocumentPolicy,
             "DocumentPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable document policy negotiation mechanism.
BASE_FEATURE(kDocumentPolicyNegotiation,
             "DocumentPolicyNegotiation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable establishing the GPU channel early in renderer startup.
BASE_FEATURE(kEarlyEstablishGpuChannel,
             "EarlyEstablishGpuChannel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Requires documents embedded via <iframe>, etc, to explicitly opt-into the
// embedding: https://github.com/mikewest/embedding-requires-opt-in.
BASE_FEATURE(kEmbeddingRequiresOptIn,
             "EmbeddingRequiresOptIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables canvas 2d methods BeginLayer and EndLayer.
BASE_FEATURE(kEnableCanvas2DLayers,
             "EnableCanvas2DLayers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Machine Learning Model Loader Web Platform API. Explainer:
// https://github.com/webmachinelearning/model-loader/blob/main/explainer.md
BASE_FEATURE(kEnableMachineLearningModelLoaderWebPlatformApi,
             "EnableMachineLearningModelLoaderWebPlatformApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome-untrusted:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeUntrusted,
             "EnableServiceWorkersForChromeUntrusted",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables service workers on chrome:// urls.
BASE_FEATURE(kEnableServiceWorkersForChromeScheme,
             "EnableServiceWorkersForChromeScheme",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled and device permission is not granted by the user,
// media-device enumeration will provide at most one device per type and the
// device IDs will not be available.
// TODO(crbug.com/1019176): remove the feature in M89.
BASE_FEATURE(kEnumerateDevicesHideDeviceIDs,
             "EnumerateDevicesHideDeviceIDs",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Content counterpart of ExperimentalContentSecurityPolicyFeatures in
// third_party/blink/renderer/platform/runtime_enabled_features.json5. Enables
// experimental Content Security Policy features ('navigate-to').
BASE_FEATURE(kExperimentalContentSecurityPolicyFeatures,
             "ExperimentalContentSecurityPolicyFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Extra CORS safelisted headers. See https://crbug.com/999054.
BASE_FEATURE(kExtraSafelistedRequestHeadersForOutOfBlinkCors,
             "ExtraSafelistedRequestHeadersForOutOfBlinkCors",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables JavaScript API to intermediate federated identity requests.
// Note that actual exposure of the FedCM API to web content is controlled
// by the flag in RuntimeEnabledFeatures on the blink side. See also
// the use of kSetOnlyIfOverridden in content/child/runtime_features.cc.
// We enable it here by default to support use in origin trials.
BASE_FEATURE(kFedCm, "FedCm", base::FEATURE_ENABLED_BY_DEFAULT);

// Field trial boolean parameter which indicates whether FedCM IDP sign-out
// is enabled.
const char kFedCmIdpSignoutFieldTrialParamName[] = "IdpSignout";

// Enables usage of the FedCM Authz API.
BASE_FEATURE(kFedCmAuthz, "FedCmAuthz", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with auto re-authentication. Note that actual
// exposure of FedCM's auto re-authentication feature to web content is
// controlled by the flag in RuntimeEnabledFeatures on the blink side. See also
// the use of kSetOnlyIfOverridden in content/child/runtime_features.cc.
// We enable it here by default to support use in origin trials.
BASE_FEATURE(kFedCmAutoReauthn,
             "FedCmAutoReauthn",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM IdP Registration API.
BASE_FEATURE(kFedCmIdPRegistration,
             "FedCmIdPregistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with iframe support.
BASE_FEATURE(kFedCmIframeSupport,
             "FedCmIframeSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM API with metrics endpoint at the same time.
BASE_FEATURE(kFedCmMetricsEndpoint,
             "FedCmMetricsEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with multiple identity providers at the same
// time.
BASE_FEATURE(kFedCmMultipleIdentityProviders,
             "FedCmMultipleIdentityProviders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM Relying Party Context API.
BASE_FEATURE(kFedCmRpContext,
             "FedCmRpContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with the User Info API at the same time.
// Note that actual exposure of the FedCM API to web content is controlled
// by the flag in RuntimeEnabledFeatures on the blink side. See also
// the use of kSetOnlyIfOverridden in content/child/runtime_features.cc.
BASE_FEATURE(kFedCmUserInfo, "FedCmUserInfo", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables usage of the FedCM API with the Selective Disclosure API at the same
// time.
BASE_FEATURE(kFedCmSelectiveDisclosure,
             "FedCmSelectiveDisclosure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of the FedCM API with the login hint parameter.
BASE_FEATURE(kFedCmLoginHint,
             "FedCmLoginHint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Field trial boolean parameter which indicates whether IdpSigninStatus API is
// used in FedCM API.
const char kFedCmIdpSigninStatusFieldTrialParamName[] = "IdpSigninStatus";

// Alternative to `kFedCmIdpSigninStatusFieldTrialParamName` which runs
// IdpSigninStatus API in a metrics-only mode. This field trial is default-on
// and is intended as a kill switch.
// `kFedCmIdpSigninStatusFieldTrialParamName` takes precedence over
// `kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName`.
const char kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName[] =
    "IdpSigninStatusMetricsOnly";

// Enables the MDocs API in the IdentityCredential.
BASE_FEATURE(kWebIdentityMDocs,
             "WebIdentityMDocs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables usage of First Party Sets to determine cookie availability.
BASE_FEATURE(kFirstPartySets,
             "FirstPartySets",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to clear sites data on FPS transitions.
const base::FeatureParam<bool> kFirstPartySetsClearSiteDataOnChangedSets{
    &kFirstPartySets, "FirstPartySetsClearSiteDataOnChangedSets", true};

// Controls whether the client is considered a dogfooder for the FirstPartySets
// feature.
const base::FeatureParam<bool> kFirstPartySetsIsDogfooder{
    &kFirstPartySets, "FirstPartySetsIsDogfooder", false};

// Controls how many sites are allowed to be in the Associated subset (ignoring
// ccTLD aliases).
const base::FeatureParam<int> kFirstPartySetsMaxAssociatedSites{
    &kFirstPartySets, "FirstPartySetsMaxAssociatedSites", 3};

// Controls the maximum time duration an outermost frame navigation should be
// deferred by FPS initialization.
// Using 2s as the starting default timeout. This is based on the UMA metric
// `History.ClearBrowsingData.Duration.OriginDeletion`.
const base::FeatureParam<base::TimeDelta>
    kFirstPartySetsNavigationThrottleTimeout{
        &kFirstPartySets, "FirstPartySetsNavigationThrottleTimeout",
        base::Seconds(2)};

// Enables fixes for matching src: local() for web fonts correctly against full
// font name or postscript name. Rolling out behind a flag, as enabling this
// enables a font indexer on Android which we need to test in the field first.
BASE_FEATURE(kFontSrcLocalMatching,
             "FontSrcLocalMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
// Feature controlling whether or not memory pressure signals will be forwarded
// to the GPU process.
BASE_FEATURE(kForwardMemoryPressureEventsToGpuProcess,
             "ForwardMemoryPressureEventsToGpuProcess",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// If enabled, limits the number of FLEDGE auctions that can be run between page
// load and unload -- any attempt to run more than this number of auctions will
// fail (return null to JavaScript).
BASE_FEATURE(kFledgeLimitNumAuctions,
             "LimitNumFledgeAuctions",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The number of allowed auctions for each page load (load to unload).
const base::FeatureParam<int> kFledgeLimitNumAuctionsParam{
    &kFledgeLimitNumAuctions, "max_auctions_per_page", 8};

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Supports proxying thread type changes of renderer processes to browser
// process and having browser process handle adjusting thread properties (nice
// value, c-group, latency sensitivity...) for renderers which have sandbox
// restrictions.
BASE_FEATURE(kHandleRendererThreadTypeChangesInBrowser,
             "HandleRendererThreadTypeChangesInBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// This is intended as a kill switch for the Idle Detection feature. To enable
// this feature, the experimental web platform features flag should be set,
// or the site should obtain an Origin Trial token.
BASE_FEATURE(kIdleDetection, "IdleDetection", base::FEATURE_ENABLED_BY_DEFAULT);

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
// Please don't use this feature flag directly to guard the IWA code. Use
// IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled() in the browser process
// or check kEnableIsolatedWebAppsInRenderer command line flag in the renderer
// process.
BASE_FEATURE(kIsolatedWebApps,
             "IsolatedWebApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable support for IWA Controlled Frame. This gates allowing IWAs to provide
// a functional Controlled Frame tag to IWA apps.
// See https://github.com/chasephillips/controlled-frame/blob/main/EXPLAINER.md
// for more info.
BASE_FEATURE(kIwaControlledFrame,
             "IwaControlledFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the TC39 Array grouping proposal.
BASE_FEATURE(kJavaScriptArrayGrouping,
             "JavaScriptArrayGrouping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables experimental JavaScript shared memory features.
BASE_FEATURE(kJavaScriptExperimentalSharedMemory,
             "JavaScriptExperimentalSharedMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLazyFrameLoading,
             "LazyFrameLoading",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable lazy initialization of the media controls.
BASE_FEATURE(kLazyInitializeMediaControls,
             "LazyInitializeMediaControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Configures whether we set a lower limit for renderers that do not have a main
// frame, similar to the limit that is already done for backgrounded renderers.
BASE_FEATURE(kLowerV8MemoryLimitForNonMainRenderers,
             "LowerV8MemoryLimitForNonMainRenderers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a fix for a macOS IME Live Conversion issue. crbug.com/1328530.
BASE_FEATURE(kMacImeLiveConversionFix,
             "MacImeLiveConversionFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Uses ThreadType::kCompositing for the main thread
BASE_FEATURE(kMainThreadCompositingPriority,
             "MainThreadCompositingPriority",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
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

// Enables/disables the video capture service.
BASE_FEATURE(kMojoVideoCapture,
             "MojoVideoCapture",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A secondary switch used in combination with kMojoVideoCapture.
// This is intended as a kill switch to allow disabling the service on
// particular groups of devices even if they forcibly enable kMojoVideoCapture
// via a command-line argument.
BASE_FEATURE(kMojoVideoCaptureSecondary,
             "MojoVideoCaptureSecondary",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When NavigationNetworkResponseQueue is enabled, the browser will schedule
// some tasks related to navigation network responses in a kHighest priority
// queue.
BASE_FEATURE(kNavigationNetworkResponseQueue,
             "NavigationNetworkResponseQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Preconnects socket at the construction of NavigationRequest.
BASE_FEATURE(kNavigationRequestPreconnect,
             "NavigationRequestPreconnect",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Whether web apps can run periodic tasks upon network connectivity.
BASE_FEATURE(kPeriodicBackgroundSync,
             "PeriodicBackgroundSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If Pepper 3D Image Chromium is allowed, this feature controls whether it is
// enabled.
// TODO(https://crbug.com/1196009): Remove this feature, remove the code that
// uses it.
BASE_FEATURE(kPepper3DImageChromium,
             "Pepper3DImageChromium",
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

// Tells the RenderFrameHost to send beforeunload messages on a different
// local frame interface which will handle the messages at a higher priority.
BASE_FEATURE(kHighPriorityBeforeUnload,
             "HighPriorityBeforeUnload",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Preload cookie database on NetworkContext creation.
BASE_FEATURE(kPreloadCookies,
             "PreloadCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prerender2 holdback feature disables prerendering on all predictors. This is
// useful in comparing the impact of blink::features::kPrerender2 experiment
// with and without Prerendering.

// Please note this feature is only used for experimental purposes, please don't
// enable this feature by default.
BASE_FEATURE(kPrerender2Holdback,
             "Prerender2Holdback",
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

// Please note this feature is only used for experimental purposes, please don't
// enable this feature by default.
BASE_FEATURE(kPreloadingHoldback,
             "PreloadingHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables exposure of the core milestone 1 (M1) APIs in the renderer without an
// origin trial token: Attribution Reporting, FLEDGE, Topics.
BASE_FEATURE(kPrivacySandboxAdsAPIsM1Override,
             "PrivacySandboxAdsAPIsM1Override",
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
// Similar to `kPrivateNetworkAccessForWorkers`, except that it does not require
// CORS preflight requests to succeed, and shows a warning in devtools instead.
BASE_FEATURE(kPrivateNetworkAccessForWorkersWarningOnly,
             "PrivateNetworkAccessForWorkersWarningOnly",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Enable the ProactivelySwapBrowsingInstance experiment. A browsing instance
// represents a set of frames that can script each other. Currently, Chrome does
// not always switch BrowsingInstance when navigating in between two unrelated
// pages. This experiment makes Chrome swap BrowsingInstances for cross-site
// HTTP(S) navigations when the BrowsingInstance doesn't contain any other
// windows.
BASE_FEATURE(kProactivelySwapBrowsingInstance,
             "ProactivelySwapBrowsingInstance",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables skipping the early call to CommitPending when navigating away from a
// crashed frame.
BASE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame,
             "SkipEarlyCommitPendingForCrashedFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables bypassing the service worker fetch handler. Unlike
// `kServiceWorkerSkipIgnorableFetchHandler`, this feature starts the service
// worker for subsequent requests.
BASE_FEATURE(kServiceWorkerBypassFetchHandler,
             "ServiceWorkerBypassFetchHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<ServiceWorkerBypassFetchHandlerStrategy>::Option
    service_worker_bypass_fetch_handler_strategy_options[] = {
        {ServiceWorkerBypassFetchHandlerStrategy::kFeatureOptIn, "optin"},
        {ServiceWorkerBypassFetchHandlerStrategy::kAllowList, "allowlist"}};
const base::FeatureParam<ServiceWorkerBypassFetchHandlerStrategy>
    kServiceWorkerBypassFetchHandlerStrategy{
        &kServiceWorkerBypassFetchHandler, "strategy",
        ServiceWorkerBypassFetchHandlerStrategy::kFeatureOptIn,
        &service_worker_bypass_fetch_handler_strategy_options};

const base::FeatureParam<ServiceWorkerBypassFetchHandlerTarget>::Option
    service_worker_bypass_fetch_handler_target_options[] = {
        {
            ServiceWorkerBypassFetchHandlerTarget::kMainResource,
            "main_resource",
        },
        {
            ServiceWorkerBypassFetchHandlerTarget::
                kAllOnlyIfServiceWorkerNotStarted,
            "all_only_if_service_worker_not_started",
        },
        {
            ServiceWorkerBypassFetchHandlerTarget::kAllWithRaceNetworkRequest,
            "all_with_race_network_request",
        },
        {
            ServiceWorkerBypassFetchHandlerTarget::kSubResource,
            "sub_resource",
        },
};
const base::FeatureParam<ServiceWorkerBypassFetchHandlerTarget>
    kServiceWorkerBypassFetchHandlerTarget{
        &kServiceWorkerBypassFetchHandler, "bypass_for",
        ServiceWorkerBypassFetchHandlerTarget::kMainResource,
        &service_worker_bypass_fetch_handler_target_options};

// The set of ServiceWorker to bypass while making navigation request.
// They are represented by a comma separated list of HEX encoded SHA256 hash of
// the ServiceWorker's scripts.
// e.g.
// 9685C8DE399237BDA6FF3AD0F281E9D522D46BB0ECFACE05E98D2B9AAE51D1EF,
// 20F0D78B280E40C0A17ABB568ACF4BDAFFB9649ADA75B0675F962B3F4FC78EA4
const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings{
        &kServiceWorkerBypassFetchHandler, "script_checksum_to_bypass", ""};

// Enables skipping the service worker fetch handler if the fetch handler is
// identified as ignorable.
BASE_FEATURE(kServiceWorkerSkipIgnorableFetchHandler,
             "ServiceWorkerSkipIgnorableFetchHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature param controls if the empty service worker fetch handler is
// skipped.
constexpr base::FeatureParam<bool> kSkipEmptyFetchHandler{
    &kServiceWorkerSkipIgnorableFetchHandler,
    "SkipEmptyFetchHandler",
    false,
};

// This feature param controls if the service worker is started for an
// empty service worker fetch handler while `kSkipEmptyFetchHandler` is on.
constexpr base::FeatureParam<bool> kStartServiceWorkerForEmptyFetchHandler{
    &kServiceWorkerSkipIgnorableFetchHandler,
    "StartServiceWorkerForEmptyFetchHandler",
    false,
};

// This feature param controls if the service worker is started for an
// empty service worker fetch handler while `kSkipEmptyFetchHandler` is on.
// Unlike the feature param `kStartServiceWorkerForEmptyFetchHandler`,
// this starts service worker in `TaskRunner::PostDelayTask`.
constexpr base::FeatureParam<bool> kAsyncStartServiceWorkerForEmptyFetchHandler{
    &kServiceWorkerSkipIgnorableFetchHandler,
    "AsyncStartServiceWorkerForEmptyFetchHandler",
    false,
};

// This feature param controls duration to start fetch handler
// if `kAsyncStartServiceWorkerForEmptyFetchHandler` is used.
// Negative values and the value larger than a threshold is ignored, and
// treated as 0.
constexpr base::FeatureParam<int>
    kAsyncStartServiceWorkerForEmptyFetchHandlerDurationInMs{
        &kServiceWorkerSkipIgnorableFetchHandler,
        "AsyncStartServiceWorkerForEmptyFetchHandlerDurationInMs",
        0,
    };

// Run video capture service in the Browser process as opposed to a dedicated
// utility process
BASE_FEATURE(kRunVideoCaptureServiceInBrowserProcess,
             "RunVideoCaptureServiceInBrowserProcess",
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

// Make sendBeacon throw for a Blob with a non simple type.
BASE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType,
             "SendBeaconThrowForBlobWithNonSimpleType",
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

// Signed Exchange Reporting for distributors
// https://www.chromestatus.com/feature/5687904902840320
BASE_FEATURE(kSignedExchangeReportingForDistributors,
             "SignedExchangeReportingForDistributors",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Origin-Signed HTTP Exchanges (for WebPackage Loading)
// https://www.chromestatus.com/feature/5745285984681984
BASE_FEATURE(kSignedHTTPExchange,
             "SignedHTTPExchange",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Delays RenderProcessHost shutdown by a few seconds to allow the subframe's
// process to be potentially reused. This aims to reduce process churn in
// navigations where the source and destination share subframes.
// This is enabled only on platforms where the behavior leads to performance
// gains, i.e., those where process startup is expensive.
BASE_FEATURE(kSubframeShutdownDelay,
             "SubframeShutdownDelay",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
);

// If enabled, GetUserMedia API will only work when the concerned tab is in
// focus
BASE_FEATURE(kUserMediaCaptureOnFocus,
             "UserMediaCaptureOnFocus",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is intended as a kill switch for the WebOTP Service feature. To enable
// this feature, the experimental web platform features flag should be set.
BASE_FEATURE(kWebOTP, "WebOTP", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables WebOTP calls in cross-origin iframes if allowed by Permissions
// Policy.
BASE_FEATURE(kWebOTPAssertionFeaturePolicy,
             "WebOTPAssertionFeaturePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable the web lockscreen API implementation
// (https://github.com/WICG/lock-screen) in Chrome.
BASE_FEATURE(kWebLockScreenApi,
             "WebLockScreenApi",
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

BASE_FEATURE(kStopVideoCaptureOnScreenLock,
             "StopVideoCaptureOnScreenLock",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether site isolation should use origins instead of scheme and
// eTLD+1.
BASE_FEATURE(kStrictOriginIsolation,
             "StrictOriginIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disallows window.{alert, prompt, confirm} if triggered inside a subframe that
// is not same origin with the main frame.
BASE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs,
             "SuppressDifferentOriginSubframeJSDialogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// To disable the updated fullscreen handling of the companion Viz
// SurfaceSyncThrottling flag. Disabling this will restore the base
// SurfaceSyncThrottling path.
BASE_FEATURE(kSurfaceSyncFullscreenKillswitch,
             "SurfaceSyncFullscreenKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// This feature is for a reverse Origin Trial, enabling SharedArrayBuffer for
// sites as they migrate towards requiring cross-origin isolation for these
// features.
// TODO(bbudge): Remove when the deprecation is complete.
// https://developer.chrome.com/origintrials/#/view_trial/303992974847508481
// https://crbug.com/1144104
BASE_FEATURE(kUnrestrictedSharedArrayBuffer,
             "UnrestrictedSharedArrayBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the <video>.getVideoPlaybackQuality() API is enabled.
BASE_FEATURE(kVideoPlaybackQuality,
             "VideoPlaybackQuality",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables future V8 VM features
BASE_FEATURE(kV8VmFuture, "V8VmFuture", base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebAssembly baseline compilation (Liftoff).
BASE_FEATURE(kWebAssemblyBaseline,
             "WebAssemblyBaseline",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly JSPI.
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)
BASE_FEATURE(kEnableExperimentalWebAssemblyJSPI,
             "WebAssemblyExperimentalJSPI",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)

// Enable WebAssembly dynamic tiering (only tier up hot functions).
BASE_FEATURE(kWebAssemblyDynamicTiering,
             "WebAssemblyDynamicTiering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable support for the WebAssembly Garbage Collection proposal:
// https://github.com/WebAssembly/gc.
BASE_FEATURE(kWebAssemblyGarbageCollection,
             "WebAssemblyGarbageCollection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebAssembly lazy compilation (JIT on first call).
BASE_FEATURE(kWebAssemblyLazyCompilation,
             "WebAssemblyLazyCompilation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the use of WebAssembly Relaxed SIMD operations
BASE_FEATURE(kWebAssemblyRelaxedSimd,
             "WebAssemblyRelaxedSimd",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable support for the WebAssembly Stringref proposal:
// https://github.com/WebAssembly/stringref.
BASE_FEATURE(kWebAssemblyStringref,
             "WebAssemblyStringref",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable WebAssembly tiering (Liftoff -> TurboFan).
BASE_FEATURE(kWebAssemblyTiering,
             "WebAssemblyTiering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WebAssembly trap handler.
BASE_FEATURE(kWebAssemblyTrapHandler,
             "WebAssemblyTrapHandler",
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
      BUILDFLAG(IS_MAC)) &&                                                 \
     defined(ARCH_CPU_X86_64)) ||                                           \
    (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64))
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether WebAuthn get requests for discoverable credentials use the
// Touch To Fill bottom sheet on Android.
BASE_FEATURE(kWebAuthnTouchToFillCredentialSelection,
             "WebAuthnTouchToFillCredentialSelection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the Web Bluetooth API is enabled:
// https://webbluetoothcg.github.io/web-bluetooth/
BASE_FEATURE(kWebBluetooth, "WebBluetooth", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Web Bluetooth should use the new permissions backend. The
// new permissions backend uses ChooserContextBase, which is used by other
// device APIs, such as WebUSB. When enabled, WebBluetoothWatchAdvertisements
// and WebBluetoothGetDevices blink features are also enabled.
BASE_FEATURE(kWebBluetoothNewPermissionsBackend,
             "WebBluetoothNewPermissionsBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Web Environment Integrity API.
BASE_FEATURE(kWebEnvironmentIntegrity,
             "WebEnvironmentIntegrity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
BASE_FEATURE(kWebGLImageChromium,
             "WebGLImageChromium",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the browser process components of the Web MIDI API. This flag does not
// control whether the API is exposed in Blink.
BASE_FEATURE(kWebMidi, "WebMidi", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls which backend is used to retrieve OTP on Android. When disabled
// we use User Consent API.
BASE_FEATURE(kWebOtpBackendAuto,
             "WebOtpBackendAuto",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The JavaScript API for payments on the web.
BASE_FEATURE(kWebPayments, "WebPayments", base::FEATURE_ENABLED_BY_DEFAULT);

// Use GpuMemoryBuffer backed VideoFrames in media streams.
BASE_FEATURE(kWebRtcUseGpuMemoryBufferVideoFrames,
             "WebRTC-UseGpuMemoryBufferVideoFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables code caching for scripts used on WebUI pages.
BASE_FEATURE(kWebUICodeCache,
             "WebUICodeCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
BASE_FEATURE(kWebUsb, "WebUSB", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the WebXR Device API is enabled.
BASE_FEATURE(kWebXr, "WebXR", base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Allows the experimental approach of proactively generating an accessibility
// tree asynchronously off the main thread, before the framework requests it.
BASE_FEATURE(kAccessibilityAsyncTreeConstruction,
             "AccessibilityAsyncTreeConstruction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the use of page zoom in place of accessibility text autosizing, and
// updated UI to replace existing Chrome Accessibility Settings.
BASE_FEATURE(kAccessibilityPageZoom,
             "AccessibilityPageZoom",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Automatically disables accessibility on Android when no assistive
// technologies are present
BASE_FEATURE(kAutoDisableAccessibilityV2,
             "AutoDisableAccessibilityV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Sets moderate binding to background renderers playing media, when enabled.
// Else the renderer will have strong binding.
BASE_FEATURE(kBackgroundMediaRendererHasModerateBinding,
             "BackgroundMediaRendererHasModerateBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reduce the priority of GPU process when in background so it is more likely
// to be killed first if the OS needs more memory.
BASE_FEATURE(kReduceGpuPriorityOnBackground,
             "ReduceGpuPriorityOnBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows the use of an experimental feature to drop any AccessibilityEvents
// that are not relevant to currently enabled accessibility services.
BASE_FEATURE(kOnDemandAccessibilityEvents,
             "OnDemandAccessibilityEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Request Desktop Site secondary settings for Android; including display
// setting and peripheral setting.
BASE_FEATURE(kRequestDesktopSiteAdditions,
             "RequestDesktopSiteAdditions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Request Desktop Site per-site setting for Android.
// Refer to the launch bug (https://crbug.com/1244979) for more information.
BASE_FEATURE(kRequestDesktopSiteExceptions,
             "RequestDesktopSiteExceptions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Request Desktop Site zoom for Android. Apply a pre-defined page zoom level
// when desktop user agent is used.
BASE_FEATURE(kRequestDesktopSiteZoom,
             "RequestDesktopSiteZoom",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Text autosizing uses heuristics to inflate text sizes on devices with
// small screens. This feature is for disabling these heuristics.
BASE_FEATURE(kForceOffTextAutosizing,
             "ForceOffTextAutosizing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Screen Capture API support for Android
BASE_FEATURE(kUserMediaScreenCapturing,
             "UserMediaScreenCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Pre-warm up the network process on browser startup.
BASE_FEATURE(kWarmUpNetworkProcess,
             "WarmUpNetworkProcess",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for the WebNFC feature. This feature can be enabled for all sites
// using the kEnableExperimentalWebPlatformFeatures flag.
// https://w3c.github.io/web-nfc/
BASE_FEATURE(kWebNfc, "WebNFC", base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enables caching of media devices for the purpose of enumerating them.
BASE_FEATURE(kDeviceMonitorMac,
             "DeviceMonitorMac",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable IOSurface based screen capturer.
BASE_FEATURE(kIOSurfaceCapturer,
             "IOSurfaceCapturer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables backgrounding hidden renderers on Mac.
BASE_FEATURE(kMacAllowBackgroundingRenderProcesses,
             "MacAllowBackgroundingRenderProcesses",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMacSyscallSandbox,
             "MacSyscallSandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature that controls whether WebContentsOcclusionChecker should handle
// occlusion notifications.
BASE_FEATURE(kMacWebContentsOcclusion,
             "MacWebContentsOcclusion",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_MAC)

#if defined(WEBRTC_USE_PIPEWIRE)
// Controls whether the PipeWire support for screen capturing is enabled on the
// Wayland display server.
BASE_FEATURE(kWebRtcPipeWireCapturer,
             "WebRTCPipeWireCapturer",
             base::FEATURE_ENABLED_BY_DEFAULT);
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
  if (!ShouldEnableVideoCaptureService()) {
    return VideoCaptureServiceConfiguration::kDisabled;
  }

// On ChromeOS the service must run in the browser process, because parts of the
// code depend on global objects that are only available in the Browser process.
// See https://crbug.com/891961.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return VideoCaptureServiceConfiguration::kEnabledForBrowserProcess;
#else
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
