// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class AssistantAIMHeaderView;

// Represents the possible interactions with the header view.
@protocol AssistantAIMHeaderViewDelegate <NSObject>

// Called when the close button in the header view is closed.
- (void)assistantAIMHeaderViewDidPressClose:(AssistantAIMHeaderView*)headerView;

@end

// Represents the header of cobrowse, containing the title and action buttons.
@interface AssistantAIMHeaderView : UIView

// The delegate for this header view.
@property(nonatomic, weak) id<AssistantAIMHeaderViewDelegate> delegate;

// Sets the title text of this header.
- (void)setTitle:(NSString*)title;

// Proportionally adjusts the header based on the given percentage.
- (void)adjustForPercentage:(CGFloat)percentage;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
