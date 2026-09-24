// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationInterventionData;
@class ActuationInterventionView;
enum class ActuationInterventionAction;

// Delegate protocol for user interaction events on ActuationInterventionView.
@protocol ActuationInterventionViewDelegate <NSObject>

// Called when the user triggers `action` on the displayed intervention.
- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction)action;

@end

// Displays the bottom intervention for the actuation worklog based on
// `ActuationInterventionType`. Manages its own padding (hosts only pin edges)
// and collapses to zero height when empty to simplify host sizing.
@interface ActuationInterventionView : UIView

// Delegate receiving interaction events.
@property(nonatomic, weak) id<ActuationInterventionViewDelegate> delegate;

// Designated initializer
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Configures the view with a model data. Passing `nil` hides the view.
- (void)configureWithData:(ActuationInterventionData*)data;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_H_
