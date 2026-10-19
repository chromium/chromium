// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_CUSTOMIZATION_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_CUSTOMIZATION_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

// Bottom sheet view controller for the actionable NTP Customization tip.
@interface NTPCustomizationPromoViewController : ButtonStackViewController

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_CUSTOMIZATION_PROMO_VIEW_CONTROLLER_H_
