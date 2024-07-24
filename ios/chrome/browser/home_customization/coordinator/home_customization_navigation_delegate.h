// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_

// Delegate protocol for navigating within the customization menu.
@protocol HomeCustomizationNavigationDelegate

// Navigates to a given page within the customization menu.
- (void)navigateToPage:(CustomizationMenuPage)page;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_NAVIGATION_DELEGATE_H_
