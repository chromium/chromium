// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_mediator.h"

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"

@implementation HomeCustomizationBackgroundPickerActionSheetMediator

- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  // TODO(crbug.com/408243803): Apply NTP background configuration to NTP.
}

- (void)addBackgroundToRecentlyUsed:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  // TODO(crbug.com/408243803): Add the selected background configuration to the
  // recently used list.
}

@end
