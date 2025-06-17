// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol BackgroundCustomizationConfiguration;

// A delegate protocol for handling presentation-related actions in the
// Home Customization Background Picker action sheet.
@protocol
    HomeCustomizationBackgroundPickerActionSheetPresentationDelegate <NSObject>

// Applies the specified background configuration to the NTP based on the
// current user selection.
- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_PRESENTATION_DELEGATE_H_
