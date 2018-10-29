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
CONTENT_EXPORT extern const base::Feature
    kAllowSignedHTTPExchangeCertsWithoutExtension;
CONTENT_EXPORT extern const base::Feature kAudioServiceAudioStreams;
CONTENT_EXPORT extern const base::Feature kAudioServiceLaunchOnStartup;
CONTENT_EXPORT extern const base::Feature kAudioServiceOutOfProcess;
CONTENT_EXPORT extern const base::Feature kAwaitOptimization;
CONTENT_EXPORT extern const base::Feature kBackgroundFetch;
CONTENT_EXPORT extern const base::Feature kBackgroundFetchUploads;
CONTENT_EXPORT extern const base::Feature kBlinkHeapIncrementalMarking;
CONTENT_EXPORT extern const base::Feature kBlinkHeapUnifiedGarbageCollection;
CONTENT_EXPORT extern const base::Feature kBloatedRendererDetection;
CONTENT_EXPORT extern const base::Feature kBlockCredentialedSubresources;
CONTENT_EXPORT extern const base::Feature kBrotliEncoding;
CONTENT_EXPORT extern const base::Feature kCacheInlineScriptCode;
CONTENT_EXPORT extern const base::Feature kCanvas2DImageChromium;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueFixedPosition;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueScrollers;
CONTENT_EXPORT extern const base::Feature kCompositorTouchAction;
CONTENT_EXPORT extern const base::Feature kCSSFragmentIdentifiers;
CONTENT_EXPORT extern const base::Feature kDataSaverHoldback;
CONTENT_EXPORT extern const base::Feature kExperimentalProductivityFeatures;
CONTENT_EXPORT extern const base::Feature kExpensiveBackgroundTimerThrottling;
CONTENT_EXPORT extern const base::Feature kExtendedMouseButtons;
CONTENT_EXPORT extern const base::Feature kFontCacheScaling;
CONTENT_EXPORT extern const base::Feature kFontSrcLocalMatching;
CONTENT_EXPORT extern const base::Feature
    kFramebustingNeedsSameOriginOrUserGesture;
CONTENT_EXPORT extern const base::Feature kGamepadExtensions;
CONTENT_EXPORT extern const base::Feature kGamepadVibration;
CONTENT_EXPORT extern const base::Feature kGuestViewCrossProcessFrames;
CONTENT_EXPORT extern const base::Feature kHeapCompaction;
CONTENT_EXPORT extern const base::Feature kImageCaptureAPI;
CONTENT_EXPORT extern const base::Feature kInputPredictorTypeChoice;
CONTENT_EXPORT extern const base::Feature kIsolateOrigins;
CONTENT_EXPORT extern const char kIsolateOriginsFieldTrialParamName[];
CONTENT_EXPORT extern const base::Feature kKeyboardLockAPI;
CONTENT_EXPORT extern const base::Feature kLayeredAPI;
CONTENT_EXPORT extern const base::Feature kLazyFrameLoading;
CONTENT_EXPORT extern const base::Feature kLazyFrameVisibleLoadTimeMetrics;
CONTENT_EXPORT extern const base::Feature kLazyImageLoading;
CONTENT_EXPORT extern const base::Feature kLazyImageVisibleLoadTimeMetrics;
CONTENT_EXPORT extern const base::Feature kLazyInitializeMediaControls;
CONTENT_EXPORT extern const base::Feature kLowPriorityIframes;
CONTENT_EXPORT extern const base::Feature kMediaDevicesSystemMonitorCache;
CONTENT_EXPORT extern const base::Feature kMemoryCoordinator;
CONTENT_EXPORT extern const base::Feature kMimeHandlerViewInCrossProcessFrame;
CONTENT_EXPORT extern const base::Feature kMojoVideoCapture;
CONTENT_EXPORT extern const base::Feature kMojoVideoCaptureSecondary;
CONTENT_EXPORT extern const base::Feature kNetworkQualityEstimatorWebHoldback;
CONTENT_EXPORT extern const base::Feature kNetworkServiceInProcess;
CONTENT_EXPORT extern const base::Feature kNotificationContentImage;
CONTENT_EXPORT extern const base::Feature kOriginPolicy;
CONTENT_EXPORT extern const base::Feature kOriginTrials;
CONTENT_EXPORT extern const base::Feature kOverscrollHistoryNavigation;
CONTENT_EXPORT extern const base::Feature kPageLifecycle;
CONTENT_EXPORT extern const base::Feature kPassiveDocumentEventListeners;
CONTENT_EXPORT extern const base::Feature kPassiveDocumentWheelEventListeners;
CONTENT_EXPORT extern const base::Feature kPassiveEventListenersDueToFling;
CONTENT_EXPORT extern const base::Feature kPdfIsolation;
CONTENT_EXPORT extern const base::Feature kPerNavigationMojoInterface;
CONTENT_EXPORT extern const base::Feature kPepper3DImageChromium;
CONTENT_EXPORT extern const base::Feature kPurgeAndSuspend;
CONTENT_EXPORT extern const base::Feature kRasterInducingScroll;
CONTENT_EXPORT extern const base::Feature kRenderingPipelineThrottling;
CONTENT_EXPORT extern const base::Feature kRequireCSSExtensionForFile;
CONTENT_EXPORT extern const base::Feature kResamplingInputEvents;
CONTENT_EXPORT extern const base::Feature kResourceLoadScheduler;
CONTENT_EXPORT extern const base::Feature
    kRunVideoCaptureServiceInBrowserProcess;
