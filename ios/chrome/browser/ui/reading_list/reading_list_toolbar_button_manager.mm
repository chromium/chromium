// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_manager.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_commands.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the title to use for the "Mark" button for |state|.
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
}  // namespace

@interface ReadingListToolbarButtonManager () {
  // The button items corresponding to the current state.
  NSMutableArray<UIBarButtonItem*>* _buttonItems;
}

// The possible button items that may be returned by the |-buttonItems|.
@property(nonatomic, strong, readonly) UIBarButtonItem* editButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* deleteButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* deleteAllReadButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* cancelButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* markButton;

// Whether the corresponding button items should be returned in |-buttonItems|.
@property(nonatomic, readonly) BOOL shouldShowEditButton;
@property(nonatomic, readonly) BOOL shouldShowDeleteButton;
@property(nonatomic, readonly) BOOL shouldShowDeleteAllReadButton;
@property(nonatomic, readonly) BOOL shouldShowCancelButton;
@property(nonatomic, readonly) BOOL shouldShowMarkButton;

@end

@implementation ReadingListToolbarButtonManager
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
  if (self = [super init]) {
    _selectionState = ReadingListSelectionState::NONE;

    _editButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_READING_LIST_EDIT_BUTTON)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(enterReadingListEditMode)];
    _editButton.accessibilityIdentifier = kReadingListToolbarEditButtonID;

    _deleteButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_READING_LIST_DELETE_BUTTON)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(deleteSelectedReadingListItems)];
    _deleteButton.accessibilityIdentifier = kReadingListToolbarDeleteButtonID;
    _deleteButton.tintColor = [UIColor colorNamed:kRedColor];

    _deleteAllReadButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(deleteAllReadReadingListItems)];
    _deleteAllReadButton.accessibilityIdentifier =
        kReadingListToolbarDeleteAllReadButtonID;
    _deleteAllReadButton.tintColor = [UIColor colorNamed:kRedColor];

    _cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_READING_LIST_CANCEL_BUTTON)
                style:UIBarButtonItemStyleDone
               target:nil
               action:@selector(exitReadingListEditMode)];
    _cancelButton.accessibilityIdentifier = kReadingListToolbarCancelButtonID;

    _markButton = [[UIBarButtonItem alloc]
        initWithTitle:GetMarkButtonTitleForSelectionState(self.selectionState)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(markButtonWasTapped)];
    _markButton.accessibilityIdentifier = kReadingListToolbarMarkButtonID;
  }
  return self;
}

#pragma mark - Accessors

- (void)setSelectionState:(ReadingListSelectionState)selectionState {
  if (_selectionState == selectionState)
    return;
  BOOL hadSelectedItems = _selectionState != ReadingListSelectionState::NONE;
  _selectionState = selectionState;
  // Check whether selection status has changed to or from NONE.
  if ((_selectionState != ReadingListSelectionState::NONE) != hadSelectedItems)
    _buttonItems = nil;
}

- (void)setHasReadItems:(BOOL)hasReadItems {
  if (_hasReadItems == hasReadItems)
    return;
  BOOL didShowDeleteAllReadButton = self.shouldShowDeleteAllReadButton;
  _hasReadItems = hasReadItems;
  // Check whether the delete all read button visiblity has changed due to the
  // new value of self.hasReadItems.
  if (didShowDeleteAllReadButton != self.shouldShowDeleteAllReadButton)
    _buttonItems = nil;
}

- (void)setEditing:(BOOL)editing {
  if (_editing == editing)
    return;
  _editing = editing;
  // Entering and exiting edit mode always updates the button items.
  _buttonItems = nil;
  // Selected cells are unselected when exiting edit mode.
  if (!_editing)
    self.selectionState = ReadingListSelectionState::NONE;
}

- (void)updateMarkButtonTitle {
  if (!_editing)
    return;
  _markButton.title = GetMarkButtonTitleForSelectionState(_selectionState);
}

- (BOOL)buttonItemsUpdated {
  // When the changes to the values of this class's public properties require
  // an updated buttons array, |_buttonItems| will be reset to nil.  Subsequent
  // calls to |-buttonItems| will regenerate the correct button item array.
  return !_buttonItems;
}

- (void)setCommandHandler:(id<ReadingListToolbarButtonCommands>)commandHandler {
  if (_commandHandler == commandHandler)
    return;
  _commandHandler = commandHandler;
  self.editButton.target = _commandHandler;
  self.deleteButton.target = _commandHandler;
  self.deleteAllReadButton.target = _commandHandler;
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
  if (_buttonItems)
    return _buttonItems;
  _buttonItems = [[NSMutableArray alloc] init];
  if (self.shouldShowEditButton)
    [_buttonItems addObject:self.editButton];
  if (self.shouldShowDeleteButton)
    [_buttonItems addObject:self.deleteButton];
  if (self.shouldShowDeleteAllReadButton)
    [_buttonItems addObject:self.deleteAllReadButton];
  if (self.shouldShowMarkButton)
    [_buttonItems addObject:self.markButton];
  if (self.shouldShowCancelButton)
    [_buttonItems addObject:self.cancelButton];
  [self addSpacersToItems:_buttonItems];
  return _buttonItems;
}

- (ActionSheetCoordinator*)markButtonConfirmationWithBaseViewController:
    (UIViewController*)viewController {
  return [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                           title:nil
                         message:nil
                   barButtonItem:self.markButton];
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

// Inserts spacer button items between the items in |items|, right aligning the
// buttons if they appear alone.
- (void)addSpacersToItems:(NSMutableArray<UIBarButtonItem*>*)items {
  NSMutableArray<UIBarButtonItem*>* spacers = [NSMutableArray array];
  NSMutableIndexSet* indexes = [NSMutableIndexSet indexSet];
  NSUInteger itemCount = items.count;
  // If there's a single item, add the spacer at index 0 to right-align the
  // button.  Otherwise, add the first spacer at index 1 to add space between
  // the first and second buttons.
  NSUInteger firstIndex = itemCount == 1 ? 0 : 1;
  for (NSUInteger i = 0; i < itemCount - firstIndex; ++i) {
    [spacers addObject:[[UIBarButtonItem alloc]
                           initWithBarButtonSystemItem:
                               UIBarButtonSystemItemFlexibleSpace
                                                target:nil
                                                action:nil]];
    [indexes addIndex:firstIndex + 2 * i];
  }
  [items insertObjects:spacers atIndexes:indexes];
}

@end
