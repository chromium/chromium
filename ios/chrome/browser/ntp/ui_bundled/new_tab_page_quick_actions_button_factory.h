// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

// Factory for buttons in the NTP Quick Actions row.
@interface NewTabPageQuickActionsButtonFactory : NSObject

// Returns an AI Mode Quick Action button.
+ (UIButton*)aimButtonWithTitle:(BOOL)hasTitle;

// Returns a Quick Action button for opening AI Mode with the Image Generation
// chip active.
+ (UIButton*)aimImageGenerationButton;

// Returns a Quick Action button for attaching media when triggering AI Mode.
+ (UIButton*)aimAttachImageButton;

// Returns an Incognito search Quick Action button.
+ (UIButton*)incognitoSearchButtonWithTitle:(BOOL)hasTitle;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_BUTTON_FACTORY_H_
