// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAttributes = kSectionIdentifierEnumZero,
};
}  // namespace

@implementation AutofillAIEntityEditTableViewController {
  // Items to be displayed.
  NSArray<TableViewItem*>* _editItems;

  // Whether editing is allowed.
  BOOL _editingAllowed;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierAttributes];

  for (TableViewItem* item in _editItems) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierAttributes];
  }
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return _editingAllowed;
}

#pragma mark - AutofillAIEntityEditConsumer

- (void)setTitle:(NSString*)title {
  super.title = title;
}

- (void)setEditItems:(NSArray<TableViewItem*>*)items {
  _editItems = items;

  // If the view has already loaded, we need to reload the model to reflect
  // changes.
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
}

- (void)setEditingAllowed:(BOOL)editingAllowed {
  _editingAllowed = editingAllowed;
  [self updateUIForEditState];
}

@end
