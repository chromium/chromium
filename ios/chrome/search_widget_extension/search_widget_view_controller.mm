// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// This constant indicates is horizontal margin between the current view and the
// content.
const CGFloat kLeadingAnchorConstant = 10;
}  // namespace

@implementation SearchWidgetViewController

#pragma mark - UIViewController

+ (void)initialize {
  if (self == [SearchWidgetViewController self]) {
    crash_helper::common::StartCrashpad();
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  UIVibrancyEffect* labelEffect = nil;
  labelEffect = [UIVibrancyEffect
      widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];

  UIVisualEffectView* titleLabelEffectView =
      [[UIVisualEffectView alloc] initWithEffect:labelEffect];
  titleLabelEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:titleLabelEffectView];
  AddSameConstraints(self.view, titleLabelEffectView);

  UILabel* updateExtensionLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  updateExtensionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  updateExtensionLabel.textAlignment = NSTextAlignmentCenter;
  updateExtensionLabel.numberOfLines = 0;
  updateExtensionLabel.userInteractionEnabled = NO;
  updateExtensionLabel.text =
      NSLocalizedString(@"IDS_IOS_SEARCH_WIDGET_LABEL", @"");
  updateExtensionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [titleLabelEffectView.contentView addSubview:updateExtensionLabel];

  [NSLayoutConstraint activateConstraints:@[
    [updateExtensionLabel.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [updateExtensionLabel.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [updateExtensionLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.leadingAnchor
                                    constant:kLeadingAnchorConstant]
  ]];
}
@end
