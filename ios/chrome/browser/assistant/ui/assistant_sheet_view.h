// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_

#import <UIKit/UIKit.h>

@class AssistantBarConfiguration;

// View that contains the visual elements of the Assistant Sheet.
@interface AssistantSheetView : UIView

// The navigation configuration.
@property(nonatomic, strong) AssistantBarConfiguration* configuration;

// The close button.
@property(nonatomic, strong, readonly) UIButton* closeButton;

// The header view (contains grabber, title and buttons).
@property(nonatomic, strong, readonly) UIView* headerView;

// The title of the sheet.
@property(nonatomic, copy) NSString* title;

// The content view where subviews should be added.
@property(nonatomic, strong, readonly) UIView* contentView;

// Returns the preferred height of the sheet based on its content.
- (CGFloat)preferredHeight;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_VIEW_H_
