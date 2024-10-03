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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAndroidDragDropOopif);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheTimeToLiveControl);
BASE_DECLARE_FEATURE(kBeforeUnloadBrowserResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromUnknown);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBrowserVerifiedUserActivationKeyboard);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCapturedSurfaceControlTemporaryZoom);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCanvas2DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeClipPathAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCodeCacheDeletionWithoutFilter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCriticalClientHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDesktopCaptureChangeSource);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDeviceMonitorMac);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicyNegotiation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableBackForwardCacheForScreenReader);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kEnableBackForwardCacheForOngoingSubframeNavigation);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableDevToolsJsErrorReporting);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEmbeddingRequiresOptIn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalContentSecurityPolicyFeatures);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmFlexibleFields);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmIdAssertionCORS);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSameSiteLax);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSameSiteNone);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFilterInstalledAppsWebAppMatching);
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFilterInstalledAppsWinMatching);
#endif  // BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeLimitNumAuctions);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeLimitNumAuctionsParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeUseInterestGroupCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeDelayPostAuctionInterestGroupUpdate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeSellerWorkletThreadPool);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeSellerWorkletThreadPoolSize;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeBidderWorkletThreadPool);
CONTENT_EXPORT extern const base::FeatureParam<double>
    kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor;
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFocusRenderWidgetHostViewAndroidOnActionDown);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontSrcLocalMatching);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontIndexingFontations);
#endif
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kForwardMemoryPressureEventsToGpuProcess);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFrameRoutingCache);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFrameRoutingCacheResponseSize;
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGpuInfoCollectionSeparatePrefetch);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGroupNIKByJoiningOrigin);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHandleChildThreadTypeChangesInBrowser);
#endif
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHidePastePopupOnGSB);
#endif

BASE_DECLARE_FEATURE(kHistoryInterventionSameDocumentFix);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kInMemoryCodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInnerFrameCompositorSurfaceEviction);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInterestGroupUpdateIfOlderThan);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceCapturer);
#endif
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMacWebContentsOcclusion);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaDevicesSystemMonitorCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaStreamTrackTransfer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoDedicatedThread);
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kOptimizeImmHideCalls);
#endif
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPermissionsPolicyVerificationInContent);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadingConfig);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPriorityOverridePendingViews);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsM1Override);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessReuseOnPrerenderCOOPSwap);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithDefaultSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessSharingWithStrictSiteInstances);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReloadHiddenTabsWithCrashedSubframes);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kRunStableVideoDecoderFactoryProcessServiceOnIOThread);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kContinueGestureOnLosingFocus);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kResourceTimingForCancelledNavigationInFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRemoveRendererProcessLimit);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReuseInitialRenderFrameHostForWebUI);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerAutoPreload);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerAvoidMainThreadForInitialization);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterStartServiceWorker);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerBypassFetchHandlerHashStrings);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerStartup);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTextInputClient);
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTextInputClientIPCTimeout;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTrustedTypesFromLiteral);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWarmUpNetworkProcess);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyDynamicTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTPAssertionFeaturePolicy);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebRtcUseGpuMemoryBufferVideoFrames);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWindowOpenFileSelectFix);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kScrollBubblingFix);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLimitCrossOriginNonActivatedPaintHolding);

// Please keep features in alphabetical order.

}  // namespace features

#endif  // CONTENT_COMMON_FEATURES_H_
