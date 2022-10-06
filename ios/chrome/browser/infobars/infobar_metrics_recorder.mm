// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Histogram names for InfobarTypeConfirm.
// Banner.
const char kInfobarConfirmBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeConfirm";
const char kInfobarConfirmBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeConfirm";
// Modal.
const char kInfobarConfirmModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeConfirm";
// Badge.
const char kInfobarConfirmBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeConfirm";

// Histogram names for InfobarTypePasswordSave.
// Banner.
const char kInfobarPasswordSaveBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePasswordSave";
const char kInfobarPasswordSaveBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePasswordSave";
// Modal.
const char kInfobarPasswordSaveModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePasswordSave";
// Badge.
const char kInfobarPasswordSaveBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypePasswordSave";

// Histogram names for InfobarTypePasswordUpdate.
// Banner.
const char kInfobarPasswordUpdateBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePasswordUpdate";
const char kInfobarPasswordUpdateBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePasswordUpdate";
// Modal.
const char kInfobarPasswordUpdateModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePasswordUpdate";
// Badge.
const char kInfobarPasswordUpdateBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypePasswordUpdate";

// Histogram names for InfobarTypeSaveCard.
// Banner.
const char kInfobarSaveCardBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeSaveCard";
const char kInfobarSaveCardBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeSaveCard";
// Modal.
const char kInfobarSaveCardModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeSaveCard";
// Badge.
const char kInfobarSaveCardBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeSaveCard";

// Histogram names for InfobarTypeTranslate.
// Banner.
const char kInfobarTranslateBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeTranslate";
const char kInfobarTranslateBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeTranslate";
// Modal.
const char kInfobarTranslateModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeTranslate";
// Badge.
const char kInfobarTranslateBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeTranslate";

// Histogram names for InfobarTypeSaveAutofillAddressProfile.
// Banner.
const char kInfobarAutofillAddressBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeAutofillAddressProfile";
const char kInfobarAutofillAddressBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeAutofillAddressProfile";
// Modal.
const char kInfobarAutofillAddressModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeAutofillAddressProfile";
// Badge.
const char kInfobarAutofillAddressBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeAutofillAddressProfile";

// Histogram names for InfobarTypeReadingList.
// Banner.
const char kInfobarReadingListBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeReadingList";
const char kInfobarReadingListBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeReadingList";
// Modal.
const char kInfobarReadingListModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeReadingList";
// Badge.
const char kInfobarReadingListBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeReadingList";

// Histogram names for InfobarTypePermissions.
// Banner.
const char kInfobarPermissionsBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePermissions";
const char kInfobarPermissionsBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePermissions";
// Modal.
const char kInfobarPermissionsModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePermissions";
// Badge.
const char kInfobarPermissionsBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypePermissions";

// Histogram names for InfobarTypeTailoredSecurityService.
const char kInfobarTailoredSecurityServiceBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypePermissions";
const char kInfobarTailoredSecurityServiceBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypePermissions";
// Modal.
const char kInfobarTailoredSecurityServiceModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypePermissions";

// Histogram names for InfobarTypeSyncError.
// Banner.
const char kInfobarSyncErrorBannerEventHistogram[] =
    "Mobile.Messages.Banner.Event.InfobarTypeSyncError";
const char kInfobarSyncErrorBannerDismissTypeHistogram[] =
    "Mobile.Messages.Banner.Dismiss.InfobarTypeSyncError";
// Modal.
const char kInfobarSyncErrorModalEventHistogram[] =
    "Mobile.Messages.Modal.Event.InfobarTypeSyncError";
// Badge.
const char kInfobarSyncErrorBadgeTappedHistogram[] =
    "Mobile.Messages.Badge.Tapped.InfobarTypeSyncError";

}  // namespace

@interface InfobarMetricsRecorder ()
// The Infobar type for the metrics recorder.
@property(nonatomic, assign) InfobarType infobarType;
@end

@implementation InfobarMetricsRecorder

#pragma mark - Public

- (instancetype)initWithType:(InfobarType)infobarType {
  self = [super init];
  if (self) {
    _infobarType = infobarType;
  }
  return self;
}

