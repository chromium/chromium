// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class BestFeaturesItem;

@protocol BestFeaturesScreenConsumer;

// Mediator for the Best Feature's Screen.
@interface BestFeaturesScreenMediator : NSObject

// The consumer for this object. Setting it will update the consumer with
// current data.
@property(nonatomic, weak) id<BestFeaturesScreenConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

// Returns the list of BestFeaturesItems to be shown.
- (NSArray<BestFeaturesItem*>*)bestFeatureItems;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_
