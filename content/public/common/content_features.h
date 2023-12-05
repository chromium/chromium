// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/dips_utils.h"

namespace features {

// BEFORE MODIFYING THIS FILE: If your feature is only used inside content/, add
// your feature in `content/common/features.h` instead.

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControlMagnifier);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionFencedFrameReportingBeacon);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceLaunchOnStartup);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceOutOfProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceSandbox);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundFetch);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheEntryTimeout);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMemoryControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMediaSessionService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardTransitions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBlockInsecurePrivateNetworkRequests);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromPrivate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsDeprecationTrial);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBlockMidiByDefault);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBrokerFileOperationsOnDiskCacheInNetworkService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationMouse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCacheControlNoStoreEnterBackForwardCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCdmStorageDatabase);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCdmStorageDatabaseMigration);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kClearCrossSiteCrossBrowsingContextGroupWindowName);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeBGColorAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCookieDeprecationFacilitatedTesting);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kCookieDeprecationFacilitatedTestingEnableOTRProfiles;
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kCookieDeprecationLabel;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kCookieDeprecationTestingDisableAdsAPIs;
CONTENT_EXPORT extern const char kCookieDeprecationLabelName[];
CONTENT_EXPORT extern const char kCookieDeprecationTestingDisableAdsAPIsName[];
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCooperativeScheduling);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCrashReporting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDevicePosture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDigitalGoodsApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDIPS);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kDIPSPersistedDatabaseEnabled;
CONTENT_EXPORT extern const base::FeatureParam<bool> kDIPSDeletionEnabled;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDIPSGracePeriod;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta> kDIPSTimerDelay;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDIPSInteractionTtl;
CONTENT_EXPORT extern const base::FeatureParam<content::DIPSTriggeringAction>
    kDIPSTriggeringAction;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDIPSClientBounceDetectionTimeout;
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kDisconnectExtensionMessagePortWhenPageEntersBFCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDrawCutoutEdgeToEdge);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kEarlyDocumentSwapForBackForwardTransitions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEarlyEstablishGpuChannel);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableCanvas2DLayers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kEnableMachineLearningModelLoaderWebPlatformApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeScheme);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeUntrusted);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCm);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmAddAccount);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmAuthz);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmAutoSelectedFlag);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmDomainHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmError);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmExemptIdpWithThirdPartyCookies);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdPRegistration);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdpSigninStatusEnabled);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMetricsEndpoint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMultipleIdentityProviders);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmDisconnect);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSelectiveDisclosure);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmWithoutWellKnownEnforcement);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFencedFramesEnforceFocus);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebIdentityDigitalCredentials);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFirstPartySets);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kFirstPartySetsClearSiteDataOnChangedSets;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFirstPartySetsMaxAssociatedSites;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFirstPartySetsNavigationThrottleTimeout;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFractionalScrollOffsets);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGreaseUACH);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledApp);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledAppProvider);
// LINT.IfChange
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolatedWebApps);
// LINT.ThenChange(//PRESUBMIT.py)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateFencedFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateOrigins);
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT BASE_DECLARE_FEATURE(kJavaScriptExperimentalSharedMemory);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyInitializeMediaControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyTechReportEnableCookieIssueReports);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyTechReportTopLevelUrl);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyWindowsDWriteFontFallback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLogJsConsoleMessages);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMainThreadCompositingPriority);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMBIMode);
enum class MBIMode {
  // In this mode, the AgentSchedulingGroup will use the process-wide legacy IPC
  // channel for communication with the renderer process and to associate its
  // interfaces with. AgentSchedulingGroup will effectively be a pass-through,
  // enabling legacy IPC and mojo behavior.
  kLegacy,

  // In this mode, each AgentSchedulingGroup will have its own legacy IPC
  // channel for communication with the renderer process and to associate its
  // interfaces with. Communication over that channel will not be ordered with
  // respect to the process-global legacy IPC channel. There will only be a
  // single AgentSchedulingGroup per RenderProcessHost.
  kEnabledPerRenderProcessHost,

