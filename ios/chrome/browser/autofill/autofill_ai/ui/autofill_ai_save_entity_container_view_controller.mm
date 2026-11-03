// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_container_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_mutator.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kButtonStackSpacing = 8;
constexpr CGFloat kButtonStackHorizontalMargin = 16;
constexpr CGFloat kButtonStackVerticalMargin = 16;
}  // namespace

@interface AutofillAISaveEntityContainerViewController () <
    AutofillAISaveEntityTableViewControllerDelegate>
@end

@implementation AutofillAISaveEntityContainerViewController {
  // The table view containing the entity attributes.
  AutofillAISaveEntityTableViewController* _tableViewController;

  // The sticky "Save" or "Update" button at the bottom.
  ChromeButton* _saveButton;

  // The sticky "No thanks" button at the bottom for explicit decline.
  ChromeButton* _noThanksButton;

  // The stack containing the action button.
  UIStackView* _buttonStack;

  // Button title.
  NSString* _buttonTitle;

  // Denotes if the save is synchronous.
  BOOL _saveIsSynchronous;

  // Denotes if an explicit decline button should be displayed in the button
  // stack instead of the navigation bar cancel button.
  BOOL _showsExplicitDeclineButton;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _tableViewController = [[AutofillAISaveEntityTableViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
    _tableViewController.delegate = self;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  // Setup the Action Button and Stack View.
  _buttonStack = [[UIStackView alloc] init];
  _buttonStack.axis = UILayoutConstraintAxisVertical;
  _buttonStack.alignment = UIStackViewAlignmentFill;
  _buttonStack.spacing = kButtonStackSpacing;
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;

  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  if (_buttonTitle) {
    [_saveButton setTitle:_buttonTitle forState:UIControlStateNormal];
  }
  _saveButton.accessibilityIdentifier = kAutofillAISaveEntitySaveButtonId;
  [_saveButton addTarget:self
                  action:@selector(didTapSaveButton:)
        forControlEvents:UIControlEventTouchUpInside];
  [_buttonStack addArrangedSubview:_saveButton];

  [self updateLayoutForExplicitDeclineButton];

  UIView* tableView = _tableViewController.view;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;

  [self addChildViewController:_tableViewController];
  // The subviews must be added in this order to have a scroll edge effect.
  [self.view addSubview:tableView];
  [self.view addSubview:_buttonStack];
  [_tableViewController didMoveToParentViewController:self];

  // Layout: Table view on top, button stack pinned to the bottom safe area.
  [NSLayoutConstraint activateConstraints:@[
    [tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [tableView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],

    // Pin bottom of the table view to the top of the button stack.
    [tableView.bottomAnchor
        constraintEqualToAnchor:_buttonStack.topAnchor
                       constant:-kButtonStackVerticalMargin],

    [_buttonStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kButtonStackHorizontalMargin],
    [_buttonStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kButtonStackHorizontalMargin],
    [_buttonStack.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kButtonStackVerticalMargin],
  ]];
}

#pragma mark - AutofillAISaveEntityConsumer

- (void)setNewEntity:(autofill::EntityInstance)newEntity
            oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
            userEmail:(const std::u16string&)userEmail
    saveIsSynchronous:(BOOL)saveIsSynchronous {
  // Forward the data to the table view controller for display.
  [_tableViewController setNewEntity:newEntity
                           oldEntity:oldEntity
                           userEmail:userEmail];

  autofill::EntityTypeName typeName = newEntity.type().name();
  NSString* titleString =
      oldEntity.has_value() ? autofill::GetDialogTitleForUpdateEntity(typeName)
                            : autofill::GetDialogTitleForSaveEntity(typeName);

  if (newEntity.record_type() !=
      autofill::EntityInstance::RecordType::kServerWallet) {
    self.navigationItem.titleView = nil;
    self.title = titleString;
  } else {
    self.navigationItem.titleView =
        autofill::CreateBrandedTitleForWalletSave(titleString);
  }
  _saveIsSynchronous = saveIsSynchronous;

  // Update the button title based on whether it's an update or save.
  _buttonTitle = l10n_util::GetNSString(
      oldEntity.has_value() ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
                            : autofill::GetSaveEntityAcceptButtonStringId());

  if (_saveButton) {
    [_saveButton setTitle:_buttonTitle forState:UIControlStateNormal];
  }
}

- (void)setIsPrivateWalletPass:(BOOL)isPrivateWalletPass {
  [_tableViewController setIsPrivateWalletPass:isPrivateWalletPass];
}

- (void)setShowsExplicitDeclineButton:(BOOL)showsExplicitDeclineButton {
  if (_showsExplicitDeclineButton == showsExplicitDeclineButton) {
    return;
  }
  _showsExplicitDeclineButton = showsExplicitDeclineButton;
  if (self.isViewLoaded) {
    [self updateLayoutForExplicitDeclineButton];
  }
}

- (void)showLoadingState {
  _saveButton.enabled = NO;
  _noThanksButton.enabled = NO;

  [_saveButton setTitle:@"" forState:UIControlStateNormal];
  _saveButton.tunedDownStyle = YES;
  _saveButton.primaryButtonImage = PrimaryButtonImageSpinner;

  _saveButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_AI_WALLET_UPLOAD_THROBBER_ACCESSIBLE_NAME);

  // Prevent dismissing the bottomsheet while the view is in loading state.
  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.modalInPresentation = YES;
}

- (void)showConfirmationState {
  _saveButton.primaryButtonImage = PrimaryButtonImageCheckmark;

  [_saveButton setTitle:@"" forState:UIControlStateNormal];

  _saveButton.enabled = NO;
  _noThanksButton.enabled = NO;

  _saveButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_AI_INFO_SAVED_TO_WALLET_ACCESSIBILITY_LABEL);

  UIAccessibilityPostNotification(
      UIAccessibilityAnnouncementNotification,
      l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_INFO_SAVED_TO_WALLET_ACCESSIBILITY_LABEL));

  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.modalInPresentation = NO;
}

