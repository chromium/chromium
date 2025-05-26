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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAvoidUnnecessaryBeforeUnloadCheckSync);
// Please check the code comment on
// ContentBrowserClient::SupportsAvoidUnnecessaryBeforeUnloadCheckSync() in the
// header file for the context (See: https://crbug.com/396998476).
enum class AvoidUnnecessaryBeforeUnloadCheckSyncMode {
  // Enable DumpWithoutCrashing code for beforeunload investigation.
  kDumpWithoutCrashing,
  // The following mode is mostly the same as the original
  // kAvoidUnnecessaryBeforeUnloadCheckSync feature that sky@ experimented in
  // the past (Ref: https://crbug.com/40361673, https://crbug.com/396998476).
  //
  // The significant difference is that this mode still relies on
  // RenderFrameHostImpl::SendBeforeUnload() in for_legacy mode.
  //
  // This means both the control and enabled groups will adjust the
  // common_params start time. This is more consistent than sky@'s original
  // experiment and will show an improvement on metrics like FCP, but the
  // improvement will look inflated because the denominator (i.e., how long the
  // navigation was to begin with) was incorrectly too small.  We think this
  // could be fine as long as we don't use FCP or similar metrics based on the
  // common_params start time to judge the size of the improvement.
  //
  // Using Navigation.Timeline.TotalExcludingBeforeUnload.Duration should give
  // us a better picture of how much skipping the PostTask helps.
  kWithSendBeforeUnload,
  // When this mode is specified, the navigation will synchronously continue if
  // it knows beforeunload handlers are not registered.
  kWithoutSendBeforeUnload,
};
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    AvoidUnnecessaryBeforeUnloadCheckSyncMode,
    kAvoidUnnecessaryBeforeUnloadCheckSyncMode);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheTimeToLiveControl);
BASE_DECLARE_FEATURE(kBeforeUnloadBrowserResponseQueue);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kBlockInsecurePrivateNetworkRequestsFromUnknown);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCanvas2DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCDPScreenshotNewSurface);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeClipPathAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCodeCacheDeletionWithoutFilter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCommittedOriginEnforcements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCommittedOriginTracking);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCriticalClientHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicyNegotiation);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableDevToolsJsErrorReporting);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEmbeddingRequiresOptIn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalContentSecurityPolicyFeatures);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmUseOtherAccountAndLabelsNewSyntax);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmSameSiteLax);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFilterInstalledAppsWebAppMatching);
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFilterInstalledAppsWinMatching);
#endif  // BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeLimitNumAuctions);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeLimitNumAuctionsParam;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeDelayPostAuctionInterestGroupUpdate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeSellerWorkletThreadPool);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFledgeSellerWorkletThreadPoolSize;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeBidderWorkletThreadPool);
CONTENT_EXPORT extern const base::FeatureParam<double>
    kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor;
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFledgeAndroidWorkletOffMainThread);
#endif

CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontSrcLocalMatching);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFrameRoutingCache);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kFrameRoutingCacheResponseSize;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGroupNIKByJoiningOrigin);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHidePastePopupOnGSB);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kHoldbackDebugReasonStringRemoval);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kInMemoryCodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInterestGroupUpdateIfOlderThan);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceCapturer);
#endif
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRendererProcessLimitOnAndroid);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                          kRendererProcessLimitOnAndroidCount);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaDevicesSystemMonitorCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaStreamTrackTransfer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoDedicatedThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMultipleSpareRPHs);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kMultipleSpareRPHsCount);
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPermissionsPolicyVerificationInContent);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadingConfig);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrerenderMoreCorrectSpeculativeRFHCreation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPriorityOverridePendingViews);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAdsAPIsM1Override);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProcessReuseOnPrerenderCOOPSwap);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kProgressiveAccessibility);
enum class ProgressiveAccessibilityMode {
  // Application of mode flags is deferred for hidden WebContents, but otherwise
  // never cleared.
  kOnlyEnable,

  // Application of mode flags is deferred for hidden WebContents, and mode
  // flags are cleared after a WebContents is hidden.
  kDisableOnHide,
};
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(ProgressiveAccessibilityMode,
                                          kProgressiveAccessibilityModeParam);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReloadHiddenTabsWithCrashedSubframes);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRestrictOrientationLockToPhones);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kContinueGestureOnLosingFocus);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRemoveRendererProcessLimit);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerAvoidMainThreadForInitialization);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerBypassFetchHandlerHashStrings);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerSrcdocSupport);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterStartServiceWorker);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerClientUrlIsCreationUrl);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame);
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipGrantAccessToDataPathIfAlreadySet);
#endif
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTextInputClient);
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTextInputClientIPCTimeout;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTrustedTypesFromLiteral);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kValidateNetworkServiceProcessIdentity);
#endif
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWarmUpNetworkProcess);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyDynamicTiering);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebOTPAssertionFeaturePolicy);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kWebUIInProcessResourceLoading);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLimitCrossOriginNonActivatedPaintHolding);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kDisallowRasterInterfaceWithoutSkiaBackend);

// Please keep features in alphabetical order.

}  // namespace features

#endif  // CONTENT_COMMON_FEATURES_H_
