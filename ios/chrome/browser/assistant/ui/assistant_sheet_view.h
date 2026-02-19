// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_

#import <UIKit/UIKit.h>

// View that contains the visual elements of the Assistant Sheet.
//
// The layout is structured as follows:
//
// +----------------------------------+
// |            headerView            |
// |         +-------------+          |
// |         |   Grabber   |          |
// |         +-------------+          |
// +----------------------------------+
// |           scrollView             |
// |  +----------------------------+  |
// |  |        contentView         |  |
// |  |  (Child VC View goes here) |  |
// |  +----------------------------+  |
// +----------------------------------+
@interface AssistantSheetView : UIView

// The header view (contains grabber).
@property(nonatomic, strong, readonly) UIView* headerView;

// The content view where subviews should be added.
@property(nonatomic, strong, readonly) UIView* contentView;

// Returns the preferred height of the sheet based on its content.
- (CGFloat)preferredHeight;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_
