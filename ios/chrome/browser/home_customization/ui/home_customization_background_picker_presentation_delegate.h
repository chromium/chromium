// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_

// Protocol for all presentation events for background picker views.
@protocol HomeCustomizationBackgroundPickerPresentationDelegate <NSObject>

// Presents a background picker alert with options such as gallery, camera roll,
// or color selection.
- (void)showBackgroundPickerOptionsFromSourceView:(UIView*)sourceView;

// Dismiss the background picker after a successful change, dismissing the
// entire customization menu.
- (void)dismissBackgroundPicker;

// Cancel background picker presentation, returning to the main customiztion
// menu.
- (void)cancelBackgroundPicker;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_
