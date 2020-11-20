// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const base::Feature kAllowActivationDelegationAttr;
CONTENT_EXPORT extern const base::Feature
    kAllowContentInitiatedDataUrlNavigations;
CONTENT_EXPORT extern const base::Feature kAllowPopupsDuringPageUnload;
CONTENT_EXPORT extern const base::Feature kAndroidDownloadableFontsMatching;
CONTENT_EXPORT extern const base::Feature
    kAllowSignedHTTPExchangeCertsWithoutExtension;
CONTENT_EXPORT extern const base::Feature kAudioServiceLaunchOnStartup;
CONTENT_EXPORT extern const base::Feature kAudioServiceOutOfProcess;
CONTENT_EXPORT extern const base::Feature kAudioServiceSandbox;
CONTENT_EXPORT extern const base::Feature kBackgroundFetch;
CONTENT_EXPORT extern const base::Feature kBackForwardCache;
CONTENT_EXPORT extern const base::Feature kBackForwardCacheMemoryControl;
CONTENT_EXPORT extern const base::Feature kBlockCredentialedSubresources;
CONTENT_EXPORT extern const base::Feature kBlockInsecurePrivateNetworkRequests;
CONTENT_EXPORT extern const base::Feature kBrowserUseDisplayThreadPriority;
CONTENT_EXPORT extern const base::Feature
    kBrowserVerifiedUserActivationKeyboard;
CONTENT_EXPORT extern const base::Feature kBrowserVerifiedUserActivationMouse;
CONTENT_EXPORT extern const base::Feature kCacheInlineScriptCode;
CONTENT_EXPORT extern const base::Feature kCacheStorageParallelOps;
CONTENT_EXPORT extern const base::Feature kCanvas2DImageChromium;
CONTENT_EXPORT extern const base::Feature kCanvasOopRasterization;
CONTENT_EXPORT extern const base::Feature kClickPointerEvent;
CONTENT_EXPORT extern const base::Feature kCodeCacheDeletionWithoutFilter;
CONTENT_EXPORT extern const base::Feature kConsolidatedMovementXY;
CONTENT_EXPORT extern const base::Feature kConversionMeasurement;
CONTENT_EXPORT extern const base::Feature kCookieDeprecationMessages;
CONTENT_EXPORT extern const base::Feature kCooperativeScheduling;
CONTENT_EXPORT extern const base::Feature kCrashReporting;
CONTENT_EXPORT extern const base::Feature kDataSaverHoldback;
CONTENT_EXPORT extern const base::Feature kDesktopCaptureChangeSource;
CONTENT_EXPORT extern const base::Feature kDocumentPolicy;
CONTENT_EXPORT extern const base::Feature kDocumentPolicyNegotiation;
CONTENT_EXPORT extern const base::Feature kEnableNewCanvas2DAPI;
CONTENT_EXPORT extern const base::Feature kEnumerateDevicesHideDeviceIDs;
CONTENT_EXPORT extern const base::Feature kExperimentalAccessibilityLabels;
CONTENT_EXPORT extern const base::Feature kExperimentalProductivityFeatures;
CONTENT_EXPORT extern const base::Feature kExpensiveBackgroundTimerThrottling;
CONTENT_EXPORT extern const base::Feature
    kExtraSafelistedRequestHeadersForOutOfBlinkCors;
CONTENT_EXPORT extern const base::Feature kFeaturePolicyForClientHints;
CONTENT_EXPORT extern const base::Feature kFeaturePolicyForSandbox;
CONTENT_EXPORT extern const base::Feature kFirstScrollLatencyMeasurement;
CONTENT_EXPORT extern const base::Feature kFontSrcLocalMatching;
#if !defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature
    kForwardMemoryPressureEventsToGpuProcess;
#endif
CONTENT_EXPORT extern const base::Feature kFractionalScrollOffsets;
CONTENT_EXPORT extern const base::Feature kGreaseUACH;
CONTENT_EXPORT extern const base::Feature kHistoryManipulationIntervention;
CONTENT_EXPORT extern const base::Feature kHistoryPreventSandboxedNavigation;
CONTENT_EXPORT extern const base::Feature kIdleDetection;
CONTENT_EXPORT extern const base::Feature kInstalledApp;
CONTENT_EXPORT extern const base::Feature kInstalledAppProvider;
CONTENT_EXPORT extern const base::Feature kInstalledAppsInCbd;
CONTENT_EXPORT extern const base::Feature kIsolateOrigins;
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT extern const base::Feature kLangClientHintHeader;
CONTENT_EXPORT extern const base::Feature kLazyFrameLoading;
CONTENT_EXPORT extern const base::Feature kLazyFrameVisibleLoadTimeMetrics;
CONTENT_EXPORT extern const base::Feature kLazyImageLoading;
CONTENT_EXPORT extern const base::Feature kLazyImageVisibleLoadTimeMetrics;
CONTENT_EXPORT extern const base::Feature kLazyInitializeMediaControls;
CONTENT_EXPORT extern const base::Feature kLegacyWindowsDWriteFontFallback;
CONTENT_EXPORT extern const base::Feature kLogJsConsoleMessages;
CONTENT_EXPORT extern const base::Feature kLowPriorityIframes;
CONTENT_EXPORT extern const base::Feature
    kMbiDetachAgentSchedulingGroupFromChannel;
