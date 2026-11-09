// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_export_view_controller.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
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
  instruction_view.accessibilityIdentifier =
      kSafariDataExportInstructionsViewAccessibilityIdentifier;
  return instruction_view;
}

/// Text provider for the animated promo.
NSDictionary<NSString*, NSString*>* GetTextProvider() {
  return @{
    @"chapter-01.title" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_1_TITLE),
    @"chapter-02.item-import" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_IMPORT),
    @"chapter-02.item-export" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_EXPORT),
    @"chapter-02.item-clear" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_2_ITEM_CLEAR_DATA),
    @"chapter-03.title-1" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_3_TITLE),
    @"chapter-03.button" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_3_BUTTON),
    @"chapter-03.title-2" : l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_EXPORT_ANIMATED_INSTRUCTION_PAGE_4_TITLE),
    @"chapter-01.safari" : l10n_util::GetNSString(IDS_IOS_SAFARI),
    @"chapter-02.safari" : l10n_util::GetNSString(IDS_IOS_SAFARI),
  };
}

}  // namespace

@implementation SafariDataImportExportViewController

- (instancetype)init {
  ButtonStackConfiguration* buttonStackConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonStackConfiguration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_EXPORT_BUTTON_GO_TO_SETTINGS);
  buttonStackConfiguration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_SAFARI_IMPORT_EXPORT_BUTTON_CONTINUE);

  AnimationBrickConfiguration* animationConfiguration =
      [[AnimationBrickConfiguration alloc] init];
  animationConfiguration.animationName = GetAnimationName(/*dark_mode=*/NO);
  animationConfiguration.darkAnimationName =
      GetAnimationName(/*dark_mode=*/YES);
  animationConfiguration.animationBackgroundColor =
      [UIColor colorNamed:kBlueHaloColor];
  animationConfiguration.animationTextProvider = GetTextProvider();

  AnimatedPromoConfiguration* config = [[AnimatedPromoConfiguration alloc]
      initWithAnimationBrickConfiguration:animationConfiguration
                 buttonStackConfiguration:buttonStackConfiguration
                  titleBrickConfiguration:nil
                           underTitleView:GetInstructionsView()];

  return [super initWithConfiguration:config];
}

@end
