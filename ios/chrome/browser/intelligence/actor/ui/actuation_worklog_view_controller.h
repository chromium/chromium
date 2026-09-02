// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"

@class ActuationWorklogViewController;

// Delegate protocol for ActuationWorklogViewController events.
@protocol ActuationWorklogViewControllerDelegate <NSObject>

// Called when actuation active state changes.
// TODO(crbug.com/555198195): Remove in favor of a separate actor observer.
- (void)worklogViewController:(ActuationWorklogViewController*)viewController
           setActuationActive:(BOOL)active;

@end

// View controller displaying the actuation worklog in the Helios sheet.
@interface ActuationWorklogViewController
    : UIViewController <ActuationWorklogConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<ActuationWorklogViewControllerDelegate> delegate;

// Whether the worklog is presented in compact mode or full timeline mode.
@property(nonatomic, assign, getter=isCompact) BOOL compact;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_CONTROLLER_H_
