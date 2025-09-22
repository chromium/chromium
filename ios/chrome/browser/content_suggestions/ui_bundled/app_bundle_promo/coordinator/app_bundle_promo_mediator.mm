// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/coordinator/app_bundle_promo_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"

@interface AppBundlePromoMediator () <AppBundlePromoAudience>

@end

@implementation AppBundlePromoMediator {
  raw_ptr<AppStoreBundleService> _appStoreBundleService;
}

- (instancetype)initWithAppStoreBundleService:
    (AppStoreBundleService*)appStoreBundleService {
  CHECK(appStoreBundleService);
  if ((self = [super init])) {
    _appStoreBundleService = appStoreBundleService;
    self.config = [[AppBundlePromoConfig alloc] init];
    self.config.audience = self;
  }
  return self;
}

- (void)disconnect {
  self.config = nil;
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeAppBundlePromoModuleWithCompletion:completion];
}

- (void)didSelectAppBundlePromo {
  [self.delegate logMagicStackEngagementForType:self.config.type];
  [self.presentationAudience didSelectAppBundlePromo];
}

- (void)presentAppStoreBundlePage:(UIViewController*)baseViewController
                   withCompletion:(ProceduralBlock)completion {
  CHECK(_appStoreBundleService);
  _appStoreBundleService->PresentAppStoreBundlePromo(baseViewController,
                                                     completion);
}

@end
