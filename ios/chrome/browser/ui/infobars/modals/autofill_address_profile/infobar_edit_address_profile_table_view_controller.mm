// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarEditAddressProfileTableViewController () <UITextFieldDelegate>

// The delegate passed to this instance.
@property(nonatomic, weak) id<InfobarModalDelegate> delegate;

// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Yes, if the edit is done for updating the profile.
@property(nonatomic, assign) BOOL isEditForUpdate;

// Yes, if the edit is shown for the migration prompt.
@property(nonatomic, assign) BOOL migrationPrompt;

@end

@implementation InfobarEditAddressProfileTableViewController

#pragma mark - Initialization

- (instancetype)initWithModalDelegate:(id<InfobarModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _delegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.estimatedRowHeight = 56;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;
  if (self.migrationPrompt) {
    self.navigationItem.title = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE);
  } else {
    self.navigationItem.title = l10n_util::GetNSString(
        self.isEditForUpdate ? IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE
                             : IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  self.tableView.allowsSelectionDuringEditing = YES;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  [self.handler setMigrationPrompt:self.migrationPrompt];
  [self.handler loadModel];
  [self.handler
      loadMessageAndButtonForModalIfSaveOrUpdate:self.isEditForUpdate];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  return [self.handler cell:cell
          forRowAtIndexPath:indexPath
           withTextDelegate:self];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.handler didSelectRowAtIndexPath:indexPath];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.handler heightForHeaderShouldBeZeroInSection:section]) {
    return 0;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.handler heightForFooterShouldBeZeroInSection:section]) {
    return 0;
  }
  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - InfobarEditAddressProfileModalConsumer

- (void)setupModalViewControllerWithData:(NSDictionary*)data {
}

#pragma mark - Actions

- (void)handleCancelButton {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.delegate dismissInfobarModal:self];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return NO;
}

@end
