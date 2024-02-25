// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The accessibility identifiers of the family picker view.
extern NSString* const kFamilyPickerTableViewID;
extern NSString* const kFamilyPickerBackButtonID;
extern NSString* const kFamilyPickerCancelButtonID;
extern NSString* const kFamilyPickerShareButtonID;
extern NSString* const kFamilyPickerInfoButtonID;

// The accessibility identifier of the family promo view.
extern NSString* const kFamilyPromoViewID;

// The accessibility identifiers of the password picker view.
extern NSString* const kPasswordPickerViewID;
extern NSString* const kPasswordPickerCancelButtonID;
extern NSString* const kPasswordPickerNextButtonID;

// The accessibility identifiers of the sharing status view.
extern NSString* const kSharingStatusViewID;
extern NSString* const kSharingStatusDoneButtonID;

// Links for managing Google Family groups.
extern const char kCreateFamilyGroupURL[];
extern const char kManageFamilyGroupURL[];

// Link for the password sharing HC article.
extern const char kPasswordSharingLearnMoreURL[];

// Represents possible variants of the family promo view.
enum class FamilyPromoType {
  // Promo to create a family group.
  kUserNotInFamilyGroup,
  // Promo to invite members to existing family group.
  kUserWithNoOtherFamilyMembers,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_CONSTANTS_H_
