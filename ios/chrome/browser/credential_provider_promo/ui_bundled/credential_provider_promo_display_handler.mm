// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_display_handler.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"

@implementation CredentialProviderPromoDisplayHandler

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  DCHECK(self.handler);
  [self.handler showCredentialProviderPromoWithTrigger:
                    CredentialProviderPromoTrigger::RemindMeLater];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(
      promos_manager::Promo::CredentialProviderExtension,
      feature_engagement::kIPHiOSPromoCredentialProviderExtensionFeature);
}

@end
