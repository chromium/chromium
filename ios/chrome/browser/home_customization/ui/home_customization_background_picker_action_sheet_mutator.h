// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MUTATOR_H_

@protocol BackgroundCustomizationConfiguration;

// A mutator protocol used to communicate with the the background customization
// action sheet.
@protocol HomeCustomizationBackgroundPickerActionSheetMutator

// Applies the given background configuration to the NTP.
// This method updates the background based on the provided configuration.
- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration;

// Adds the given background configuration to the list of recently used
// backgrounds.
- (void)addBackgroundToRecentlyUsed:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MUTATOR_H_
