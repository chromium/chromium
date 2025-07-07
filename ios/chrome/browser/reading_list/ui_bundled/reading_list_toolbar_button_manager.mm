// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_toolbar_button_manager.h"

#import "base/check.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_constants.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_toolbar_button_commands.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Size of the delete symbol.
const CGFloat kSymbolSize = 22;
// Horizontal margin between the buttons and their container.
const CGFloat kButtonHorizontalMargin = 4;

// Returns the title to use for the "Mark" button for `state`.
NSString* GetMarkButtonTitleForSelectionState(ReadingListSelectionState state) {
  switch (state) {
    case ReadingListSelectionState::NONE:
      return l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
    case ReadingListSelectionState::ONLY_READ_ITEMS:
      return l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
    case ReadingListSelectionState::ONLY_UNREAD_ITEMS:
      return l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
    case ReadingListSelectionState::READ_AND_UNREAD_ITEMS:
      return l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_BUTTON);
  }
}

// Returns a new button to be used in UIBarButtonItem. UIButton have to be used
// instead of relying on UIBarButtonItem to allow the width to be limited.
UIButton* CreateButton(NSString* title) {
  UIButton* button = [[UIButton alloc] init];
  [button setTitle:title forState:UIControlStateNormal];
  if (@available(iOS 26, *)) {
    [button setTitleColor:[UIColor colorNamed:kSolidBlackColor]
                 forState:UIControlStateNormal];
  } else {
    [button setTitleColor:[UIColor colorNamed:kBlueColor]
                 forState:UIControlStateNormal];
  }
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
  // Using those requires the configuration to not be set.
  button.titleLabel.numberOfLines = 1;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;

  return button;
}

// Returns a container for the button, adding horizontal margins. The
// configuration of the button cannot be used as it would prevent the
// adjustsFontSizeToFitWidth from working.
UIView* ContainerForButton(UIView* button) {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:button];
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor constraintEqualToAnchor:container.leadingAnchor
                                         constant:kButtonHorizontalMargin],
    [button.topAnchor constraintEqualToAnchor:container.topAnchor],
    [container.trailingAnchor constraintEqualToAnchor:button.trailingAnchor
                                             constant:kButtonHorizontalMargin],
    [container.bottomAnchor constraintEqualToAnchor:button.bottomAnchor],
  ]];
  return container;
}

}  // namespace

@interface ReadingListToolbarButtonManager ()

// The possible button items that may be returned by the `-buttonItems`.
@property(nonatomic, strong, readonly) UIBarButtonItem* editButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* deleteButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* deleteAllReadButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* cancelButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* markButton;

// Whether the corresponding button items should be returned in `-buttonItems`.
@property(nonatomic, readonly) BOOL shouldShowEditButton;
@property(nonatomic, readonly) BOOL shouldShowDeleteButton;
@property(nonatomic, readonly) BOOL shouldShowDeleteAllReadButton;
@property(nonatomic, readonly) BOOL shouldShowCancelButton;
@property(nonatomic, readonly) BOOL shouldShowMarkButton;

@end

@implementation ReadingListToolbarButtonManager {
  // The button items corresponding to the current state.
  NSMutableArray<UIBarButtonItem*>* _buttonItems;
  NSMutableArray<NSLayoutConstraint*>* _allButtonWidthConstraints;
  UIButton* _editInnerButton;
  UIButton* _deleteAllReadInnerButton;
  UIButton* _markInnerButton;
}

@synthesize selectionState = _selectionState;
@synthesize hasReadItems = _hasReadItems;
@synthesize editing = _editing;
@synthesize commandHandler = _commandHandler;
@synthesize editButton = _editButton;
@synthesize deleteButton = _deleteButton;
@synthesize deleteAllReadButton = _deleteAllReadButton;
@synthesize cancelButton = _cancelButton;
@synthesize markButton = _markButton;

