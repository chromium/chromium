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
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kArabicDigitSubstitution);
#endif
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
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCancelCompositionWhenWindowLosesFocus);
#endif  // BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCanvas2DImageChromium);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCDPScreenshotNewSurface);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCompositeClipPathAnimation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCodeCacheDeletionWithoutFilter);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCommittedOriginEnforcements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCommittedOriginTracking);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCopyFromSurfaceAlwaysCallCallback);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kCriticalClientHint);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kDocumentPolicyNegotiation);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnableDevToolsJsErrorReporting);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnforceSameDocumentOriginInvariants);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEmbeddingRequiresOptIn);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kExperimentalContentSecurityPolicyFeatures);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmUseOtherAccountAndLabelsNewSyntax);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmNonStringToken);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmWellKnownEndpointValidation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmPreservePortsForTesting);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFedCmErrorAttribute);
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

#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontDataServiceAllWebContents);
enum class FontDataServiceTypefaceType {
  kDwrite,
  kFreetype,
  kFontations,
};
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(FontDataServiceTypefaceType,
                                          kFontDataServiceTypefaceType);
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFontDataServiceLinux);
enum class FontDataServiceTypefaceType {
  kFreetype,
  kFontations,
};
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(FontDataServiceTypefaceType,
                                          kFontDataServiceTypefaceType);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
bool IsFontDataServiceEnabled();
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

CONTENT_EXPORT BASE_DECLARE_FEATURE(kIgnoreDuplicateNavsOnlyWithUserGesture);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInMemoryCodeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kInterestGroupUpdateIfOlderThan);
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceCapturer);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kKeepChildProcessAfterIPCReset);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kLocalNetworkAccessForWorkers);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLocalNetworkAccessForWorkersWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLocalNetworkAccessForNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kLocalNetworkAccessForNavigationsWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kLocalNetworkAccessForSubframeNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kLocalNetworkAccessForSubframeNavigationsWarningOnly);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kLocalNetworkAccessForFencedFrameNavigations);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kLocalNetworkAccessForFencedFrameNavigationsWarningOnly);

CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kMainFrameProcessReuseAllowDevToolsAttached);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMainFrameProcessReuseAllowIPAndLocalhost);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaDevicesSystemMonitorCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMediaStreamTrackTransfer);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMojoDedicatedThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kMultipleSpareRPHs);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kMultipleSpareRPHsCount);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationThrottleRegistryAttributeCache);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationThrottleRunner2);
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPermissionsPolicyVerificationInContent);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchCookieIndices);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchDevtoolsUserAgentOverride);
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
CONTENT_EXPORT BASE_DECLARE_FEATURE(kReusePrerenderingProcessForMainFrames);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRestrictOrientationLockToPhones);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kContinueGestureOnLosingFocus);
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRemoveRendererProcessLimit);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kRendererCancellationThrottleImprovements);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kResumeNavigationWithSpeculativeRFHProcessGone);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSendBeaconThrowForBlobWithNonSimpleType);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerAvoidMainThreadForInitialization);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerBypassFetchHandlerHashStrings);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kServiceWorkerBypassFetchHandlerBypassedHashStrings;
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerSrcdocSupport);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerStaticRouterRaceRequestFix2);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterStartServiceWorker);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kServiceWorkerClientUrlIsCreationUrl);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSideBySideFilePickerCancelling);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipEarlyCommitPendingForCrashedFrame);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipRedundantNavigationStateNotification);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSkipRendererCancellationThrottle);
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kStrictHighRankProcessLRU);
#endif
#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTextInputClient);
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTextInputClientIPCTimeout;
#endif
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTouchpadOverscrollHistoryNavigation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kTrustedTypesFromLiteral);
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kUpdateDirectManipulationHelperOnParentChange);
#endif
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

// Please keep features in alphabetical order.

CONTENT_EXPORT bool IsEnforceSameDocumentOriginInvariantsEnabled();

}  // namespace features

#endif  // CONTENT_COMMON_FEATURES_H_
