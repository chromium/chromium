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
#include "tools/v8_context_snapshot/buildflags.h"

namespace features {

// BEFORE MODIFYING THIS FILE: If your feature is only used inside content/, add
// your feature in `content/common/features.h` instead.

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAdditionalOpaqueOriginEnforcements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidWarmUpSpareRendererWithTimeout);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kAndroidSpareRendererCreationTiming;
inline constexpr const char kAndroidSpareRendererCreationAfterLoading[] =
    "after-loading";
inline constexpr const char kAndroidSpareRendererCreationAfterFirstPaint[] =
    "after-first-paint";
inline constexpr const char
    kAndroidSpareRendererCreationDelayedDuringLoading[] =
        "delayed-during-loading";
CONTENT_EXPORT extern const base::FeatureParam<int>
    kAndroidSpareRendererCreationDelayMs;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kAndroidSpareRendererTimeoutSeconds;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceLaunchOnStartup);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceOutOfProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAudioServiceSandbox);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundFetch);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheEntryTimeout);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMemoryControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheMediaSessionService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBatterySaverModeAlignWakeUps);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBlockInsecurePrivateNetworkRequests);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromPrivate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsDeprecationTrial);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBrokerFileOperationsOnDiskCacheInNetworkService);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationMouse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCacheControlNoStoreEnterBackForwardCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCapturedSurfaceControlStickyPermissions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCapturedSurfaceControlKillswitch);
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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDeferSpeculativeRFHCreation);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kWarmupSpareProcessCreationWhenDeferRFH;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kCreateSpeculativeRFHFilterRestore;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kCreateSpeculativeRFHDelayMs;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDevToolsPrivacyUI);
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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDIPSPreservePSData);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebContentsDiscard);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kDisconnectExtensionMessagePortWhenPageEntersBFCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDrawCutoutEdgeToEdge);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEarlyEstablishGpuChannel);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableCanvas2DLayers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeScheme);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableServiceWorkersForChromeUntrusted);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnsureExistingRendererAlive);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCm);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmAuthz);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmButtonMode);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdPRegistration);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdpSigninStatusEnabled);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMetricsEndpoint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmMultipleIdentityProviders);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSelectiveDisclosure);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmUseOtherAccount);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmWithoutWellKnownEnforcement);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFencedFramesEnforceFocus);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebIdentityDigitalCredentials);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFractionalScrollOffsets);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGreaseUACH);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIdbPrioritizeForegroundClients);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIgnoreDuplicateNavs);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                          kDuplicateNavThreshold);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledApp);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInstalledAppProvider);
// LINT.IfChange
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolatedWebApps);
// LINT.ThenChange(//PRESUBMIT.py)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAutomaticFullscreenContentSetting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateFencedFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIsolateOrigins);
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyInitializeMediaControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyTechReportEnableCookieIssueReports);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLegacyWindowsDWriteFontFallback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLogJsConsoleMessages);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMainThreadCompositingPriority);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMBIMode);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcHWDecoding);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcHWEncoding);

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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationNetworkResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkQualityEstimatorWebHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceInProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationContentImage);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNotificationTriggers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNoStatePrefetchHoldback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOriginIsolationHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPartitionedPopinsHeaderPolicyBypass);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPdfEnforcements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPeriodicBackgroundSync);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFeaturePolicyHeader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPepperCrossOriginRedirectRestriction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPersistentOriginTrials);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchBrowserInitiatedTriggers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsOverride);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivateNetworkAccessForNavigationsWarningOnly);
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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRestrictThreadPoolInBackground);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSetIsolatesPriority);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRetryGetVideoCaptureDeviceInfos);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessPerSiteUpToMainFrameThreshold);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kProcessPerSiteMainFrameThreshold;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kProcessPerSiteMainFrameAllowIPAndLocalhost;
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kProcessPerSiteMainFrameAllowDevToolsAttached;
CONTENT_EXPORT extern const base::FeatureParam<double>
    kProcessPerSiteMainFrameSiteScalingFactor;
CONTENT_EXPORT extern const base::FeatureParam<double>
    kProcessPerSiteMainFrameTotalMemoryLimit;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRunVideoCaptureServiceInBrowserProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSchedQoSOnResourcedForChrome);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSecurePaymentConfirmationDebug);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerPaymentApps);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedArrayBufferOnDesktop);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kShouldAllowFirstPartyStorageKeyOverrideFromEmbedder);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteInstanceGroupsForDataUrls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationForCrossOriginOpenerPolicy);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam;
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkiaFontService);
enum class SkiaFontServiceTypefaceType {
  kDwrite,
  kFreetype,
};
extern const base::FeatureParam<SkiaFontServiceTypefaceType>
    kSkiaFontServiceTypefaceType;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUtilityWithUiPumpInitializesCom);
#endif  // BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDisableProcessReuse);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStaticRouter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaCaptureOnFocus);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebLockScreenApi);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTP);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpareRendererForSitePerProcess);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStrictOriginIsolation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSubframeProcessReuseThresholds);
CONTENT_EXPORT extern const base::FeatureParam<double>
    kSubframeProcessReuseMemoryThreshold;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSuppressDifferentOriginSubframeJSDialogs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSyntheticPointerActions);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchDragAndContextMenu);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUseBrowserCalculatedOrigin);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const base::FeatureParam<int>
    kTouchDragMovementThresholdDip;
#endif
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUseContextSnapshot);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUnrestrictedSharedArrayBuffer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserActivationSameOriginVisibility);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kVerifyDidCommitParams);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kV8VmFuture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAppSystemMediaControls);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyBaseline);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableExperimentalWebAssemblyJSPI);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyLazyCompilation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyMemory64);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTrapHandler);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetooth);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothNewPermissionsBackend);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOtpBackendAuto);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebPayments);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUICodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUsb);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebXr);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebPermissionsApi);

#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityIncludeLongClickAction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPageZoom);
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kAccessibilityPageZoomOSLevelAdjustment;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPageZoomEnhancements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityPageZoomV2);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAccessibilityUnifiedSnapshots);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kAccessibilityManageBroadcastReceiverOnBackground);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidOpenPdfInline);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kGinJavaBridgeMojoSkipClearObjectsOnMainDocumentReady);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReduceGpuPriorityOnBackground);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMouseAndTrackpadDropdownMenu);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSelectionMenuItemModification);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSmartZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserMediaScreenCapturing);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebNfc);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebViewSuppressTapDuringFling);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacAllowBackgroundingRenderProcesses);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacImeLiveConversionFix);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacSyscallSandbox);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSonomaAccessibilityActivationRefinements);
#endif  // BUILDFLAG(IS_MAC)

#if defined(WEBRTC_USE_PIPEWIRE)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcPipeWireCapturer);
#endif  // defined(WEBRTC_USE_PIPEWIRE)

// Number of days to "store" IPH guardrails for navigation captured app launches
// till they are cleared.
CONTENT_EXPORT extern const base::FeatureParam<int>
    kNavigationCapturingIPHGuardrailStorageDuration;

// Enables user link capturing on all desktop platforms.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPwaNavigationCapturing);
enum class CapturingState {
  kDefaultOn = 0,
  kDefaultOff = 1,
  kReimplDefaultOn = 2,
  kReimplDefaultOff = 3
};
// If links should be captured by apps by default.
CONTENT_EXPORT extern const base::FeatureParam<CapturingState>
    kNavigationCapturingDefaultState;

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForOutOfProcess();
CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForBrowserProcess();

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
