// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class AssistantAIMHeaderView;
@protocol AssistantAIMMutator;

// The possible modes for the header view.
enum class AssistantAIMHeaderViewMode {
  kChat,
  kHistory,
};

// Represents the possible interactions with the header view.
@protocol AssistantAIMHeaderViewDelegate <NSObject>

// Called when the close button in the header view is closed.
- (void)assistantAIMHeaderViewDidPressClose:(AssistantAIMHeaderView*)headerView;

// Called when the back button in the header view is tapped.
- (void)assistantAIMHeaderViewDidTapBack:(AssistantAIMHeaderView*)headerView;

// Called when the user requests to see the AIM SRP logs.
- (void)assistantAIMHeaderViewDidRequestSRPLogs:
    (AssistantAIMHeaderView*)headerView;

@end

// Represents the header of cobrowse, containing the title and action buttons.
@interface AssistantAIMHeaderView : UIView

// The delegate for this header view.
@property(nonatomic, weak) id<AssistantAIMHeaderViewDelegate> delegate;

// The action handler for this header view.
@property(nonatomic, weak) id<AssistantAIMMutator> actionHandler;

// Sets the title text of this header.
- (void)setTitle:(NSString*)title;

// Sets the mode of the header view.
- (void)setMode:(AssistantAIMHeaderViewMode)mode;

// Proportionally adjusts the header based on the given percentage.
- (void)adjustForPercentage:(CGFloat)percentage;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
