// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_export_view_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Video animation asset names.
NSString* const kSafariDataExportEducationAnimation =
    @"safari_data_export_education_light";
NSString* const kSafariDataExportEducationAnimationRtl =
    @"safari_data_export_education_light_rtl";
NSString* const kSafariDataExportEducationAnimationDarkmode =
    @"safari_data_export_education_dark";
NSString* const kSafariDataExportEducationAnimationRtlDarkmode =
    @"safari_data_export_education_dark_rtl";

NSString* GetAnimationName(BOOL dark_mode) {
  if (base::i18n::IsRTL()) {
    return dark_mode ? kSafariDataExportEducationAnimationRtlDarkmode
                     : kSafariDataExportEducationAnimationRtl;
  }
  return dark_mode ? kSafariDataExportEducationAnimationDarkmode
                   : kSafariDataExportEducationAnimation;
}


/// Static text instructions to export data from Safari, formatted with
/// paddings.
UIView* GetInstructionsView() {
  /// Creates the instructions.
  InstructionView* instruction_view = [[InstructionView alloc] initWithList:@[
    l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_STATIC_INSTRUCTION_STEP_1),
    l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_STATIC_INSTRUCTION_STEP_2),
    l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_STATIC_INSTRUCTION_STEP_3),
    l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_STATIC_INSTRUCTION_STEP_4),
    l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_STATIC_INSTRUCTION_STEP_5),
  ]];
  instruction_view.translatesAutoresizingMaskIntoConstraints = NO;
  return instruction_view;
}

/// Text provider for the animated promo.
NSDictionary<NSString*, NSString*>* GetTextProvider() {
  return @{
    @"safari" : l10n_util::GetNSString(IDS_IOS_SAFARI),
    @"page-1-title" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_1_TITLE),
    @"page-2-item-import" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_IMPORT),
    @"page-2-item-export" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_EXPORT),
    @"page-2-item-clear" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_CLEAR_DATA),
    @"page-3-title" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_3_TITLE),
    @"page-3-button" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_3_BUTTON),
    @"page-4-title" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_4_TITLE),
  };
}

}  // namespace

@implementation SafariDataImportExportViewController

- (void)viewDidLoad {
  /// Sets up the safari data import item.
  self.animationName = GetAnimationName(/*dark_mode=*/NO);
  self.animationNameDarkMode = GetAnimationName(/*dark_mode=*/YES);
  self.animationBackgroundColor = [UIColor colorNamed:kBlueHaloColor];
  self.underTitleView = GetInstructionsView();
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_EXPORT_BUTTON_GO_TO_SETTINGS);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_EXPORT_BUTTON_CONTINUE);
  self.animationTextProvider = GetTextProvider();
  [super viewDidLoad];
}

@end
