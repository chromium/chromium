// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/data_import/ui/import_data_item_table_view.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/public/ui_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation SafariDataImportImportViewController {
  /// Disclaimer text above the button after import items are ready and before
  /// they are imported.
  UITextView* _disclaimer;
}

- (void)viewDidLoad {
  _importStage = SafariDataImportStage::kNotStarted;
  self.bannerName = @"safari_data_import";
  self.shouldHideBanner = IsCompactHeight(self.traitCollection);
  self.titleText = l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_SUBTITLE);
  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_SELECT_YOUR_FILE);
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  [super viewDidLoad];
  [self makeBannerImageVisibilityAdaptive];
  [self showInstructionView];
}

- (UIFontTextStyle)titleLabelFontTextStyle {
  return GetSafariDataImportTitleLabelFontTextStyle(self.traitCollection);
}

#pragma mark - Accessor

- (void)setImportStage:(SafariDataImportStage)stage {
  switch (stage) {
    case SafariDataImportStage::kNotStarted:
      /// Currently, this view controller does NOT support transitioning back to
      /// `kNotStarted` once the table view is displayed.
      CHECK_LT(static_cast<int>(_importStage),
               static_cast<int>(SafariDataImportStage::kReadyForImport));
      self.configuration.primaryActionString = l10n_util::GetNSString(
          IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_SELECT_YOUR_FILE);
      self.configuration.loading = NO;
      [self.itemTableView reset];
      break;
    case SafariDataImportStage::kFileLoading:
      self.configuration.loading = YES;
      break;
    case SafariDataImportStage::kReadyForImport:
      [self showTableView];
      [self showDisclaimerForEligibleUser];
      self.configuration.primaryActionString = l10n_util::GetNSString(
          IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_IMPORT);
      self.configuration.loading = NO;
      break;
    case SafariDataImportStage::kImporting:
      self.configuration.primaryActionEnabled = NO;
      self.navigationItem.hidesBackButton = YES;
      self.navigationItem.rightBarButtonItem = nil;
      [self.itemTableView notifyImportStart];
      break;
    case SafariDataImportStage::kImported:
      [_disclaimer removeFromSuperview];
      self.configuration.primaryActionString = l10n_util::GetNSString(
          IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_DONE);
      self.configuration.primaryActionEnabled = YES;
      break;
  }
  _importStage = stage;
  [self reloadConfiguration];
}

#pragma mark - Private

/// The banner image should be hidden on iPhone landscape mode. This method
/// makes sure of that when the user rotates the device.
- (void)makeBannerImageVisibilityAdaptive {
  NSArray<UITrait>* traits =
      TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.class ]);
  __weak __typeof(self) weakSelf = self;
  UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                   UITraitCollection* previousCollection) {
    weakSelf.shouldHideBanner = IsCompactHeight(traitEnvironment);
  };
  [self registerForTraitChanges:traits withHandler:handler];
}

/// Adds an instructional view to show import steps  to the view hierarchy and
/// position it.
- (void)showInstructionView {
  CHECK_EQ(static_cast<int>(self.specificContentView.subviews.count), 0);
  /// Creates the instructions view.
  NSArray* instructions = @[
    l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_INSTRUCTIONS_ONE),
    l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_IMPORT_INSTRUCTIONS_TWO),
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

/// Displays the table view containing progress for each Safari import item.
- (void)showTableView {
  CHECK_EQ(static_cast<int>(self.specificContentView.subviews.count), 1);
  CHECK(self.itemTableView);
  /// Removes the instruction view first.
  [self.specificContentView.subviews[0] removeFromSuperview];
  /// Displays the table view.
  ImportDataItemTableView* tableView = self.itemTableView;
  [self.specificContentView addSubview:tableView];
  /// Top align the table view.
  [NSLayoutConstraint activateConstraints:@[
    [tableView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [tableView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
    [tableView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [tableView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
  ]];
}

/// Displays the disclaimer informing user that the Safari items will be
/// imported to their account store.
- (void)showDisclaimerForEligibleUser {
  if (!self.email) {
    return;
  }
  CHECK(self.itemTableView.superview);
  UITextView* disclaimer = CreateUITextViewWithTextKit1();
  disclaimer.scrollEnabled = NO;
  disclaimer.editable = NO;
  disclaimer.textColor = [UIColor colorNamed:kTextSecondaryColor];
  disclaimer.backgroundColor = UIColor.clearColor;
  disclaimer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  disclaimer.textAlignment = NSTextAlignmentCenter;
  disclaimer.adjustsFontForContentSizeCategory = YES;
  disclaimer.translatesAutoresizingMaskIntoConstraints = NO;
  disclaimer.text = l10n_util::GetNSStringF(
      IDS_IOS_SAFARI_IMPORT_IMPORT_ITEM_TYPE_PENDING_DISCLAIMER,
      base::SysNSStringToUTF16(self.email));
  [self.specificContentView addSubview:disclaimer];
  /// Bottom align the disclaimer view.
  [NSLayoutConstraint activateConstraints:@[
    [disclaimer.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.itemTableView.bottomAnchor],
    [disclaimer.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [disclaimer.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [disclaimer.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
  ]];
  _disclaimer = disclaimer;
}

/// Invoked when the user taps the "Cancel" button.
- (void)cancelButtonTapped {
  [self.delegate didTapDismissButton];
}

@end
