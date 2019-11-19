// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

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
      break;
    case InfobarType::kInfobarTypePasswordUpdate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarPasswordUpdateBannerEventHistogram,
                                event);
      break;
    case InfobarType::kInfobarTypeSaveCard:
      UMA_HISTOGRAM_ENUMERATION(kInfobarSaveCardBannerEventHistogram, event);
      break;
    case InfobarType::kInfobarTypeTranslate:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTranslateBannerEventHistogram, event);
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
  }
}

- (void)recordBannerOnScreenDuration:(double)duration {
  base::TimeDelta timeDelta = base::TimeDelta::FromSecondsD(duration);
  UMA_HISTOGRAM_MEDIUM_TIMES("Mobile.Messages.Banner.OnScreenTime", timeDelta);
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
  }
}

@end
