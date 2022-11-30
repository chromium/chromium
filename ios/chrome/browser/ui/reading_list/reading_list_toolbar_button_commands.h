// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands sent by reading list toolbar buttons.
@protocol ReadingListToolbarButtonCommands<NSObject>

// Called when the "Edit" button is tapped.
- (void)enterReadingListEditMode;
// Called when the "Cancel" button is tapped.
- (void)exitReadingListEditMode;
// Called when the "Delete Read" button is tapped.
- (void)deleteAllReadReadingListItems;
// Called when the "Delete" button is tapped.
- (void)deleteSelectedReadingListItems;
// Called when the "Mark Read" button is tapped.
- (void)markSelectedReadingListItemsRead;
// Called when the "Mark Unread" button is tapped.
- (void)markSelectedReadingListItemsUnread;
// Called when the "Mark..." button is tapped.  Handlers of this command should
// confirm whether the selected items should be marked as read or unread, then
// perform the disambiguated action.
- (void)markSelectedReadingListItemsAfterConfirmation;
// Called when the "Mark All..." button is tapped.  Handlers of this command
// should confirm whether the items should be marked as read or unread, then
// perform the disambiguated action.
- (void)markAllReadingListItemsAfterConfirmation;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TOOLBAR_BUTTON_COMMANDS_H_
