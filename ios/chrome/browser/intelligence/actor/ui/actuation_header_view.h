// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

// Header view anchored above the actuation worklog.
//
// +----------------------------------------------------+
// | (*)  Task Title              [secondary] [primary] |
// |      Subtitle (optional)                           |
// +----------------------------------------------------+
// (*) = Icon, [x] = Accessory buttons (optional)
@interface ActuationHeaderView : UIView

// Main title text.
@property(nonatomic, copy) NSString* title;

// Subtitle text displayed below the title.
@property(nonatomic, copy) NSString* subtitle;

// Whether actuation is currently in progress. Default is NO (static logo).
@property(nonatomic, assign, getter=isActuating) BOOL actuating;

// Primary accessory button. Setting nil removes the button from the header.
@property(nonatomic, strong) UIButton* primaryAccessoryButton;

// Secondary accessory button. Setting nil removes the button from the header.
@property(nonatomic, strong) UIButton* secondaryAccessoryButton;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Resets the header state, clearing all internal properties.
- (void)reset;

// Helper to create a circular icon button for header accessories.
+ (UIButton*)createCircularIconButtonWithIcon:(UIImage*)icon
                                       action:(UIAction*)action;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_