- (instancetype)init {
  if ((self = [super init])) {
    _allButtonWidthConstraints = [NSMutableArray array];

    _selectionState = ReadingListSelectionState::NONE;

    _editInnerButton =
        CreateButton(l10n_util::GetNSString(IDS_IOS_READING_LIST_EDIT_BUTTON));
    UIView* editContainer = ContainerForButton(_editInnerButton);
    [_allButtonWidthConstraints
        addObject:[editContainer.widthAnchor
                      constraintLessThanOrEqualToConstant:0]];
    _editButton = [[UIBarButtonItem alloc] initWithCustomView:editContainer];
    _editInnerButton.accessibilityIdentifier = kReadingListToolbarEditButtonID;

    _deleteButton = [[UIBarButtonItem alloc]
        initWithImage:DefaultSymbolWithPointSize(kDeleteActionSymbol,
                                                 kSymbolSize)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(deleteSelectedReadingListItems)];
    _deleteButton.accessibilityIdentifier = kReadingListToolbarDeleteButtonID;
    _deleteButton.tintColor = [UIColor colorNamed:kRedColor];

    _deleteAllReadInnerButton = CreateButton(
        l10n_util::GetNSString(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON));
    [_deleteAllReadInnerButton setTitleColor:[UIColor colorNamed:kRedColor]
                                    forState:UIControlStateNormal];
    UIView* deleteAllReadContainer =
        ContainerForButton(_deleteAllReadInnerButton);
    [_allButtonWidthConstraints
        addObject:[deleteAllReadContainer.widthAnchor
                      constraintLessThanOrEqualToConstant:0]];
    _deleteAllReadButton =
        [[UIBarButtonItem alloc] initWithCustomView:deleteAllReadContainer];
    _deleteAllReadInnerButton.accessibilityIdentifier =
        kReadingListToolbarDeleteAllReadButtonID;

    _cancelButton = [[UIBarButtonItem alloc]
        initWithImage:DefaultCloseButtonForToolbar()
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(exitReadingListEditMode)];
    _cancelButton.accessibilityIdentifier = kReadingListToolbarCancelButtonID;

    _markInnerButton =
        CreateButton(GetMarkButtonTitleForSelectionState(self.selectionState));
    [_markInnerButton addTarget:self
                         action:@selector(markButtonWasTapped)
               forControlEvents:UIControlEventTouchUpInside];
    UIView* markContainer = ContainerForButton(_markInnerButton);
    [_allButtonWidthConstraints
        addObject:[markContainer.widthAnchor
                      constraintLessThanOrEqualToConstant:0]];
    _markButton = [[UIBarButtonItem alloc] initWithCustomView:markContainer];
    _markButton.action = @selector(markButtonWasTapped);
    _markInnerButton.accessibilityIdentifier = kReadingListToolbarMarkButtonID;
  }
  return self;
}

#pragma mark - Accessors

- (void)setSelectionState:(ReadingListSelectionState)selectionState {
  if (_selectionState == selectionState) {
    return;
  }
  BOOL hadSelectedItems = _selectionState != ReadingListSelectionState::NONE;
  _selectionState = selectionState;
  // Check whether selection status has changed to or from NONE.
  if ((_selectionState != ReadingListSelectionState::NONE) !=
      hadSelectedItems) {
    _buttonItems = nil;
  }
}

- (void)setHasReadItems:(BOOL)hasReadItems {
  if (_hasReadItems == hasReadItems) {
    return;
  }
  BOOL didShowDeleteAllReadButton = self.shouldShowDeleteAllReadButton;
  _hasReadItems = hasReadItems;
  // Check whether the delete all read button visiblity has changed due to the
  // new value of self.hasReadItems.
  if (didShowDeleteAllReadButton != self.shouldShowDeleteAllReadButton) {
    _buttonItems = nil;
  }
}

- (void)setEditing:(BOOL)editing {
  if (_editing == editing) {
    return;
  }
  _editing = editing;
  // Entering and exiting edit mode always updates the button items.
  _buttonItems = nil;
  // Selected cells are unselected when exiting edit mode.
  if (!_editing) {
    self.selectionState = ReadingListSelectionState::NONE;
  }
}

- (void)updateMarkButtonTitle {
  if (!_editing) {
    return;
  }
  [_markInnerButton
      setTitle:GetMarkButtonTitleForSelectionState(_selectionState)
      forState:UIControlStateNormal];
}

- (BOOL)buttonItemsUpdated {
  // When the changes to the values of this class's public properties require
  // an updated buttons array, `_buttonItems` will be reset to nil.  Subsequent
  // calls to `-buttonItems` will regenerate the correct button item array.
  return !_buttonItems;
}

- (void)setCommandHandler:(id<ReadingListToolbarButtonCommands>)commandHandler {
  if (_commandHandler == commandHandler) {
    return;
  }
  _commandHandler = commandHandler;
  [_editInnerButton addTarget:_commandHandler
                       action:@selector(enterReadingListEditMode)
             forControlEvents:UIControlEventTouchUpInside];
  self.deleteButton.target = _commandHandler;
  [_deleteAllReadInnerButton addTarget:_commandHandler
                                action:@selector(deleteAllReadReadingListItems)
                      forControlEvents:UIControlEventTouchUpInside];
  self.cancelButton.target = _commandHandler;
}

