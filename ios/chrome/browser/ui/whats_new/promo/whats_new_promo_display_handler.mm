// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_promo_display_handler.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

@implementation WhatsNewPromoDisplayHandler {
  // Promos Manager to alert if the user uses What's New.
  raw_ptr<PromosManager> _promosManager;
}

#pragma mark - StandardPromoDisplayHandler

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if ((self = [super init])) {
    _promosManager = promosManager;
  }
  return self;
}

- (void)handleDisplay {
  DCHECK(self.handler);
  base::RecordAction(base::UserMetricsAction("WhatsNew.Promo.Displayed"));
  [self.handler showWhatsNewPromo];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::WhatsNew,
                     &feature_engagement::kIPHiOSPromoWhatsNewFeature);
}

@end
