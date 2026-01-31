// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// View controller of default browser screen.
@interface DefaultBrowserScreenViewController
    : PromoStyleViewController <DefaultBrowserScreenConsumer>

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_H_
