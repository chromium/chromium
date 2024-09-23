// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_mutator.h"
#import "ios/chrome/browser/unit_conversion/unit_conversion_service.h"

@protocol UnitConversionConsumer;

// Mediator that handles the unit conversion operations.
@interface UnitConversionMediator : NSObject <UnitConversionMutator>

@property(nonatomic, weak) id<UnitConversionConsumer> consumer;

// UnitConversionMediator designated init function.
- (instancetype)initWithService:(UnitConversionService*)service
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Logs all the unit conversion histograms at the coordinator stop.
- (void)reportMetrics;

// Clears the references to model objects.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_MEDIATOR_H_
