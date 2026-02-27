// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation AutofillAISaveEntityTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAutofillAISaveEntityTableViewId;
}

#pragma mark - AutofillAISaveEntityConsumer

- (void)setNewEntity:(autofill::EntityInstance)newEntity
           oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
           userEmail:(const std::u16string&)userEmail {
  // TODO(crbug.com/480934220): update UI.
}

@end
