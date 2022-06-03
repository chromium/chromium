// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol FindInPageCommands <NSObject>

// Starts the finding process and shows the Find In Page bar.
- (void)openFindInPage;

// Closes and disables the Find In Page bar.
- (void)closeFindInPage;

// Shows the Find In Page bar if finding is ongoing.
- (void)showFindUIIfActive;

// Closes the Find In Page bar, but does not stop finding. The find bar will
// reappear.
- (void)hideFindUI;

// Defocuses the find in page text field.
- (void)defocusFindInPage;

// Search the current tab for the query string in the Find In Page bar.
- (void)searchFindInPage;

// Go to the next location of the Find In Page query string in the current tab.
- (void)findNextStringInPage;

// Go to the previous location of the Find In Page query string in the current
// tab.
- (void)findPreviousStringInPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_COMMANDS_H_
