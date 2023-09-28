// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The accessibility identifiers of the family picker view.
extern NSString* const kFamilyPickerTableViewId;
extern NSString* const kFamilyPickerBackButtonId;
extern NSString* const kFamilyPickerCancelButtonId;
extern NSString* const kFamilyPickerShareButtonId;
extern NSString* const kFamilyPickerInfoButtonId;

// The accessibility identifiers of the password picker view.
extern NSString* const kPasswordPickerCancelButtonId;
extern NSString* const kPasswordPickerNextButtonId;

// Link for creating family group with Google Families.
extern const char kFamilyGroupSiteURL[];

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
