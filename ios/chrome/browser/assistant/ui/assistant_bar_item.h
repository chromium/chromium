// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_ITEM_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_ITEM_H_

#import <UIKit/UIKit.h>

// Represents a button in the Assistant Sheet bar.
@interface AssistantBarItem : NSObject

// The image to display for the button.
@property(nonatomic, strong, readonly) UIImage* image;

// The accessibility label for the button.
@property(nonatomic, copy, readonly) NSString* accessibilityLabel;

// The action to execute when the button is tapped.
@property(nonatomic, readonly) void (^action)(void);

// Initializes the button with the given image, accessibility label, and action.
- (instancetype)initWithImage:(UIImage*)image
           accessibilityLabel:(NSString*)accessibilityLabel
                       action:(void (^)(void))action NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_ITEM_H_
