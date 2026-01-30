// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_mediator.h"

#import <Foundation/Foundation.h>

#import <map>
#import <optional>

#import "base/containers/small_map.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/promos_manager/model/promo_display_context.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@interface PromosManagerMediator () {
  raw_ptr<WebStateList> _webStateList;
}
@end

@implementation PromosManagerMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                         promoConfigs:(PromoConfigsSet)promoConfigs
                         webStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    _promosManager = promosManager;
    _webStateList = webStateList;
    if (promoConfigs.size()) {
      _promosManager->InitializePromoConfigs(std::move(promoConfigs));
    }
  }

  return self;
}

- (void)deregisterPromo:(promos_manager::Promo)promo {
  _promosManager->DeregisterPromo(promo);
}

- (void)deregisterAfterDisplay:(promos_manager::Promo)promo {
  _promosManager->DeregisterAfterDisplay(promo);
}

- (std::optional<PromoDisplayData>)nextPromoForDisplay {
  DCHECK_NE(_promosManager, nullptr);

  std::optional<promos_manager::Promo> forcedPromo =
      [self forcedPromoToDisplay];
  if (forcedPromo) {
    return PromoDisplayData{.promo = forcedPromo.value(), .was_forced = true};
  }

  PromoDisplayContext displayContext;
  displayContext.is_on_fresh_ntp = false;

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (activeWebState && IsVisibleURLNewTabPage(activeWebState)) {
    displayContext.is_on_fresh_ntp =
        NewTabPageTabHelper::FromWebState(activeWebState)->IsScrolledToTop();
  }

  std::optional<promos_manager::Promo> promo =
      self.promosManager->NextPromoForDisplay(displayContext);
  if (promo) {
    return PromoDisplayData{.promo = promo.value(), .was_forced = false};
  }
  return std::nullopt;
}

// Returns the promo selected in the Force Promo experimental setting.
// If none are selected, returns empty string. If user is in beta/stable,
// this method always returns nil.
- (std::optional<promos_manager::Promo>)forcedPromoToDisplay {
  NSString* forcedPromoName = experimental_flags::GetForcedPromoToDisplay();
  if ([forcedPromoName length] > 0) {
    std::optional<promos_manager::Promo> forcedPromo =
        promos_manager::PromoForName(base::SysNSStringToUTF8(forcedPromoName));
    DCHECK(forcedPromo);
    return forcedPromo;
  }
  return std::nullopt;
}

@end
