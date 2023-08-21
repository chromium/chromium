// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_DELEGATE_H_

@class FamilyPickerCoordinator;

// Delegate for FamilyPickerCoordinator.
@protocol FamilyPickerCoordinatorDelegate

// Called when the user cancels or dismisses the family recipient selection.
- (void)familyPickerCoordinatorWasDismissed:
    (FamilyPickerCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_COORDINATOR_DELEGATE_H_
