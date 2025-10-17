// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ACCESSIBILITY_IDENTIFIERS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ACCESSIBILITY_IDENTIFIERS_H_

#import <UIKit/UIKit.h>

// A11y identifier for the menu cell that opens the background picker alert
// view.
extern NSString* const kBackgroundPickerCellAccessibilityIdentifier;

// A11y identifier for the gallery picker view.
extern NSString* const
    kHomeCustomizationGalleryPickerViewAccessibilityIdentifier;

// A11y identifier for the main customization menu view.
extern NSString* const kHomeCustomizationMainViewAccessibilityIdentifier;

// A11y identifier for the main photo framing view.
extern NSString* const kPhotoFramingMainViewAccessibilityIdentifier;

// A11y identifier for the save button in the photo framing view.
extern NSString* const kPhotoFramingViewSaveButtonAccessibilityIdentifier;

// A11y identifier for the cancel button in the background picker views (color
// or gallery).
extern NSString* const kPickerViewCancelButtonAccessibilityIdentifier;

// A11y identifier for the done button in the background picker views (color or
// gallery).
extern NSString* const kPickerViewDoneButtonAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_ACCESSIBILITY_IDENTIFIERS_H_
