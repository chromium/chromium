// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_mediator.h"

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_consumer.h"

@interface PrimaryToolbarMediator () <DefaultBrowserBannerAppAgentObserver>

@end

@implementation PrimaryToolbarMediator {
  DefaultBrowserBannerPromoAppAgent* _defaultBrowserBannerAppAgent;
}

- (instancetype)initWithDefaultBrowserBannerPromoAppAgent:
    (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent {
  self = [super init];
  if (self) {
    _defaultBrowserBannerAppAgent = defaultBrowserBannerAppAgent;
    [defaultBrowserBannerAppAgent addObserver:self];
  }
  return self;
}

- (void)disconnect {
  [_defaultBrowserBannerAppAgent removeObserver:self];
}

- (void)setConsumer:(id<PrimaryToolbarConsumer>)consumer {
  _consumer = consumer;

  if (_defaultBrowserBannerAppAgent.promoCurrentlyShown) {
    [self.consumer showBannerPromo];
  }
}

#pragma mark - DefaultBrowserBannerAppAgentObserver

- (void)displayPromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer showBannerPromo];
}

- (void)hidePromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer hideBannerPromo];
}

#pragma mark - BannerPromoViewDelegate

- (void)bannerPromoWasTapped:(BannerPromoView*)bannerPromoView {
  [self.settingsHandler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kBannerPromo];
  [_defaultBrowserBannerAppAgent promoTapped];
}

- (void)bannerPromoCloseButtonWasTapped:(BannerPromoView*)bannerPromoView {
  [_defaultBrowserBannerAppAgent promoCloseButtonTapped];
}

@end
