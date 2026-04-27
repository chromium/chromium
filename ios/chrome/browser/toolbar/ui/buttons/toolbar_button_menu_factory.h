// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_H_

#import <UIKit/UIKit.h>

#import <vector>

@class BrowserActionFactory;
namespace web {
class NavigationItem;
}  // namespace web
@class TabGridState;
class TemplateURLService;
@protocol ToolbarButtonMenuFactoryDelegate;
class WebStateList;

// Factory that provides context menus for toolbar buttons.
@interface ToolbarButtonMenuFactory : NSObject

// Delegate for this factory.
@property(nonatomic, weak) id<ToolbarButtonMenuFactoryDelegate> delegate;

// Initializer for this factory when used by the Toolbar.
- (instancetype)initForToolbarWithIncognito:(BOOL)incognito
                               webStateList:(WebStateList*)webStateList
                              actionFactory:
                                  (BrowserActionFactory*)actionFactory;

// Initializer for this factory when used by the App Bar.
- (instancetype)initForAppBarWithIncognito:(BOOL)incognito
                              webStateList:(WebStateList*)webStateList
                             actionFactory:(BrowserActionFactory*)actionFactory
                        templateURLService:
                            (TemplateURLService*)templateURLService
                              tabGridState:(TabGridState*)tabGridState;

- (instancetype)init NS_UNAVAILABLE;

// Returns the context menu for a forward/back navigation button.
- (UIMenu*)menuForNavigationButton:
    (const std::vector<web::NavigationItem*>)navigationItems;

// Returns the context menu for the Assistant button.
/// TODO(crbug.com/484000556) Implement this menu.
- (UIMenu*)menuForAssistantButton;

// Returns the context menu for the New Tab button.
- (UIMenu*)menuForNewTabButton;

// Returns the context menu for the Tab Grid button.
- (UIMenu*)menuForTabGridButton;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_MENU_FACTORY_H_