- (void)recordBannerEvent:(MobileMessagesBannerEvent)event {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveBannerEventHistogram,
                                event);
      if (event == MobileMessagesBannerEvent::Accepted) {
        LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
      }
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateBannerEventHistogram,
                                event);
      if (event == MobileMessagesBannerEvent::Accepted) {
        LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
      }
      break;
    case InfobarType::kInfobarTypeSaveCard:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSaveCardBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeTranslate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTranslateBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      UMA_HISTOGRAM_ENUMERATION(kInfobarAutofillAddressBannerEventHistogram,
                                event);
      break;
    case InfobarType::kInfobarTypeAddToReadingList:
      base::UmaHistogramEnumeration(kInfobarReadingListBannerEventHistogram,
                                    event);
      break;
    case InfobarType::kInfobarTypePermissions:
      base::UmaHistogramEnumeration(kInfobarPermissionsBannerEventHistogram,
                                    event);
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      base::UmaHistogramEnumeration(
          kInfobarTailoredSecurityServiceBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeSyncError:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSyncErrorBannerEventHistogram, event);
      break;
  }
}

- (void)recordBannerDismissType:(MobileMessagesBannerDismissType)dismissType {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(
          kInfobarPasswordUpdateBannerDismissTypeHistogram, dismissType);
      break;
    case InfobarType::kInfobarTypeSaveCard:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSaveCardBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypeTranslate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTranslateBannerDismissTypeHistogram,
                                dismissType);
      break;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      UMA_HISTOGRAM_ENUMERATION(
          kInfobarAutofillAddressBannerDismissTypeHistogram, dismissType);
      break;
    case InfobarType::kInfobarTypeAddToReadingList:
      base::UmaHistogramEnumeration(
          kInfobarReadingListBannerDismissTypeHistogram, dismissType);
      break;
    case InfobarType::kInfobarTypePermissions:
      base::UmaHistogramEnumeration(
          kInfobarPermissionsBannerDismissTypeHistogram, dismissType);
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      base::UmaHistogramEnumeration(
          kInfobarTailoredSecurityServiceBannerDismissTypeHistogram,
          dismissType);
      break;
    case InfobarType::kInfobarTypeSyncError:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSyncErrorBannerDismissTypeHistogram,
                                dismissType);
      break;
  }
}

- (void)recordBannerOnScreenDuration:(base::TimeDelta)duration {
  UMA_HISTOGRAM_MEDIUM_TIMES("Mobile.Messages.Banner.OnScreenTime", duration);
}

- (void)recordModalEvent:(MobileMessagesModalEvent)event {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateModalEventHistogram,
                                event);
      break;
    case InfobarType::kInfobarTypeSaveCard:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSaveCardModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeTranslate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTranslateModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      UMA_HISTOGRAM_ENUMERATION(kInfobarAutofillAddressModalEventHistogram,
                                event);
      break;
    case InfobarType::kInfobarTypeAddToReadingList:
      base::UmaHistogramEnumeration(kInfobarReadingListModalEventHistogram,
                                    event);
      break;
    case InfobarType::kInfobarTypePermissions:
      base::UmaHistogramEnumeration(kInfobarPermissionsModalEventHistogram,
                                    event);
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      base::UmaHistogramEnumeration(
          kInfobarTailoredSecurityServiceModalEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeSyncError:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSyncErrorModalEventHistogram, event);
      break;
  }
}

- (void)recordBadgeTappedInState:(MobileMessagesBadgeState)state {
  switch (self.infobarType) {
    case InfobarType::kInfobarTypeConfirm:
      UMA_HISTOGRAM_ENUMERATION(kInfobarConfirmBadgeTappedHistogram, state);
      break;
    case InfobarType::kInfobarTypePasswordSave:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordSaveBadgeTappedHistogram,
                                state);
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateBadgeTappedHistogram,
                                state);
      break;
    case InfobarType::kInfobarTypeSaveCard:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSaveCardBadgeTappedHistogram, state);
      break;
    case InfobarType::kInfobarTypeTranslate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTranslateBadgeTappedHistogram, state);
      break;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      UMA_HISTOGRAM_ENUMERATION(kInfobarAutofillAddressBadgeTappedHistogram,
                                state);
      break;
    case InfobarType::kInfobarTypeAddToReadingList:
      base::UmaHistogramEnumeration(kInfobarReadingListBadgeTappedHistogram,
                                    state);
      break;
    case InfobarType::kInfobarTypePermissions:
      base::UmaHistogramEnumeration(kInfobarPermissionsBadgeTappedHistogram,
                                    state);
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      // TailoredSecurityService infobar doesn't have a badge.
      NOTREACHED();
      break;
    case InfobarType::kInfobarTypeSyncError:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSyncErrorBadgeTappedHistogram, state);
      break;
  }
}

@end
