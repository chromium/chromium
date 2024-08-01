// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Used to asynchronously fetch favicon to use. Needs to be invoked with a
// block as parameter that will be invoked with the fetched favicon.
using FirstFollowFaviconSource = void (^)(void (^completion)(UIImage* favicon));

// The UI that informs the user about the feed and following websites the
// first few times the user follows any website.
@interface FirstFollowViewController : ConfirmationAlertViewController

// Convenience initializer.
- (instancetype)initWithTitle:(NSString*)title
                       active:(BOOL)active
                faviconSource:(FirstFollowFaviconSource)faviconSource
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibName
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_VIEW_CONTROLLER_H_
