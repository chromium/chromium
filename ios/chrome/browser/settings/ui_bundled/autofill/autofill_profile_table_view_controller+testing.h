// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

@interface AutofillProfileTableViewController (Testing)
- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths;
@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_
