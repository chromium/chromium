// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_display_handler.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"

@implementation CredentialProviderPromoDisplayHandler {
  id<CredentialProviderPromoCommands> _handler;
}

- (instancetype)initWithHandler:(id<CredentialProviderPromoCommands>)handler {
  self = [super init];
  if (self) {
    DCHECK(handler);
    _handler = handler;
  }
  return self;
}

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  [_handler showCredentialProviderPromoWithTrigger:
                CredentialProviderPromoTrigger::RemindMeLater];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      promos_manager::Promo::CredentialProviderExtension,
      &feature_engagement::kIPHiOSPromoCredentialProviderExtensionFeature,
      @[ [[ImpressionLimit alloc] initWithLimit:3 forNumDays:365] ]);
}

@end
