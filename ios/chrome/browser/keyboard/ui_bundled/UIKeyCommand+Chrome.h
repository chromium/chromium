// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_UIKEYCOMMAND_CHROME_H_
#define IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_UIKEYCOMMAND_CHROME_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// Note: this is implemented as a category on UIKeyCommand because UIKeyCommand
// can't be subclassed as of iOS 9 beta 4. http://crbug.com/510970
@interface UIKeyCommand (Chrome)

// These commands come pre-configured with localized titles (for those that
// appear in the HUD or menu), inputs, and modifier flags. Their action is
// matching their name, where the UIKeyCommand cr_xxx triggers the action method
// keyCommand_xxx.
// Variants are provided if necessary. Variants are named cr_xxx_2, cr_xxx_3,
// etc. They don't have a title and don't appear in the HUD or menu, but trigger
// the same action method keyCommand_xxx.
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewRegularTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewIncognitoTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewWindow;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewIncognitoWindow;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reopenLastClosedTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_find;
@property(class, nonatomic, readonly) UIKeyCommand* cr_findNext;
@property(class, nonatomic, readonly) UIKeyCommand* cr_findPrevious;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openLocation;
@property(class, nonatomic, readonly) UIKeyCommand* cr_closeTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab_3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab_3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showBookmarks;
@property(class, nonatomic, readonly) UIKeyCommand* cr_addToBookmarks;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reload;
@property(class, nonatomic, readonly) UIKeyCommand* cr_back;
@property(class, nonatomic, readonly) UIKeyCommand* cr_forward;
@property(class, nonatomic, readonly) UIKeyCommand* cr_back_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_forward_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showHistory;
@property(class, nonatomic, readonly) UIKeyCommand* cr_voiceSearch;
@property(class, nonatomic, readonly) UIKeyCommand* cr_close;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showSettings;
@property(class, nonatomic, readonly) UIKeyCommand* cr_stop;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showHelp;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showDownloads;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showDownloads_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select1;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select4;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select5;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select6;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select7;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select8;
@property(class, nonatomic, readonly) UIKeyCommand* cr_select9;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reportAnIssue;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reportAnIssue_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_addToReadingList;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showReadingList;
@property(class, nonatomic, readonly) UIKeyCommand* cr_goToTabGrid;
@property(class, nonatomic, readonly) UIKeyCommand* cr_clearBrowsingData;
@property(class, nonatomic, readonly) UIKeyCommand* cr_closeAll;
@property(class, nonatomic, readonly) UIKeyCommand* cr_undo;

// Returns a symbolic description of the key command. For example: ⇧⌘T.
@property(nonatomic, readonly) NSString* cr_symbolicDescription;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_UIKEYCOMMAND_CHROME_H_
