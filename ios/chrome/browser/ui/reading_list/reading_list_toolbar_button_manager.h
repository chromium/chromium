// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_MANAGER_H_

#import <UIKit/UIKit.h>

@class ActionSheetCoordinator;
@protocol ReadingListToolbarButtonCommands;
class Browser;

// Enum type describing the items that are currently selected.
enum class ReadingListSelectionState {
  NONE,
  ONLY_READ_ITEMS,
  ONLY_UNREAD_ITEMS,
  READ_AND_UNREAD_ITEMS
};

// State object that manages the buttons that should be displayed in the toolbar
// at the bottom of the reading list.
@interface ReadingListToolbarButtonManager : NSObject

// The selection state of the reading list view.
@property(nonatomic, assign) ReadingListSelectionState selectionState;
// Whether the reading list has any read items.
@property(nonatomic, assign) BOOL hasReadItems;
// Whether the reading list is being edited.
@property(nonatomic, assign, getter=isEditing) BOOL editing;

// Whether the toolbar buttons have changed since the last `-buttonItems` call.
@property(nonatomic, readonly) BOOL buttonItemsUpdated;

// The handler for toolbar button actions.
@property(nonatomic, weak) id<ReadingListToolbarButtonCommands> commandHandler;

// Returns an array of button items that should be displayed for the current
// state.  The buttons are be set up to send ReadingListToolbarButtonCommands
// to `self.commandHandler`.
- (NSArray<UIBarButtonItem*>*)buttonItems;

// Updates the title of the mark button based on the selection state. This
// method isn't part of the update of the selection state to avoid updating it
// too soon and messing with VoiceOver. See https://crbug.com/985744 .
- (void)updateMarkButtonTitle;

// Returns an empty ActionSheetCoordinator anchored to the mark button with no
// message and no title.
- (ActionSheetCoordinator*)
    markButtonConfirmationWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_MANAGER_H_
