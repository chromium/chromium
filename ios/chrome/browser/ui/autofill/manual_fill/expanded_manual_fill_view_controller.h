// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace manual_fill {
enum class ManualFillDataType;
}

// View that presents manual filling options for a specific autofill data type
// (i.e., password, payment or address) and allows switching between the
// different types.
@interface ExpandedManualFillViewController : UIViewController

// Designated initializer. `dataType` represents the type of manual filling
// options to show.
- (instancetype)initForDataType:(manual_fill::ManualFillDataType)dataType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_EXPANDED_MANUAL_FILL_VIEW_CONTROLLER_H_
