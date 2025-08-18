// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_reminder_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"

@interface SafariDataImportReminderPromoDisplayHandler () <
    SafariDataImportUIHandler>

@end

@implementation SafariDataImportReminderPromoDisplayHandler {
  /// Handler for application commands.
  __weak id<ApplicationCommands> _applicationHandler;
  /// UI handler for promos.
  __weak id<PromosManagerUIHandler> _promosManagerUIHandler;
}

- (instancetype)initWithApplicationCommandsHandler:
                    (id<ApplicationCommands>)applicationHandler
                            promosManagerUIHandler:(id<PromosManagerUIHandler>)
                                                       promosManagerUIHandler {
  self = [super init];
  if (self) {
    _applicationHandler = applicationHandler;
    _promosManagerUIHandler = promosManagerUIHandler;
  }
  return self;
}

#pragma mark - StandardPromoDisplayHandler

- (void)handleDisplay {
  CHECK(_applicationHandler);
  [_applicationHandler displaySafariDataImportFromEntryPoint:
                           SafariDataImportEntryPoint::kReminder
                                               withUIHandler:self];
}

#pragma mark - PromoProtocol

- (PromoConfig)config {
  return PromoConfig(promos_manager::Promo::SafariImportRemindMeLater,
                     &feature_engagement::kIPHiOSSafariImportFeature);
}

#pragma mark - SafariDataImportUIHandler

- (void)safariDataImportDidDismiss {
  [_promosManagerUIHandler promoWasDismissed];
}

@end
