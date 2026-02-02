// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/generic/coordinator/default_browser_generic_promo_mediator.h"

#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation DefaultBrowserGenericPromoMediator

#pragma mark - Public

- (void)didTapPrimaryActionButton:(BOOL)useDefaultAppsDestination {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (localState) {
    // Mark the Set Up List Item as complete. This is a no-op if the item is
    // already complete.
    set_up_list_prefs::MarkItemComplete(localState,
                                        SetUpListItemType::kDefaultBrowser);
  }

  OpenIOSDefaultBrowserSettingsPage(useDefaultAppsDestination);
}

@end
