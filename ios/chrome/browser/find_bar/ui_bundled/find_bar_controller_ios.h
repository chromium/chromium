// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_CONTROLLER_IOS_H_
#define IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_CONTROLLER_IOS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_consumer.h"

@protocol BrowserCommands;
@class FindBarViewController;
@protocol FindInPageCommands;
@class FindInPageModel;

@interface FindBarControllerIOS : NSObject <FindBarConsumer>

// The command handler for all necessary commands
@property(nonatomic, weak) id<FindInPageCommands> commandHandler;
// The view controller containing all the buttons and textfields that is common
// between iPhone and iPad.
@property(nonatomic, strong, readonly)
    FindBarViewController* findBarViewController;

// Init with incognito style.
- (instancetype)initWithIncognito:(BOOL)isIncognito;
// Current input search term.
- (NSString*)searchTerm;
// Update view based on model. If `focusTextfield` is YES, focus the
// textfield. Updates the results count and, if `initialUpdate` is true, fills
// the text field with search term from the model.
- (void)updateView:(FindInPageModel*)model
     initialUpdate:(BOOL)initialUpdate
    focusTextfield:(BOOL)focusTextfield;

// Updates the results count in Find Bar.
- (void)updateResultsCount:(FindInPageModel*)model;

// Alerts the controller that its find bar will hide.
- (void)findBarViewWillHide;
// Alerts the controller that its find bar did hide.
- (void)findBarViewDidHide;

// Hide the keyboard when the find next/previous buttons are pressed.
- (IBAction)hideKeyboard:(id)sender;
// Indicates that Find in Page is shown. When true, `view` is guaranteed not to
// be nil.
- (BOOL)isFindInPageShown;
// Indicates that the Find in Page text field is first responder.
- (BOOL)isFocused;

// Selects all the text in the Find in Page text field.
- (void)selectAllText;

@end

#endif  // IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_CONTROLLER_IOS_H_
