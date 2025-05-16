// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"

// A mutator protocol used to communicate with the
// `HomeCustomizationBackgroundPresetGalleryPickerMediator`.
@protocol HomeCustomizationBackgroundPresetGalleryPickerMutator

// Applies the given background configuration to the NTP.
// This method updates the background based on the provided configuration.
- (void)applyBackgroundForConfiguration:
    (BackgroundCustomizationConfiguration*)backgroundConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_
