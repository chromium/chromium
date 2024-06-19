// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace manual_fill {
enum class ManualFillDataType;
}

@class ExpandedManualFillViewController;
@class FallbackViewController;

// Delegate for the ExpandedManualFillViewController.
@protocol ExpandedManualFillViewControllerDelegate

// Invoked after the user has tapped the close button.
- (void)expandedManualFillViewController:
            (ExpandedManualFillViewController*)expandedManualFillViewController
                     didPressCloseButton:(UIButton*)closeButton;

// Invoked after the user has selected a data type from the segmented control.
- (void)expandedManualFillViewController:
            (ExpandedManualFillViewController*)expandedManualFillViewController
                  didSelectSegmentOfType:
                      (manual_fill::ManualFillDataType)dataType;

@end

// View that presents manual filling options for a specific autofill data type
// (i.e., password, payment or address) and allows switching between the
// different types.
@interface ExpandedManualFillViewController : UIViewController

// FallbackViewController embedded inside the ExpandedManualFillViewController
// and providing the manual filling options.
@property(strong, nonatomic) FallbackViewController* childViewController;

// Designated initializer. `dataType` represents the type of manual filling
// options to show.
- (instancetype)initWithDelegate:
                    (id<ExpandedManualFillViewControllerDelegate>)delegate
                     forDataType:(manual_fill::ManualFillDataType)dataType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_
