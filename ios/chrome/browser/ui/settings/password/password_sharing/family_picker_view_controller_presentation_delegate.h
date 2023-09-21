// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class FamilyPickerViewController;
@class RecipientInfoForIOSDisplay;

// Delegate for FamilyPickerViewController.
@protocol FamilyPickerViewControllerPresentationDelegate <NSObject>

// Called when the user clicks cancel button or dismisses the view by swiping.
- (void)familyPickerWasDismissed:(FamilyPickerViewController*)controller;

// Called when the user clicks share button with selected recipients;
- (void)familyPickerClosed:(FamilyPickerViewController*)controller
    withSelectedRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients;

// Called when the user clicks back button to navigate to the password picker
// view.
- (void)familyPickerNavigatedBack:(FamilyPickerViewController*)controller;

// Called when the user clicks "Learn more" in the info button popover of the
// recipient that is not eligible to receive passwords.
- (void)learnMoreLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