- (void)setLegalMessages:(NSArray<AutofillLegalMessageLine*>*)legalMessages {
  [_tableViewController setLegalMessages:legalMessages];
}

#pragma mark - Private

// Updates navigation and action buttons based on `_showsExplicitDeclineButton`.
- (void)updateLayoutForExplicitDeclineButton {
  if (_showsExplicitDeclineButton) {
    self.navigationItem.leftBarButtonItem = nil;
    if (!_noThanksButton) {
      _noThanksButton = [self createNoThanksButton];
      [_buttonStack addArrangedSubview:_noThanksButton];
    }
  } else {
    if (!self.navigationItem.leftBarButtonItem) {
      self.navigationItem.leftBarButtonItem = [self createCancelButton];
    }
    if (_noThanksButton) {
      [_noThanksButton removeFromSuperview];
      _noThanksButton = nil;
    }
  }
}

// Creates the cancel navigation bar button item.
- (UIBarButtonItem*)createCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  cancelButton.enabled = _saveButton ? _saveButton.enabled : YES;
  cancelButton.accessibilityIdentifier = kAutofillAISaveEntityCancelButtonId;
  return cancelButton;
}

// Creates the sticky "No thanks" action button.
- (ChromeButton*)createNoThanksButton {
  ChromeButton* button =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  button.enabled = _saveButton ? _saveButton.enabled : YES;
  [button
      setTitle:
          l10n_util::GetNSString(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON)
      forState:UIControlStateNormal];
  button.accessibilityIdentifier = kAutofillAISaveEntityNoThanksButtonId;
  [button addTarget:self
                action:@selector(didTapNoThanksButton:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

#pragma mark - Actions

- (void)didTapCancelButton {
  [self.mutator cancelSaving];
}

- (void)didTapNoThanksButton:(UIButton*)sender {
  [self.mutator cancelSaving];
}

- (void)didTapSaveButton:(UIButton*)sender {
  [self.mutator acceptSaving];
}

#pragma mark - AutofillAISaveEntityTableViewControllerDelegate

- (void)didTapLinkWithURL:(CrURL*)url {
  [self.delegate didTapLinkWithURL:url];
}

@end
