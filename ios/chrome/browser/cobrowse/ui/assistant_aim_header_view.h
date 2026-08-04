// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"

@class AssistantAIMHeaderView;
@protocol AssistantAIMMutator;

// Represents the possible interactions with the header view.
@protocol AssistantAIMHeaderViewDelegate <NSObject>

// Called when the close button in the header view is closed.
- (void)assistantAIMHeaderViewDidPressClose:(AssistantAIMHeaderView*)headerView;

// Called when the back button in the header view is tapped.
- (void)assistantAIMHeaderViewDidTapBack:(AssistantAIMHeaderView*)headerView;

// Called when the user requests to see the AIM SRP logs.
- (void)assistantAIMHeaderViewDidRequestSRPLogs:
    (AssistantAIMHeaderView*)headerView;

// Called when the user requests to see the AIM Loaded URL.
- (void)assistantAIMHeaderViewDidRequestLoadedURL:
    (AssistantAIMHeaderView*)headerView;

// Called when the start new thread button is tapped.
- (void)assistantAIMHeaderViewDidTapStartNewThread:
    (AssistantAIMHeaderView*)headerView;

// Called when the history button is tapped.
- (void)assistantAIMHeaderViewDidTapHistory:(AssistantAIMHeaderView*)headerView;

// Called when the my activity button is tapped.
- (void)assistantAIMHeaderViewDidTapMyActivity:
    (AssistantAIMHeaderView*)headerView;

// Called when the help button is tapped.
- (void)assistantAIMHeaderViewDidTapHelp:(AssistantAIMHeaderView*)headerView;

@end

// Represents the header of cobrowse, containing the title and action buttons.
@interface AssistantAIMHeaderView : UIView

// The delegate for this header view.
@property(nonatomic, weak) id<AssistantAIMHeaderViewDelegate> delegate;

// Sets the title text of this header.
- (void)setTitle:(NSString*)title;

// Sets the mode of the header view.
- (void)setMode:(AssistantAIMState)mode;

// Proportionally adjusts the header based on the given percentage.
- (void)adjustForPercentage:(CGFloat)percentage;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HEADER_VIEW_H_
