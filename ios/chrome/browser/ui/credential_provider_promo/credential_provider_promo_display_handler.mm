// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_display_handler.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (promos_manager::Promo)identifier {
  return promos_manager::Promo::CredentialProviderExtension;
}

- (NSArray<ImpressionLimit*>*)impressionLimits {
  return [NSArray arrayWithObject:[[ImpressionLimit alloc] initWithLimit:3
                                                              forNumDays:365]];
}

@end
