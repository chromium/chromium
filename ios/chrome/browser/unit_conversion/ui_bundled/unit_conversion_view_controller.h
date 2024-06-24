// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_consumer.h"

@protocol UnitConversionMutator;
@protocol UnitConversionViewControllerDelegate;

// UnitConversionViewController instantiated by initWithSourceUnit when long
// pressing and choosing to convert a detected unit or tapping on a detected
// unit.
@interface UnitConversionViewController
    : UITableViewController <UnitConversionConsumer,
                             UIPopoverPresentationControllerDelegate,
                             UISheetPresentationControllerDelegate>

@property(nonatomic, weak) id<UnitConversionMutator> mutator;

// A delegate to trigger the `Report an issue` UI and to dismiss the VC when
// tapping on the close button.
@property(nonatomic, weak) id<UnitConversionViewControllerDelegate> delegate;

// UnitConversionViewController designated init function.
- (instancetype)initWithSourceUnit:(NSUnit*)sourceUnit
                        targetUnit:(NSUnit*)targetUnit
                         unitValue:(double)unitValue NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_VIEW_CONTROLLER_H_
