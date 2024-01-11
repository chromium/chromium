// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller_presentation_delegate.h"
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

  UINavigationItem* navigationItem = self.navigationItem;
  navigationItem.leftBarButtonItem = [self createCancelButton];
  navigationItem.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  navigationItem.rightBarButtonItem = [self createShareButton];

  UIView* view = self.view;
  UIStackView* stackView = [self createStackView];
  [view addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
    [stackView.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
  ]];
}

#pragma mark - Private

// Notifies the delegate that the view should be dismissed.
- (void)cancelButtonTapped {
  [self.delegate sharingSpinnerViewWasDismissed:self];
}

// Creates left cancel button that closes the view.
- (UIBarButtonItem*)createCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  return cancelButton;
}

// Creates right share button disabled by default.
- (UIBarButtonItem*)createShareButton {
  UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_SHARE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:nil];
  shareButton.enabled = NO;
  return shareButton;
}

// Creates label explaining that the sharing recipients are being fetched.
- (UILabel*)createLabel {
  UILabel* label = [[UILabel alloc] init];
  label.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_LABEL);
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

// Creates a stack view with a spinner and a label below it.
- (UIStackView*)createStackView {
  UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
  spinner.translatesAutoresizingMaskIntoConstraints = NO;
  [spinner startAnimating];

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ spinner, [self createLabel] ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kTableViewSubViewHorizontalSpacing;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  return stackView;
}

@end