- (BOOL)shouldShowEditButton {
  return !self.editing;
}

- (BOOL)shouldShowDeleteButton {
  BOOL showDeleteButton =
      self.editing && self.selectionState != ReadingListSelectionState::NONE;
  // If the delete button is shown, then the delete all button should be hidden.
  DCHECK(!showDeleteButton || !self.shouldShowDeleteAllReadButton);
  return showDeleteButton;
}

- (BOOL)shouldShowDeleteAllReadButton {
  BOOL showDeleteAllReadButton =
      self.editing && self.selectionState == ReadingListSelectionState::NONE &&
      self.hasReadItems;
  // If the delete all read button is shown, then the delete should be hidden.
  DCHECK(!showDeleteAllReadButton || !self.shouldShowDeleteButton);
  return showDeleteAllReadButton;
}

- (BOOL)shouldShowCancelButton {
  return self.editing;
}

- (BOOL)shouldShowMarkButton {
  return self.editing;
}

#pragma mark - Public

- (NSArray<UIBarButtonItem*>*)buttonItems {
  if (_buttonItems) {
    return _buttonItems;
  }
  _buttonItems = [[NSMutableArray alloc] init];
  if (self.shouldShowEditButton) {
    [_buttonItems addObject:self.editButton];
  }
  if (self.shouldShowDeleteButton) {
    [_buttonItems addObject:self.deleteButton];
  }
  if (self.shouldShowDeleteAllReadButton) {
    [_buttonItems addObject:self.deleteAllReadButton];
  }
  if (self.shouldShowMarkButton) {
    [_buttonItems addObject:self.markButton];
  }
  if (self.shouldShowCancelButton) {
    [_buttonItems addObject:self.cancelButton];
  }
  _buttonItems = [self itemsWithSpacer:_buttonItems];
  return _buttonItems;
}

- (ActionSheetCoordinator*)
    markButtonConfirmationWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser {
  return [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                           title:nil
                         message:nil
                   barButtonItem:self.markButton];
}

- (void)updateForReadingListWidth:(CGFloat)readingListWidth {
  for (NSLayoutConstraint* constraint in _allButtonWidthConstraints) {
    // Ensures that each button isn't taking more than a third of the space, as
    // there is often 3 buttons displayed.
    constraint.constant = readingListWidth / 3;
  }
}

#pragma mark - Private

// Called when the "Mark" button was tapped.
- (void)markButtonWasTapped {
  switch (self.selectionState) {
    case ReadingListSelectionState::NONE:
      [self.commandHandler markAllReadingListItemsAfterConfirmation];
      break;
    case ReadingListSelectionState::ONLY_READ_ITEMS:
      [self.commandHandler markSelectedReadingListItemsUnread];
      break;
    case ReadingListSelectionState::ONLY_UNREAD_ITEMS:
      [self.commandHandler markSelectedReadingListItemsRead];
      break;
    case ReadingListSelectionState::READ_AND_UNREAD_ITEMS:
      [self.commandHandler markSelectedReadingListItemsAfterConfirmation];
      break;
  }
}

// Returns a new array which inserts spacer button items between the items in
// `items`, right aligning the buttons if they appear alone.
- (NSMutableArray<UIBarButtonItem*>*)itemsWithSpacer:
    (NSMutableArray<UIBarButtonItem*>*)items {
  NSUInteger itemCount = items.count;
  if (itemCount == 0) {
    return items;
  }
  NSMutableArray* finalArray = [NSMutableArray array];
  // If there's a single item, add the spacer at index 0 to right-align the
  // button.  Otherwise, add the first spacer at index 1 to add space between
  // the first and second buttons.
  if (itemCount == 1) {
    [finalArray addObject:[UIBarButtonItem flexibleSpaceItem]];
    [finalArray addObject:items[0]];
    return finalArray;
  }
  for (NSUInteger i = 0; i < itemCount - 1; ++i) {
    [finalArray addObject:items[i]];
    [finalArray addObject:[UIBarButtonItem flexibleSpaceItem]];
  }
  [finalArray addObject:items[itemCount - 1]];
  return finalArray;
}

@end
