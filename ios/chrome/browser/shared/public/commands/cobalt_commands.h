// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COBALT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COBALT_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Commands relating to Cobalt.
@protocol CobaltCommands <NSObject>

// Shows Cobalt.
- (void)showCobalt;

// Hides Cobalt.
- (void)hideCobalt;

// Shows a cobalt alert with the given `title` and `message`.
- (void)showCobaltAlertWithTitle:(NSString*)title
                         message:(NSString*)message
                      completion:(void (^)(bool))completion;

// Hides the cobalt alert.
- (void)hideCobaltAlert;

// Shows a cobalt popup.
- (void)showCobaltPopupViewController:(UIViewController*)popupViewController
                           completion:(void (^)(NSError*))completion;

// Hides the cobalt popup
- (void)hideCobaltPopup;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COBALT_COMMANDS_H_
