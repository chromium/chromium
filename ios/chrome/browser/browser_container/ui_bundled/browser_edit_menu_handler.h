// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_EDIT_MENU_HANDLER_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_EDIT_MENU_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"

// TODO(crbug.com/408229821): Reverse dependencies.
@protocol ExplainWithGeminiDelegate;
@protocol LinkToTextDelegate;
@protocol PartialTranslateDelegate;
@protocol SearchWithDelegate;

// A handler for the Browser edit menu.
// This class is in charge of customising the menu and executing the commands.
@interface BrowserEditMenuHandler : NSObject <EditMenuBuilder>

// The delegate to handle Explain With Gemini button selection.
@property(nonatomic, weak) id<ExplainWithGeminiDelegate>
    explainWithGeminiDelegate;

// The delegate to handle link to text button selection.
@property(nonatomic, weak) id<LinkToTextDelegate> linkToTextDelegate;

// The delegate to handle Partial Translate button selection.
@property(nonatomic, weak) id<PartialTranslateDelegate>
    partialTranslateDelegate;

// The delegate to handle Search With button selection.
@property(nonatomic, weak) id<SearchWithDelegate> searchWithDelegate;

// Will be called to customize edit menus.
- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_BROWSER_EDIT_MENU_HANDLER_H_
