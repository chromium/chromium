// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_consumer.h"

@class ActuationInterventionViewController;
@protocol ActuationInterventionMutator;

// Delegate protocol for `ActuationInterventionViewController` events.
@protocol ActuationInterventionViewControllerDelegate <NSObject>

// Called when the fitting height of `viewController` changes.
- (void)interventionViewControllerDidChangeHeight:
    (ActuationInterventionViewController*)viewController;

@end

// View controller displaying the intervention of an actuation task, such as a
// confirmation card or action buttons. Its view collapses to zero height when
// there is no intervention.
@interface ActuationInterventionViewController
    : UIViewController <ActuationInterventionConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<ActuationInterventionViewControllerDelegate>
    delegate;

// Mutator for dispatching user actions.
@property(nonatomic, weak) id<ActuationInterventionMutator> mutator;

// Returns the height fitting the intervention at the view's current width, or
// zero if there is no intervention.
- (CGFloat)fittingHeight;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_INTERVENTION_VIEW_CONTROLLER_H_
