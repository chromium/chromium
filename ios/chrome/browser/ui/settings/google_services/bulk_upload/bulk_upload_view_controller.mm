// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mutator.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface BulkUploadViewController () <SettingsControllerProtocol>
@end

namespace {

// The horizontal space between the safe area edges and the view elements.
constexpr CGFloat kHorizontalInsets = 48.;
constexpr CGFloat kSaveInAccoutButtonVerticalMargin = 10.;

// User action when the bulk upload view is closed.
const char kBulkUploadCloseUserAction[] = "Signin_BulkUpload_Close";

}  // namespace

@implementation BulkUploadViewController {
  BulkUploadTableViewController* _tableViewController;
  // The button to trigger the bulk upload.
  UIButton* _saveInAccountButton;
  // Stored as a separate field because it can be set before
  // _saveInAccountButton is instantiated.
  BOOL _saveInAccountButtonEnabled;
  // List of items to display.
  NSArray<BulkUploadViewItem*>* _viewItems;
}

#pragma mark UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.systemGroupedBackgroundColor;
  // Set the bulk upload page title.
  self.title = l10n_util::GetNSString(IDS_IOS_BULK_UPLOAD_ON_THIS_DEVICE_TITLE);
  // Create the table view.
  _tableViewController = [[BulkUploadTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _tableViewController.mutator = self.mutator;
  [self addChildViewController:_tableViewController];
  UIView* tableView = _tableViewController.view;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:tableView];
  [_tableViewController didMoveToParentViewController:self];
  // Create the save in account button.
  _saveInAccountButton =
      PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  _saveInAccountButton.accessibilityIdentifier =
      kBulkUploadSaveButtonAccessibilityIdentifer;
  SetConfigurationTitle(
      _saveInAccountButton,
      l10n_util::GetNSString(IDS_IOS_BULK_UPLOAD_BUTTON_TITLE));
  _saveInAccountButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_saveInAccountButton addTarget:self
                           action:@selector(saveInAccountTapped:)
                 forControlEvents:UIControlEventTouchUpInside];
  // setValidationButtonEnabled might have been called before the button was
  // created.
  [self updateSaveInAccountButton];
  [self.view addSubview:_saveInAccountButton];
  // Create the Cancel button.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton:)];
  // Add constraints.
  [NSLayoutConstraint activateConstraints:@[
    [tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [tableView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [tableView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [tableView.bottomAnchor
        constraintEqualToAnchor:_saveInAccountButton.topAnchor
                       constant:-kSaveInAccoutButtonVerticalMargin],
    [_saveInAccountButton.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    [_saveInAccountButton.widthAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.widthAnchor
                       constant:-kHorizontalInsets],
    [_saveInAccountButton.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-kSaveInAccoutButtonVerticalMargin],
    [_saveInAccountButton.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kSaveInAccoutButtonVerticalMargin *
                                          2],
  ]];
  // Also add constraint for the save button view and the bottom of the safe
  // area, but with a lower priority, so that the save button view is put as
  // close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint =
      [_saveInAccountButton.bottomAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;
  [_tableViewController updateViewWithViewItems:_viewItems];
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  if (!parent) {
    [self settingsWillBeDismissed];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction(kBulkUploadCloseUserAction));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction(kBulkUploadCloseUserAction));
}

- (void)settingsWillBeDismissed {
  [self.delegate viewControllerIsBeingDismissed:self];
}

#pragma mark - BulkUploadConsumer

- (void)updateViewWithViewItems:(NSArray<BulkUploadViewItem*>*)viewItems {
  _viewItems = [viewItems copy];
  [_tableViewController updateViewWithViewItems:_viewItems];
}

- (void)setValidationButtonEnabled:(BOOL)enabled {
  _saveInAccountButtonEnabled = enabled;
  [self updateSaveInAccountButton];
}

#pragma mark - Private

// Updates the state of `_saveInAccountButton` according to
// `_saveInAccountButtonEnabled`.
- (void)updateSaveInAccountButton {
  UIButtonConfiguration* buttonConfiguration =
      _saveInAccountButton.configuration;
  if (_saveInAccountButtonEnabled) {
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kBlueColor];
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
  } else {
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kUpdatedTertiaryBackgroundColor];
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kDisabledTintColor];
  }
  _saveInAccountButton.configuration = buttonConfiguration;
  _saveInAccountButton.enabled = _saveInAccountButtonEnabled;
}

- (void)saveInAccountTapped:(UIButton*)button {
  [self.mutator requestSave];
}

- (void)didTapCancelButton:(UIButton*)button {
  [self.delegate viewControllerWantsToBeDismissed:self];
}

@end
