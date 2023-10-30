// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mutator.h"

@protocol UnitConversionConsumer;

// Mediator that handles the unit conversion operations.
@interface UnitConversionMediator : NSObject <UnitConversionMutator>

@property(nonatomic, weak) id<UnitConversionConsumer> consumer;

// Logs all the unit conversion histograms at the coordinator stop.
- (void)reportMetrics;

@end

#endif  // IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_CONVERSION_MEDIATOR_H_
