// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation SafariDataImportImportViewController

- (void)viewDidLoad {
  /// TODO(crbug.com/420703283): Replace `SafariDataImportStage::kNotStarted`
  /// with dynamically set value once stage transition technique is implemented.
  self.bannerName = @"safari_data_import";
  self.titleText = l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_SUBTITLE);
  self.primaryActionString =
      [self actionButtonStringForStage:SafariDataImportStage::kNotStarted];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  [super viewDidLoad];
  [self showInstructionView];
}

#pragma mark - Private

/// Returns the action button string for the given `stage`.
- (NSString*)actionButtonStringForStage:(SafariDataImportStage)stage {
  int messageId;
  switch (stage) {
    case SafariDataImportStage::kNotStarted:
      messageId = IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_SELECT_YOUR_FILE;
      break;
    case SafariDataImportStage::kFileLoading:
    case SafariDataImportStage::kReadyForImport:
    case SafariDataImportStage::kImporting:
    case SafariDataImportStage::kImported:
      NOTREACHED() << "Not implemented yet.";
  }
  return l10n_util::GetNSString(messageId);
}

/// Adds an instructional view to show import steps  to the view hierarchy and
/// position it.
- (void)showInstructionView {
  CHECK_EQ(static_cast<int>(self.specificContentView.subviews.count), 0);
  /// Creates the instructions view.
  NSArray* instructions = @[
    l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_INSTRUCTIONS_ONE),
    l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_INSTRUCTIONS_TWO),
    l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_INSTRUCTIONS_THREE),
  ];
  InstructionView* instructionsView =
      [[InstructionView alloc] initWithList:instructions];
  instructionsView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.specificContentView addSubview:instructionsView];
  /// Bottom align the instruction view.
  [NSLayoutConstraint activateConstraints:@[
    [instructionsView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
    [instructionsView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [instructionsView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [instructionsView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
  ]];
}

/// Invoked when the user taps the "Cancel" button.
- (void)cancelButtonTapped {
  [self.delegate didTapDismissButton];
}

@end
