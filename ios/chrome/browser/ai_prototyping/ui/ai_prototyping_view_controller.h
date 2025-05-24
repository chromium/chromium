// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"

@protocol AIPrototypingMutator;

// View controller that displays a debug UI menu for AI prototyping.
// This wraps multiple pages, each representing an AI feature.
@interface AIPrototypingViewController
    : UIViewController <AIPrototypingConsumer>

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_H_
