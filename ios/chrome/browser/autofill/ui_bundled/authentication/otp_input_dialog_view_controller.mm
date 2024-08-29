// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_view_controller.h"

#import "base/check.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_runner.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_content.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_view_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/card_unmask_header_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
  SectionIdentifierError,
  SectionIdentifierNewCodeLink,
};

typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemTypeTextField = kItemTypeEnumZero,
};

// Dummy URL used as target of the link in the new code link.
constexpr char kDummyLinkTarget[] = "about:blank";

// The cooldown time for the "Get new code" link after it is clicked.
constexpr base::TimeDelta kNewCodeLinkCooldownTime = base::Seconds(5);

}  // namespace

@interface OtpInputDialogViewController () <
    TableViewLinkHeaderFooterItemDelegate,
    UITableViewDelegate,
    UITextFieldDelegate> {
}

@end

@implementation OtpInputDialogViewController {
  OtpInputDialogContent* _content;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  BOOL _contentSet;
  NSString* _inputValue;
  NSString* _errorTitle;
  BOOL _shouldEnableNewCodeLink;
}

- (instancetype)init {
  if ((self = [super initWithStyle:UITableViewStyleInsetGrouped])) {
    _shouldEnableNewCodeLink = YES;
  }
  return self;
}

#pragma mark - ChromeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_NAVIGATION_TITLE_VERIFICATION);
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  self.navigationItem.rightBarButtonItem = [self createConfirmButton];
  [self loadModel];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierContent:
    case SectionIdentifierNewCodeLink:
      return UITableViewAutomaticDimension;
    case SectionIdentifierError:
      return ChromeTableViewHeightForHeaderInSection(sectionIdentifier);
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierContent: {
      CardUnmaskHeaderView* view =
          DequeueTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
      view.titleLabel.text = _content.windowTitle;
      return view;
    }
    case SectionIdentifierError: {
      if (!_errorTitle) {
        return nil;
      }
      TableViewTextHeaderFooterView* errorMessage =
          DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
              self.tableView);
      [errorMessage setSubtitle:_errorTitle
                      withColor:[UIColor colorNamed:kRedColor]];
      [errorMessage setForceIndents:YES];
      errorMessage.accessibilityIdentifier =
          kOtpInputErrorMessageAccessibilityIdentifier;
      return errorMessage;
    }
    case SectionIdentifierNewCodeLink: {
      return [self createNewCodeLink];
    }
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [_mutator didTapNewCodeLink];
  [self setEnableNewCodeLink:NO];
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf setEnableNewCodeLink:YES];
      }),
      kNewCodeLinkCooldownTime);
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setContent:(OtpInputDialogContent*)content {
  // Content should not be updated once initialized.
  CHECK(!_contentSet);
  _content = content;
  _contentSet = YES;
}

- (void)setConfirmButtonEnabled:(BOOL)enabled {
  self.navigationItem.rightBarButtonItem.enabled = enabled;
}

- (void)showPendingState {
  UIActivityIndicatorView* activityIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  UIBarButtonItem* pendingButton =
      [[UIBarButtonItem alloc] initWithCustomView:activityIndicator];
  pendingButton.accessibilityIdentifier =
      kOtpInputNavigationBarPendingButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = pendingButton;
  [activityIndicator startAnimating];
  [self.tableView setUserInteractionEnabled:NO];
}

- (void)showInvalidState:(NSString*)invalidLabelText {
  self.navigationItem.rightBarButtonItem = [self createConfirmButton];
  _errorTitle = invalidLabelText;
  [self.tableView setUserInteractionEnabled:YES];
  [self reloadModel];
}

#pragma mark - Private

// Helper function to load the model to the data source.
- (void)loadModel {
  CHECK(_contentSet);
  RegisterTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
  RegisterTableViewCell<TableViewTextEditCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierContent) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemTypeTextField) ]
             intoSectionWithIdentifier:@(SectionIdentifierContent)];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierError) ]];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierNewCodeLink) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)reloadModel {
  NSDiffableDataSourceSnapshot* snapshot = [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ @(ItemTypeTextField) ]];
  [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierError) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Returns the appropriate cell for the table view.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  TableViewTextEditCell* cell =
      DequeueTableViewCell<TableViewTextEditCell>(self.tableView);
  [cell setIdentifyingIcon:nil];
  if (_errorTitle) {
    [cell setIcon:TableViewTextEditItemIconTypeError];
    cell.textField.text = _inputValue;
    cell.textField.textColor = [UIColor colorNamed:kRedColor];
  } else {
    [cell setIcon:TableViewTextEditItemIconTypeEdit];
    cell.textField.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  cell.textField.placeholder = _content.textFieldPlaceholder;
  cell.textField.accessibilityIdentifier =
      kOtpInputTextfieldAccessibilityIdentifier;
  [cell.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.keyboardType = UIKeyboardTypeNumberPad;
  cell.textField.returnKeyType = UIReturnKeyDone;
  cell.textField.textAlignment = NSTextAlignmentLeft;
  return cell;
}

- (void)textFieldDidChange:(UITextField*)textField {
  // Reset error state.
  if (_errorTitle) {
    _errorTitle = nil;
    [self reloadModel];
  }

  _inputValue = textField.text;
  [self didChangeOtpInputText];
}

// Invoked when the confirm button in the navigation bar is tapped by the user.
// This means a valid OTP value is typed in.
- (void)didTapConfirmButton {
  [_mutator didTapConfirmButton:_inputValue];
}

// Invoked when the cancel button in the navigation bar is tapped by the user.
- (void)didTapCancelButton {
  [_mutator didTapCancelButton];
}

// Notify the model controller when the OTP input value changes.
- (void)didChangeOtpInputText {
  [_mutator onOtpInputChanges:_inputValue];
}

- (UIBarButtonItem*)createConfirmButton {
  UIBarButtonItem* confirmButton =
      [[UIBarButtonItem alloc] initWithTitle:_content.confirmButtonLabel
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapConfirmButton)];
  // Enable the confirm button only after a valid OTP has been entered.
  confirmButton.enabled = NO;
  confirmButton.accessibilityIdentifier =
      kOtpInputNavigationBarConfirmButtonAccessibilityIdentifier;
  return confirmButton;
}

// Create a link and when tapped it will request a new OTP code from the server.
// Upon clicking the link will be disabled for a short period of time.
- (TableViewLinkHeaderFooterView*)createNewCodeLink {
  TableViewLinkHeaderFooterView* newCodeLink =
      DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(
          self.tableView);
  // Using a dummy target for the link in the footer.
  // The link target is ignored and taps on it are handled by `didTapLinkURL`.
  newCodeLink.urls = @[ [[CrURL alloc] initWithGURL:GURL(kDummyLinkTarget)] ];
  newCodeLink.delegate = self;
  newCodeLink.accessibilityIdentifier = kOtpInputFooterAccessibilityIdentifier;
  [newCodeLink
            setText:l10n_util::GetNSString(
                        IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_FOOTER_LINK)
          withColor:[UIColor colorNamed:(kTextSecondaryColor)]
      textAlignment:NSTextAlignmentCenter];
  [newCodeLink setLinkEnabled:_shouldEnableNewCodeLink];

  return newCodeLink;
}

- (void)setEnableNewCodeLink:(BOOL)enabled {
  _shouldEnableNewCodeLink = enabled;
  NSDiffableDataSourceSnapshot* snapshot = [_dataSource snapshot];
  [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierNewCodeLink) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

@end