  // This is just like the above state, however there will be a single
  // AgentSchedulingGroup per SiteInstance, and therefore potentially multiple
  // AgentSchedulingGroups per RenderProcessHost. Ordering between the
  // AgentSchedulingGroups in the same render process is not preserved.
  kEnabledPerSiteInstance,
};
CONTENT_EXPORT extern const base::FeatureParam<MBIMode> kMBIModeParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoVideoCapture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationNetworkResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationUpdatesChildViewsVisibility);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkQualityEstimatorWebHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceInProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationContentImage);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationTriggers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNoStatePrefetchHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOriginIsolationHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOverscrollHistoryNavigationSetting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPeriodicBackgroundSync);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFeaturePolicyHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPepperCrossOriginRedirectRestriction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPermissionElement);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPersistentOriginTrials);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchNewLimits);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsOverride);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForWorkers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForWorkersWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivateNetworkAccessRespectPreflightResults);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessSendPreflights);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOriginKeyedProcessesByDefault);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPushSubscriptionChangeEvent);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kQueueNavigationsWhileWaitingForCommit);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReduceSubresourceResponseStartedIPC);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRenderDocument);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRenderDocumentCompositorReuse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRetryGetVideoCaptureDeviceInfos);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessPerSiteUpToMainFrameThreshold);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kProcessPerSiteMainFrameThreshold;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kProcessPerSiteMainFrameAllowIPAndLocalhost;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kProcessPerSiteMainFrameAllowDevToolsAttached;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRunVideoCaptureServiceInBrowserProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmationDebug);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerPaymentApps);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBufferOnDesktop);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kShouldAllowFirstPartyStorageKeyOverrideFromEmbedder);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSignedHTTPExchange);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteInstanceGroupsForDataUrls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationForCrossOriginOpenerPolicy);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDisableProcessReuse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerBypassFetchHandler);
// ServiceWorkerBypassFetchHandlerStrategy provides the info how to decide if
// the request should bypass fetch handlers or not.
enum class ServiceWorkerBypassFetchHandlerStrategy {
  // Use the allowlist provided by
  // kServiceWorkerBypassFetchHandlerBypassedOrigins. If the request url's
  // origin is in the list, fetch handlers are bypassed.
  kAllowList,

  // This option is to run the feature locally for the debugging purpose. It is
  // used for the feature toggle in about:flags etc. It simply bypasses fetch
  // handlers for all the main resource requests regardless of the url while the
  // feature is enabled.
  //
  // This is set as a default value, but the origin trial uses a different
  // mechanism to enable the feature per origin. When the feature is enabled by
  // the origin trial, ServiceWorkerVersion in content/browser should contain
  // the origin trial token. If the browser successfully confirm the token,
  // fetch handlers are always bypassed regardless of
  // ServiceWorkerBypassFetchHandlerStrategy.
  kFeatureOptIn,
};
CONTENT_EXPORT extern const base::FeatureParam<
    ServiceWorkerBypassFetchHandlerStrategy>
    kServiceWorkerBypassFetchHandlerStrategy;
enum class ServiceWorkerBypassFetchHandlerTarget {
  // Bypass fetch handlers for main resource (navigation) requests. Fetch
  // handlers will be bypassed regardless of the current ServiceWorker running
  // status.
  kMainResource,
  // If the ServiceWorker is not started yet when the main resource request
  // happens, it bypasses fetch handlers for the main resource and subsequent
  // subresources. If the ServiceWorker is running, it invokes fetch handlers as
  // usual.
  kAllOnlyIfServiceWorkerNotStarted,
  // BestEffortServiceWorker(crbug.com/1420517). It allows the browser to
  // dispatch a request directly to the network even if there is a registered
  // ServiceWorker. This behavior races the network request and the
  // ServiceWorker fetch handler and uses the result of whichever is faster.
  kAllWithRaceNetworkRequest,
  // Bypass fetch handlers for subresource requests. Fetch handlers will be
  // bypassed regardless of the current ServiceWorker running status.
  kSubResource,
};
CONTENT_EXPORT extern const base::FeatureParam<
    ServiceWorkerBypassFetchHandlerTarget>
    kServiceWorkerBypassFetchHandlerTarget;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerSkipIgnorableFetchHandler);
CONTENT_EXPORT extern const base::FeatureParam<bool> kSkipEmptyFetchHandler;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kStartServiceWorkerForEmptyFetchHandler;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kAsyncStartServiceWorkerForEmptyFetchHandler;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kAsyncStartServiceWorkerForEmptyFetchHandlerDurationInMs;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStaticRouter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaCaptureOnFocus);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebLockScreenApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTP);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpareRendererForSitePerProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStrictOriginIsolation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSurfaceSyncFullscreenKillswitch);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSyntheticPointerActions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchDragAndContextMenu);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const base::FeatureParam<int>
    kTouchDragMovementThresholdDip;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUnrestrictedSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserActivationSameOriginVisibility);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kVerifyDidCommitParams);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kViewportSegments);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kV8VmFuture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAppSystemMediaControlsWin);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyBaseline);
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableExperimentalWebAssemblyJSPI);
#endif  // defined(ARCH_CPU_X86_64)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyGarbageCollection);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyLazyCompilation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyRelaxedSimd);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyStringref);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTrapHandler);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetooth);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothNewPermissionsBackend);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebMidi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOtpBackendAuto);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebPayments);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUICodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUsb);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebXr);

#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityIncludeLongClickAction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPageZoom);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kAccessibilityPageZoomOSLevelAdjustment;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibilityV2);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGinJavaBridgeMojo);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReduceGpuPriorityOnBackground);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMouseAndTrackpadDropdownMenu);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteAdditions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteWindowSetting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSelectionMenuItemModification);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSmartZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSynchronousCompositorBackgroundSignal);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaScreenCapturing);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebNfc);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacAllowBackgroundingRenderProcesses);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacSyscallSandbox);
#endif  // BUILDFLAG(IS_MAC)

#if defined(WEBRTC_USE_PIPEWIRE)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcPipeWireCapturer);
#endif  // defined(WEBRTC_USE_PIPEWIRE)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForOutOfProcess();
CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForBrowserProcess();

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