CONTENT_EXPORT extern const base::Feature kMediaDevicesSystemMonitorCache;
CONTENT_EXPORT extern const base::Feature kMojoDedicatedThread;
CONTENT_EXPORT extern const base::Feature kMojoVideoCapture;
CONTENT_EXPORT extern const base::Feature kMojoVideoCaptureSecondary;
CONTENT_EXPORT extern const base::Feature kMouseSubframeNoImplicitCapture;
CONTENT_EXPORT extern const base::Feature kNetworkQualityEstimatorWebHoldback;
CONTENT_EXPORT extern const base::Feature kNetworkServiceInProcess;
CONTENT_EXPORT extern const base::Feature kNeverSlowMode;
CONTENT_EXPORT extern const base::Feature kNoStatePrefetchUsingPrefetchLoader;
CONTENT_EXPORT extern const base::Feature kNotificationContentImage;
CONTENT_EXPORT extern const base::Feature kNotificationTriggers;
CONTENT_EXPORT extern const base::Feature kOriginIsolationHeader;
CONTENT_EXPORT extern const base::Feature kOriginPolicy;
CONTENT_EXPORT extern const base::Feature kOverscrollHistoryNavigation;
CONTENT_EXPORT extern const base::Feature kPeriodicBackgroundSync;
CONTENT_EXPORT extern const base::Feature kPermissionsPolicyHeader;
CONTENT_EXPORT extern const base::Feature kPepper3DImageChromium;
CONTENT_EXPORT extern const base::Feature kPepperCrossOriginRedirectRestriction;
CONTENT_EXPORT extern const base::Feature kHighPriorityBeforeUnload;
CONTENT_EXPORT extern const base::Feature kPrioritizeBootstrapTasks;
CONTENT_EXPORT extern const base::Feature kProactivelySwapBrowsingInstance;
CONTENT_EXPORT extern const base::Feature
    kProcessSharingWithDefaultSiteInstances;
CONTENT_EXPORT extern const base::Feature
    kProcessSharingWithStrictSiteInstances;
CONTENT_EXPORT extern const base::Feature kPushSubscriptionChangeEvent;
CONTENT_EXPORT extern const base::Feature kDirectSockets;
CONTENT_EXPORT extern const base::Feature
    kRelaxIsolatedWorldCorsInFileUrlLoaderFactory;
CONTENT_EXPORT extern const base::Feature kReloadHiddenTabsWithCrashedSubframes;
CONTENT_EXPORT extern const base::Feature kRenderDocument;
CONTENT_EXPORT extern const base::Feature kRequestUnbufferedDispatch;
CONTENT_EXPORT extern const base::Feature kRespectMacLCDTextSetting;
CONTENT_EXPORT extern const base::Feature
    kRunVideoCaptureServiceInBrowserProcess;
CONTENT_EXPORT extern const base::Feature kSavePageAsWebBundle;
CONTENT_EXPORT extern const base::Feature kSecurePaymentConfirmationDebug;
CONTENT_EXPORT extern const base::Feature
    kSendBeaconThrowForBlobWithNonSimpleType;
CONTENT_EXPORT extern const base::Feature kServiceWorkerOnUI;
CONTENT_EXPORT extern const base::Feature kServiceWorkerPaymentApps;
CONTENT_EXPORT extern const base::Feature kServiceWorkerPrefersUnusedProcess;
CONTENT_EXPORT extern const base::Feature
    kServiceWorkerTerminationOnNoControllee;
CONTENT_EXPORT extern const base::Feature kSharedArrayBuffer;
CONTENT_EXPORT extern const base::Feature
    kSignedExchangePrefetchCacheForNavigations;
CONTENT_EXPORT extern const base::Feature
    kSignedExchangeReportingForDistributors;
CONTENT_EXPORT extern const base::Feature kSignedExchangeSubresourcePrefetch;
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchange;
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchangePingValidity;
CONTENT_EXPORT extern const base::Feature
    kSiteIsolationEnforcementForFileSystemApi;
