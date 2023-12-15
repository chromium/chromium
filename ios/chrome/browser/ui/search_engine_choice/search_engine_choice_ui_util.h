// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_

#import <UIKit/UIKit.h>

// UI Util containing helper methods for the choice screen UI.

// Gets the correct font for the title.
UIFont* GetTitleFontWithTraitCollection(UITraitCollection* trait_collection);

// Creates a grey and disabled "Set as Default" primary button.
UIButton* CreateDisabledPrimaryButton();

// Creates a "More" button with an arrow that allows scrolling down.
UIButton* CreateMorePrimaryButton();

// Update the primary action button based on whether it should be the "More"
// button or the confirmation button and whether it should be enabled.
void UpdatePrimaryButton(UIButton* button,
                         BOOL isConfirmButton,
                         BOOL isEnabled);

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
