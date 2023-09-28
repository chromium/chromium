// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURES_H_
#define CONTENT_COMMON_FEATURES_H_

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {

// Please keep features in alphabetical order.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAllowContentInitiatedDataUrlNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidDownloadableFontsMatching);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheTimeToLiveControl);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundMediaRendererHasModerateBinding);
#endif
BASE_DECLARE_FEATURE(kBeforeUnloadBrowserResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromUnknown);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationKeyboard);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCanvas2DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeClipPathAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCodeCacheDeletionWithoutFilter);
BASE_DECLARE_FEATURE(kConsolidatedIPCForProxyCreation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kConsolidatedMovementXY);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCriticalClientHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDesktopCaptureChangeSource);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDeviceMonitorMac);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicyNegotiation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnumerateDevicesHideDeviceIDs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableBackForwardCacheForScreenReader);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableDevToolsJsErrorReporting);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnsureAllowBindingsIsAlwaysForWebUI);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnumerateDevicesHideDeviceIDs);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEmbeddingRequiresOptIn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalContentSecurityPolicyFeatures);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdpSigninStatusMetrics);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeLimitNumAuctions);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeLimitNumAuctionsParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontSrcLocalMatching);
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kForwardMemoryPressureEventsToGpuProcess);
#endif
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGpuInfoCollectionSeparatePrefetch);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHandleRendererThreadTypeChangesInBrowser);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHighPriorityBeforeUnload);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInMemoryCodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInnerFrameCompositorSurfaceEviction);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceCapturer);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kJavaScriptArrayGrouping);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLazyFrameLoading);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacImeLiveConversionFix);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacWebContentsOcclusion);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaDevicesSystemMonitorCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaStreamTrackTransfer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoDedicatedThread);
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kOptimizeImmHideCalls);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadingConfig);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadCookies);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsM1Override);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateNetworkAccessForIframesWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProactivelySwapBrowsingInstance);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithDefaultSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithStrictSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReloadHiddenTabsWithCrashedSubframes);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kResourceTimingForCancelledNavigationInFrame);
BASE_DECLARE_FEATURE(kRestrictCanAccessDataForOriginToUIThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerAutoPreload);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterStartServiceWorker);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerBypassFetchHandlerHashStrings);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSignedExchangeReportingForDistributors);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationCitadelEnforcement);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerStartup);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStopVideoCaptureOnScreenLock);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTextInputClient);
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTextInputClientIPCTimeout;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadAsyncPinchEvents);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTrustedTypesFromLiteral);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kVideoPlaybackQuality);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWarmUpNetworkProcess);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyDynamicTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebGLImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTPAssertionFeaturePolicy);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseGpuMemoryBufferVideoFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWindowOpenFileSelectFix);

// Please keep features in alphabetical order.

}  // namespace features

#endif  // CONTENT_COMMON_FEATURES_H_