CONTENT_EXPORT extern const base::Feature kSmsReceiver;
CONTENT_EXPORT extern const base::Feature kSpareRendererForSitePerProcess;
CONTENT_EXPORT extern const base::Feature kStoragePressureUI;
CONTENT_EXPORT extern const base::Feature kStorageServiceOutOfProcess;
CONTENT_EXPORT extern const base::Feature kStrictOriginIsolation;
CONTENT_EXPORT extern const base::Feature kSubresourceWebBundles;
CONTENT_EXPORT extern const base::Feature kSyntheticPointerActions;
CONTENT_EXPORT extern const base::Feature kTimerThrottlingForHiddenFrames;
CONTENT_EXPORT extern const base::Feature kTopLevelAwait;
CONTENT_EXPORT extern const base::Feature kTouchpadAsyncPinchEvents;
CONTENT_EXPORT extern const base::Feature kTouchpadOverscrollHistoryNavigation;
CONTENT_EXPORT extern const base::Feature kTrustedDOMTypes;
CONTENT_EXPORT extern const base::Feature kUseFramePriorityInRenderProcessHost;
CONTENT_EXPORT extern const base::Feature kUserActivationSameOriginVisibility;
CONTENT_EXPORT extern const base::Feature kUserAgentClientHint;
CONTENT_EXPORT extern const base::Feature kVideoPlaybackQuality;
CONTENT_EXPORT extern const base::Feature kV8VmFuture;
CONTENT_EXPORT extern const base::Feature kWebAppManifestDisplayOverride;
CONTENT_EXPORT extern const base::Feature kWebAssemblyBaseline;
CONTENT_EXPORT extern const base::Feature kWebAssemblyLazyCompilation;
CONTENT_EXPORT extern const base::Feature kWebAssemblySimd;
CONTENT_EXPORT extern const base::Feature kWebAssemblyThreads;
CONTENT_EXPORT extern const base::Feature kWebAssemblyTiering;
CONTENT_EXPORT extern const base::Feature kWebAssemblyTrapHandler;
CONTENT_EXPORT extern const base::Feature kWebAuth;
CONTENT_EXPORT extern const base::Feature kWebAuthCable;
CONTENT_EXPORT extern const base::Feature kWebBluetoothNewPermissionsBackend;
CONTENT_EXPORT extern const base::Feature kWebBundles;
CONTENT_EXPORT extern const base::Feature kWebBundlesFromNetwork;
CONTENT_EXPORT extern const base::Feature kWebContentsOcclusion;
CONTENT_EXPORT extern const base::Feature kWebGLImageChromium;
CONTENT_EXPORT extern const base::Feature kWebOtpBackend;
CONTENT_EXPORT extern const base::Feature kWebPayments;
CONTENT_EXPORT extern const base::Feature kWebPaymentsMinimalUI;
CONTENT_EXPORT extern const base::Feature kWebRtcEcdsaDefault;
CONTENT_EXPORT extern const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames;
CONTENT_EXPORT extern const base::Feature kWebUIReportOnlyTrustedTypes;
CONTENT_EXPORT extern const base::Feature kWebUsb;
CONTENT_EXPORT extern const base::Feature kWebXr;
CONTENT_EXPORT extern const base::Feature kWebXrArModule;
CONTENT_EXPORT extern const base::Feature kWebXrHitTest;
CONTENT_EXPORT extern const base::Feature kWebXrIncubations;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kAndroidAutofillAccessibility;
CONTENT_EXPORT extern const base::Feature
    kBackgroundMediaRendererHasModerateBinding;
CONTENT_EXPORT extern const base::Feature kForce60HzRefreshRate;
CONTENT_EXPORT extern const base::Feature kWarmUpNetworkProcess;
CONTENT_EXPORT extern const base::Feature kWebNfc;
#endif  // defined(OS_ANDROID)

#if defined(OS_MAC)
CONTENT_EXPORT extern const base::Feature kDeviceMonitorMac;
CONTENT_EXPORT extern const base::Feature kIOSurfaceCapturer;
CONTENT_EXPORT extern const base::Feature kMacSyscallSandbox;
CONTENT_EXPORT extern const base::Feature kMacV2GPUSandbox;
CONTENT_EXPORT extern const base::Feature kRetryGetVideoCaptureDeviceInfos;
#endif  // defined(OS_MAC)

#if defined(WEBRTC_USE_PIPEWIRE)
CONTENT_EXPORT extern const base::Feature kWebRtcPipeWireCapturer;
#endif  // defined(WEBRTC_USE_PIPEWIRE)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForOutOfProcess();
CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForBrowserProcess();

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
