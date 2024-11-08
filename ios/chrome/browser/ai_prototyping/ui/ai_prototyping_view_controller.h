// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/web/public/web_state.h"

@protocol AIPrototypingMutator;

// View controller that displays a debug UI menu for AI prototyping.
// Functionality in this view are noop when compile flag
// `BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE` is disabled.
@interface AIPrototypingViewController
    : UIViewController <AIPrototypingConsumer>

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_
