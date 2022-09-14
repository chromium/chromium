// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_

#import <UIKit/UIKit.h>

// Pill-shaped view that displays white text.
@interface TextBadgeView : UIView

// Text displayed on the badge.
@property(nonatomic, copy) NSString* text;

// Initialize the text badge with the given display text and horizontal label
// margin.
- (instancetype)initWithText:(NSString*)text
       labelHorizontalMargin:(CGFloat)margin NS_DESIGNATED_INITIALIZER;

// Convenience initializer that uses a default horizontal label margin value.
- (instancetype)initWithText:(NSString*)text;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_TEXT_BADGE_VIEW_H_
