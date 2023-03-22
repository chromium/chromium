// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAllowContentInitiatedDataUrlNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidDownloadableFontsMatching);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceLaunchOnStartup);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceOutOfProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceSandbox);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundFetch);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kResourceTimingForCancelledNavigationInFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheEntryTimeout);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMemoryControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheTimeToLiveControl);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMediaSessionService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardTransitions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBlockInsecurePrivateNetworkRequests);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromPrivate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromUnknown);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsDeprecationTrial);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsForNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBrokerFileOperationsOnDiskCacheInNetworkService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationKeyboard);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationMouse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCanvas2DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kClearCrossSiteCrossBrowsingContextGroupWindowName);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeBGColorAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeClipPathAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCodeCacheDeletionWithoutFilter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kConsolidatedMovementXY);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCooperativeScheduling);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCrashReporting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCriticalClientHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDebugHistoryInterventionNoUserActivation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDesktopCaptureChangeSource);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDesktopCaptureLacrosV2);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDesktopPWAsTabStrip);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDevicePosture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDigitalGoodsApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicy);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicyNegotiation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEarlyEstablishGpuChannel);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEmbeddingRequiresOptIn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableBackForwardCacheForScreenReader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableCanvas2DLayers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kEnableMachineLearningModelLoaderWebPlatformApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSupportPepperVideoDecoderDevAPI);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeScheme);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeUntrusted);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnumerateDevicesHideDeviceIDs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalAccessibilityLabels);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalContentSecurityPolicyFeatures);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kExtraSafelistedRequestHeadersForOutOfBlinkCors);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCm);
CONTENT_EXPORT extern const char kFedCmIdpSignoutFieldTrialParamName[];
CONTENT_EXPORT extern const char kFedCmIdpSigninStatusFieldTrialParamName[];
CONTENT_EXPORT extern const char
    kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName[];
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmAutoReauthn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdPRegistration);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIframeSupport);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMetricsEndpoint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMultipleIdentityProviders);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmRpContext);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmUserInfo);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSelectiveDisclosure);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmLoginHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebIdentityMDocs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFirstPartySets);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kFirstPartySetsClearSiteDataOnChangedSets;
CONTENT_EXPORT extern const base::FeatureParam<bool> kFirstPartySetsIsDogfooder;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFirstPartySetsMaxAssociatedSites;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kFirstPartySetsNavigationThrottleTimeout;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontSrcLocalMatching);
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kForwardMemoryPressureEventsToGpuProcess);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeLimitNumAuctions);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeLimitNumAuctionsParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFractionalScrollOffsets);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGetDisplayMediaSet);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGetDisplayMediaSetAutoSelectAllScreens);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGreaseUACH);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHandleRendererThreadTypeChangesInBrowser);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIdleDetection);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInMemoryCodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInnerFrameCompositorSurfaceEviction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledApp);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledAppProvider);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolatedWebApps);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateFencedFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateOrigins);
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT BASE_DECLARE_FEATURE(kJavaScriptArrayGrouping);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kJavaScriptExperimentalSharedMemory);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIwaControlledFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyFrameLoading);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyImageLoading);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyImageVisibleLoadTimeMetrics);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyInitializeMediaControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyWindowsDWriteFontFallback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLogJsConsoleMessages);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLowerV8MemoryLimitForNonMainRenderers);
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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaDevicesSystemMonitorCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaStreamTrackTransfer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoDedicatedThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoVideoCapture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoVideoCaptureSecondary);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMouseSubframeNoImplicitCapture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationNetworkResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationRequestPreconnect);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkQualityEstimatorWebHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceInProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationContentImage);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationTriggers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNoStatePrefetchHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOriginIsolationHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPeriodicBackgroundSync);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFeaturePolicyHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPepper3DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPepperCrossOriginRedirectRestriction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPersistentOriginTrials);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHighPriorityBeforeUnload);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadCookies);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2Holdback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadingHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsOverride);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForWorkers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForWorkersWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivateNetworkAccessRespectPreflightResults);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessSendPreflights);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessPermissionPrompt);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProactivelySwapBrowsingInstance);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithDefaultSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithStrictSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPushSubscriptionChangeEvent);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReloadHiddenTabsWithCrashedSubframes);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kRenderAccessibilityHostDeserializationOffMainThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRenderDocument);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRetryGetVideoCaptureDeviceInfos);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRunVideoCaptureServiceInBrowserProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmationDebug);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmationRemoveRpField);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerPaymentApps);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBufferOnDesktop);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kShouldAllowFirstPartyStorageKeyOverrideFromEmbedder);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSignedExchangeReportingForDistributors);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSignedHTTPExchange);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationForCrossOriginOpenerPolicy);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationForGuests);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDisableProcessReuse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame);
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
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerSkipIgnorableFetchHandler);
CONTENT_EXPORT extern const base::FeatureParam<bool> kSkipEmptyFetchHandler;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kStartServiceWorkerForEmptyFetchHandler;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kAsyncStartServiceWorkerForEmptyFetchHandler;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaCaptureOnFocus);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebLockScreenApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTP);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTPAssertionFeaturePolicy);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpareRendererForSitePerProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStopVideoCaptureOnScreenLock);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStrictOriginIsolation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSubframeShutdownDelay);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSurfaceSyncFullscreenKillswitch);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSyntheticPointerActions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchDragAndContextMenu);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const base::FeatureParam<int>
    kTouchDragMovementThresholdDip;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadAsyncPinchEvents);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTrustedTypesFromLiteral);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUnrestrictedSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserActivationSameOriginVisibility);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kVerifyDidCommitParams);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kVideoPlaybackQuality);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kV8VmFuture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyBaseline);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyDynamicTiering);
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableExperimentalWebAssemblyJSPI);
#endif  // defined(ARCH_CPU_X86_64)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyGarbageCollection);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyLazyCompilation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyRelaxedSimd);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyStringref);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTrapHandler);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAuthnTouchToFillCredentialSelection);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetooth);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothNewPermissionsBackend);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebGLImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebMidi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOtpBackendAuto);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebPayments);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseGpuMemoryBufferVideoFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUICodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUsb);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebXr);

#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityAsyncTreeConstruction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPageZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAutoDisableAccessibilityV2);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundMediaRendererHasModerateBinding);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBindingManagerConnectionLimit);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReduceGpuPriorityOnBackground);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOnDemandAccessibilityEvents);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteAdditions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteExceptions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRequestDesktopSiteZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaScreenCapturing);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWarmUpNetworkProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebNfc);

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDeviceMonitorMac);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceCapturer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacAllowBackgroundingRenderProcesses);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacSyscallSandbox);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacWebContentsOcclusion);
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
