// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "components/autofill/core/common/dense_set.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

@interface AutofillProfileTableViewController (Testing)
+ (autofill::DenseSet<autofill::EntityTypeName>)identityDocsForTesting;
+ (autofill::DenseSet<autofill::EntityTypeName>)travelForTesting;
+ (autofill::DenseSet<autofill::EntityTypeName>)shoppingForTesting;

- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths;
@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_PROFILE_TABLE_VIEW_CONTROLLER_TESTING_H_
