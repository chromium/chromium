// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol LinkToTextDelegate;
@protocol PartialTranslateDelegate;
@protocol SearchWithDelegate;

// A handler for the Browser edit menu.
// This class is in charge of customising the menu and executing the commands.
@interface BrowserEditMenuHandler : NSObject

// The delegate to handle link to text button selection.
@property(nonatomic, weak) id<LinkToTextDelegate> linkToTextDelegate;

// The delegate to handle Partial Translate button selection.
@property(nonatomic, weak) id<PartialTranslateDelegate>
    partialTranslateDelegate;

// The delegate to handle Search With button selection.
@property(nonatomic, weak) id<SearchWithDelegate> searchWithDelegate;

// Will be called by `BrowserContainerViewController buildMenuWithBuilder:`
// to customize its edit menu.
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_EDIT_MENU_HANDLER_H_
