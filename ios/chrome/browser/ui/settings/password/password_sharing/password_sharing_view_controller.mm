// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PasswordSharingViewController ()
@end

@implementation PasswordSharingViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_SHARE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:nil];
  shareButton.enabled = NO;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  self.navigationItem.rightBarButtonItem = shareButton;

  UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
  spinner.translatesAutoresizingMaskIntoConstraints = NO;
  [spinner startAnimating];

  UILabel* label = [[UILabel alloc] init];
  label.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_LABEL);
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ spinner, label ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kTableViewSubViewHorizontalSpacing;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  [self.view addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [stackView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

#pragma mark - private

- (void)cancelButtonTapped {
  // TODO(crbug.com/1463882): Add handling cancel taps.
}

@end
