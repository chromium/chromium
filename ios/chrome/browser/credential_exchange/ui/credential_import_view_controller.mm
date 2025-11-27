// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/ui/credential_import_view_controller.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/credential_exchange/public/credential_import_stage.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/public/import_data_item_consumer.h"
#import "ios/chrome/browser/data_import/ui/import_data_item_table_view.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Number of expected items in the table.
constexpr int kExpectedItemCount = 2;

}  // namespace

@implementation CredentialImportViewController {
  // Displays the status of importing specific credential types.
  ImportDataItemTableView* _tableView;
}

- (void)viewDidLoad {
  // TODO(crbug.com/450982128): Use correct banner.
  self.bannerName = @"safari_data_import";
  self.titleText =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_EXCHANGE_IMPORT_TITLE);
  self.configuration.primaryActionString = l10n_util::GetNSString(IDS_CONTINUE);
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  [super viewDidLoad];
}

#pragma mark - CredentialImportConsumer

- (void)setImportDataItem:(ImportDataItem*)importDataItem {
  if (!_tableView) {
    [self createTableView];
  }

  [_tableView populateItem:importDataItem];
}

- (void)setUserEmail:(const std::string&)userEmail {
  self.disclaimerText =
      l10n_util::GetNSStringF(IDS_IOS_CREDENTIAL_EXCHANGE_IMPORT_DISCLAIMER,
                              base::UTF8ToUTF16(userEmail));
}

- (void)transitionToImportStage:(CredentialImportStage)importStage {
  switch (importStage) {
    case CredentialImportStage::kNotStarted:
      NOTREACHED();
    case CredentialImportStage::kImporting:
      self.navigationItem.rightBarButtonItem = nil;
      self.configuration.primaryActionEnabled = NO;
      [self reloadConfiguration];
      [_tableView notifyImportStart];
      break;
    case CredentialImportStage::kImported:
      self.configuration.primaryActionString = l10n_util::GetNSString(IDS_DONE);
      self.configuration.primaryActionEnabled = YES;
      [self reloadConfiguration];
      break;
  }
}

#pragma mark - Actions

// Invoked when the user taps the "Cancel" button.
- (void)cancelButtonTapped {
  [self.delegate didTapDismissButton];
}

#pragma mark - Private

// Creates the table view for this view controller.
- (void)createTableView {
  _tableView =
      [[ImportDataItemTableView alloc] initWithItemCount:kExpectedItemCount];
  UIView* specificContentView = self.specificContentView;
  [specificContentView addSubview:_tableView];
  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor
        constraintEqualToAnchor:specificContentView.topAnchor],
    [_tableView.bottomAnchor
        constraintLessThanOrEqualToAnchor:specificContentView.bottomAnchor],
    [_tableView.leadingAnchor
        constraintEqualToAnchor:specificContentView.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:specificContentView.trailingAnchor],
  ]];
}

@end
