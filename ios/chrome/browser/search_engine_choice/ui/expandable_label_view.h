// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_EXPANDABLE_LABEL_VIEW_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_EXPANDABLE_LABEL_VIEW_H_

#import <UIKit/UIKit.h>

// This view can display a text either on one line, or multiple lines (as many
// as needed to show all the text).
@interface ExpandableLabelView : UIView

// The text to display.
@property(nonatomic, copy) NSString* text;
// Whether the text shown on one line or multiple lines.
@property(nonatomic, assign) BOOL expanded;
// YES if the text doesn't fit with only one line.
@property(nonatomic, assign, readonly) BOOL isExpandable;
// Vertical anchor attached to the bottom of one line label. This anchor is not
// affected by `-ExpandableLabelView.expanded` value.
@property(nonatomic, strong, readonly) NSLayoutYAxisAnchor* oneLineBottomAnchor;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_EXPANDABLE_LABEL_VIEW_H_
