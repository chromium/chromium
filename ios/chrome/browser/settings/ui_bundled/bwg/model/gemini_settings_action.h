// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, GeminiSettingsActionType);

// A settings action for Gemini, representing what should happen when the user
// taps on a settings option in the settings page.
@interface GeminiSettingsAction : NSObject

// The type of the settings action.
@property(nonatomic, readonly) GeminiSettingsActionType type;

// The URL to open when the settings action type is
// `GeminiSettingsActionTypeURL`. Otherwise this is nil.
@property(nonatomic, readonly) NSURL* URL;

// The view controller to present when the settings action type is
// `GeminiSettingsActionTypeViewController`. Otherwise this is nil.
@property(nonatomic, readonly) UIViewController* viewController;

// Designated initializer.
- (instancetype)initWithType:(GeminiSettingsActionType)type
                         URL:(NSURL*)URL
              viewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_ACTION_H_
