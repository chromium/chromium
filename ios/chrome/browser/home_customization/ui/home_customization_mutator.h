// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
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

// Applies the given background configuration to the NTP.
// This method updates the background based on the provided configuration.
- (void)applyBackgroundForConfiguration:
    (BackgroundCustomizationConfiguration*)backgroundConfiguration;

// Downloads and returns a thumbnail image from the given GURL. The image is
// returned asynchronously through the `completion` block. The method is
// intended to be used for background customization thumbnails, such as loading
// preview images for a collection view cell when it becomes visible.
- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:
                                               (void (^)(UIImage*))completion;
@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MUTATOR_H_
