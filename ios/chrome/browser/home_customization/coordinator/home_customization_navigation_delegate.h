// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_

enum class CustomizationMenuPage : NSInteger;
class GURL;

// Delegate protocol for navigating within the customization menu.
@protocol HomeCustomizationNavigationDelegate

// Opens a menu page as a new sheet. If a menu page is already being presented,
// this opens a new sheet overlaid on top of it.
- (void)presentCustomizationMenuPage:(CustomizationMenuPage)page;

// Dismisses the top page of the menu stack. If this is the only remaining page,
// the menu is closed entirely.
- (void)dismissMenuPage;

// Navigates to a given `URL` and closes the customization menu.
- (void)navigateToURL:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_
