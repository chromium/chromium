// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_VIEW_CONTROLLER_H_
#define IOS_WEB_SHELL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace web {
class BrowserState;
class WebState;
}

// Accessibility label for the back button.
extern NSString* const kWebShellBackButtonAccessibilityLabel;
// Accessibility label for the forward button.
extern NSString* const kWebShellForwardButtonAccessibilityLabel;
// Accessibility label for the URL address text field.
extern NSString* const kWebShellAddressFieldAccessibilityLabel;

// Implements the main UI for ios_web_shell, including a toolbar and web view.
@interface ViewController : UIViewController

@property(nonatomic, strong) IBOutlet UIView* containerView;
@property(nonatomic, strong) IBOutlet UIToolbar* toolbarView;
@property(nonatomic, assign, readonly) web::WebState* webState;

@end

#endif  // IOS_WEB_SHELL_VIEW_CONTROLLER_H_
