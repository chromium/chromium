// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

namespace autofill {
class AutofillProfile;
class PersonalDataManager;
}  // namespace autofill

// The table view for the Autofill profile edit settings.
@interface AutofillProfileEditTableViewController
    : AutofillEditTableViewController

// Creates a controller for |profile| and |dataManager| that cannot be null.
+ (instancetype)controllerWithProfile:(const autofill::AutofillProfile&)profile
                  personalDataManager:
                      (autofill::PersonalDataManager*)dataManager;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_H_
