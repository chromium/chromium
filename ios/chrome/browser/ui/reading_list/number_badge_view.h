// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_NUMBER_BADGE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_NUMBER_BADGE_VIEW_H_

#import <UIKit/UIKit.h>

// Provides a view that displays a number in white text on a badge that starts
// round and stretches to a pill-shape to fit the number displayed. Displays
// only positive integers: negative/0 values will result in the badge being
// hidden.
@interface NumberBadgeView : UIView

// Set the number displayed by the badge. A value <=0 will cause the badge to be
// hidden.
- (void)setNumber:(NSInteger)number animated:(BOOL)animated;
// Set the color of the badge (the text is white).
- (void)setBackgroundColor:(UIColor*)backgroundColor animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_NUMBER_BADGE_VIEW_H_
