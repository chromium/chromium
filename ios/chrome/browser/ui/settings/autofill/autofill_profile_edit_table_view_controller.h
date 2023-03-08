// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_consumer.h"

@protocol AutofillProfileEditTableViewControllerDelegate;

// The table view for the Autofill profile edit settings.
@interface AutofillProfileEditTableViewController
    : AutofillEditTableViewController <AutofillProfileEditConsumer>

// Initializes a AutofillProfileEditTableViewController with passed delegate,
// `profile` and `userEmail`.
- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditTableViewControllerDelegate>)delegate
                       userEmail:(NSString*)userEmail NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
