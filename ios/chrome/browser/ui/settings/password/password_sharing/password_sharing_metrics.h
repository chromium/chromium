// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_METRICS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_METRICS_H_

#import <Foundation/Foundation.h>

// Represents different user interactions with the password sharing UI.
// LINT.IfChange
enum class PasswordSharingInteraction {
  kPasswordDetailsShareButtonClicked = 0,
  kFirstRunShareClicked = 1,
  kFirstRunCancelClicked = 2,
  kFirstRunLearnMoreClicked = 3,
  kFamilyPickerShareWithOneMember = 4,
  kFamilyPickerShareWithMultipleMembers = 5,
  kFamilyPickerIneligibleRecipientLearnMoreClicked = 6,
  kFamilyPromoCreateFamilyGroupClicked = 7,
  kFamilyPromoInviteFamilyMembersClicked = 8,
  kFamilyPromoGotItClicked = 9,
  kSharingConfirmationCancelClicked = 10,
  kSharingConfirmationLearnMoreClicked = 11,
  kSharingConfirmationChangePasswordClicked = 12,
  kSharingConfirmationDoneClicked = 13,
  kFamilyPickerOpened = 14,
  kMaxValue = kFamilyPickerOpened,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml)

// Logs the user interaction with the password sharing flow.
void LogPasswordSharingInteraction(PasswordSharingInteraction action);

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_METRICS_H_
