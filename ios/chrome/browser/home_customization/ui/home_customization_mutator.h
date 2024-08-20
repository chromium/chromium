// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Mutator protocol for the UI layer to communicate to the
// HomeCustomizationMediator.
@protocol HomeCustomizationMutator

// Handles the visibility of a Home module being toggled.
- (void)toggleModuleVisibilityForType:(CustomizationToggleType)type
                              enabled:(BOOL)enabled;

// Navigates to the customization submenu for a given `type`.
- (void)navigateToSubmenuForType:(CustomizationToggleType)type;

// Navigates to an external URL for a given `type`.
- (void)navigateToLinkForType:(CustomizationLinkType)type;

// Dismisses the top page of the menu stack.
- (void)dismissMenuPage;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_
