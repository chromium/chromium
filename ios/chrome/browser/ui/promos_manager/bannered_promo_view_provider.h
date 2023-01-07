// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_BANNERED_PROMO_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_BANNERED_PROMO_VIEW_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/promos_manager/promo_protocol.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_action_handler.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// BanneredPromoViewProvider enables feature teams to simply and easily
// construct a bannered promo, `viewController`, for display by implementing
// BanneredPromoViewProvider's banner, titles, buttons, images, and handlers.
@protocol BanneredPromoViewProvider <PromoProtocol, StandardPromoActionHandler>

@required

// The promo, `viewController`, to be displayed. Please override & implement (or
// nillify) the banner, titles, buttons, and images your promo does (or does
// not) need.
- (PromoStyleViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_BANNERED_PROMO_VIEW_PROVIDER_H_