CONTENT_EXPORT extern const base::Feature kScrollAnchorSerialization;
CONTENT_EXPORT extern const base::Feature
    kSendBeaconThrowForBlobWithNonSimpleType;
CONTENT_EXPORT extern const base::Feature kSecMetadata;
CONTENT_EXPORT extern const base::Feature kServiceWorkerLongRunningMessage;
CONTENT_EXPORT extern const base::Feature kServiceWorkerPaymentApps;
CONTENT_EXPORT extern const base::Feature kSharedArrayBuffer;
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchange;
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchangeAcceptHeader;
CONTENT_EXPORT extern const char
    kSignedHTTPExchangeAcceptHeaderFieldTrialParamName[];
CONTENT_EXPORT extern const base::Feature kSignedHTTPExchangeOriginTrial;
CONTENT_EXPORT extern const base::Feature kSpareRendererForSitePerProcess;
CONTENT_EXPORT extern const base::Feature kTimerThrottlingForHiddenFrames;
CONTENT_EXPORT extern const base::Feature kTouchpadAsyncPinchEvents;
CONTENT_EXPORT extern const base::Feature kTouchpadOverscrollHistoryNavigation;
CONTENT_EXPORT extern const base::Feature kUserActivationV2;
CONTENT_EXPORT extern const base::Feature kV8ContextSnapshot;
CONTENT_EXPORT extern const base::Feature kV8LowMemoryModeForSubframes;
CONTENT_EXPORT extern const base::Feature kV8Orinoco;
CONTENT_EXPORT extern const base::Feature kV8VmFuture;
CONTENT_EXPORT extern const base::Feature kWebAssembly;
CONTENT_EXPORT extern const base::Feature kWebAssemblyBaseline;
CONTENT_EXPORT extern const base::Feature kWebAssemblyThreads;
CONTENT_EXPORT extern const base::Feature kWebAssemblyTrapHandler;
CONTENT_EXPORT extern const base::Feature kWebAuth;
CONTENT_EXPORT extern const base::Feature kWebAuthBle;
CONTENT_EXPORT extern const base::Feature kWebAuthCable;
CONTENT_EXPORT extern const base::Feature kWebAuthCableWin;
CONTENT_EXPORT extern const base::Feature kWebAuthGetTransports;
CONTENT_EXPORT extern const base::Feature kWebContentsOcclusion;
CONTENT_EXPORT extern const base::Feature kWebGLImageChromium;
CONTENT_EXPORT extern const base::Feature kWebPayments;
CONTENT_EXPORT extern const base::Feature kWebRtcEcdsaDefault;
CONTENT_EXPORT extern const base::Feature kWebRtcHWH264Encoding;
CONTENT_EXPORT extern const base::Feature kWebRtcHWVP8Encoding;
CONTENT_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
CONTENT_EXPORT extern const base::Feature kWebRtcScreenshareSwEncoding;
CONTENT_EXPORT extern const base::Feature kWebRtcUseEchoCanceller3;
CONTENT_EXPORT extern const base::Feature kWebRtcHybridAgc;
CONTENT_EXPORT extern const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames;
CONTENT_EXPORT extern const base::Feature kWebRtcHideLocalIpsWithMdns;
CONTENT_EXPORT extern const base::Feature kWebUsb;
CONTENT_EXPORT extern const base::Feature kWebVrVsyncAlign;
CONTENT_EXPORT extern const base::Feature kWebXr;
CONTENT_EXPORT extern const base::Feature kWebXrGamepadSupport;
CONTENT_EXPORT extern const base::Feature kWebXrHitTest;
CONTENT_EXPORT extern const base::Feature kWebXrOrientationSensorDevice;
CONTENT_EXPORT extern const base::Feature kWipeCorruptV2IDBDatabases;
CONTENT_EXPORT extern const base::Feature kWorkStealingInScriptRunner;
CONTENT_EXPORT extern const base::Feature kScheduledScriptStreaming;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kAndroidAutofillAccessibility;
CONTENT_EXPORT extern const base::Feature
    kBackgroundMediaRendererHasModerateBinding;
CONTENT_EXPORT extern const base::Feature kDisplayCutoutAPI;
CONTENT_EXPORT extern const base::Feature kHideIncorrectlySizedFullscreenFrames;
CONTENT_EXPORT extern const base::Feature kWebNfc;
CONTENT_EXPORT extern const base::Feature kWebXrRenderPath;
CONTENT_EXPORT extern const char kWebXrRenderPathParamName[];
CONTENT_EXPORT extern const char kWebXrRenderPathParamValueClientWait[];
CONTENT_EXPORT extern const char kWebXrRenderPathParamValueGpuFence[];
CONTENT_EXPORT extern const char kWebXrRenderPathParamValueSharedBuffer[];
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kWebUIPolymer2;
#endif  // !defined(OS_ANDROID)

#if defined(OS_MACOSX)
CONTENT_EXPORT extern const base::Feature kDeviceMonitorMac;
CONTENT_EXPORT extern const base::Feature kIOSurfaceCapturer;
CONTENT_EXPORT extern const base::Feature kMacV2Sandbox;
#endif  // defined(OS_MACOSX)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForOutOfProcess();
CONTENT_EXPORT bool IsVideoCaptureServiceEnabledForBrowserProcess();

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
