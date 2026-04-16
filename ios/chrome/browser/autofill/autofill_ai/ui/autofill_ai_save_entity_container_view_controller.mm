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
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
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

  // The stack containing the action button.
  UIStackView* _buttonStack;

  // Tracks if the button is currently enabled.
  BOOL _saveButtonEnabled;

  // Button title.
  NSString* _buttonTitle;

  // Denotes if the save is local (synchronous).
  BOOL _isLocalSave;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _saveButtonEnabled = YES;
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

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
  cancelButton.accessibilityIdentifier = kAutofillAISaveEntityCancelButtonId;
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  // Setup the Action Button and Stack View.
  _buttonStack = [[UIStackView alloc] init];
  _buttonStack.axis = UILayoutConstraintAxisVertical;
  _buttonStack.alignment = UIStackViewAlignmentFill;
  _buttonStack.spacing = kButtonStackSpacing;
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;

  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _saveButton.enabled = _saveButtonEnabled;
  if (_buttonTitle) {
    [_saveButton setTitle:_buttonTitle forState:UIControlStateNormal];
  }
  _saveButton.accessibilityIdentifier = kAutofillAISaveEntitySaveButtonId;
  [_saveButton addTarget:self
                  action:@selector(saveButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  [_buttonStack addArrangedSubview:_saveButton];

  [self.view addSubview:_buttonStack];

  UIView* tableView = _tableViewController.view;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;

  [self addChildViewController:_tableViewController];
  [self.view addSubview:tableView];
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
         isLocalSave:(BOOL)isLocalSave {
  // Forward the data to the table view controller for display.
  [_tableViewController setNewEntity:newEntity
                           oldEntity:oldEntity
                           userEmail:userEmail];

  autofill::EntityTypeName typeName = newEntity.type().name();
  NSString* titleString =
      oldEntity.has_value() ? autofill::GetDialogTitleForUpdateEntity(typeName)
                            : autofill::GetDialogTitleForSaveEntity(typeName);

  if (isLocalSave) {
    self.navigationItem.titleView = nil;
    self.title = titleString;
  } else {
    self.navigationItem.titleView =
        autofill::CreateBrandedTitleForWalletSave(titleString);
  }
  _isLocalSave = isLocalSave;

  // Update the button title based on whether it's an update or save.
  _buttonTitle = l10n_util::GetNSString(
      oldEntity.has_value() ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
                            : autofill::GetSaveEntityAcceptButtonStringId());

  if (_saveButton) {
    [_saveButton setTitle:_buttonTitle forState:UIControlStateNormal];
  }
}

- (void)showLoadingState {
  _saveButton.enabled = NO;
  _saveButtonEnabled = NO;

  _saveButton.title = @"";
  _saveButton.tunedDownStyle = YES;
  _saveButton.primaryButtonImage = PrimaryButtonImageSpinner;

  _saveButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_AI_WALLET_UPLOAD_THROBBER_ACCESSIBLE_NAME);

  // Prevent dismissing the bottomsheet while the view is in loading state.
  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.modalInPresentation = YES;
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.mutator cancelSaving];
  [self.autofillHandler dismissSaveEntityDialog];
}

- (void)saveButtonWasPressed:(UIButton*)sender {
  // Early return if the save button is disabled.
  if (!_saveButtonEnabled) {
    return;
  }
  [self.mutator acceptSaving];
  // Only dismiss immediately if it's a synchronous local save.
  // Otherwise, the UI stays open to show the loading state. Once the async call
  // is completed, the UI is informed and the loading state is dismissed.
  if (_isLocalSave) {
    [self.autofillHandler dismissSaveEntityDialog];
  }
}

#pragma mark - AutofillAISaveEntityTableViewControllerDelegate

- (void)didTapLinkWithURL:(CrURL*)url {
  [self.delegate didTapLinkWithURL:url];
}

@end
