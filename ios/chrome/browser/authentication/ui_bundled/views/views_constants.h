// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_VIEWS_VIEWS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_VIEWS_VIEWS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the identity picker view.
extern NSString* const kIdentityButtonControlIdentifier;
// Accessibility identifier for "Add Account" button in the identity picker
// view.
extern NSString* const kIdentityPickerAddAccountIdentifier;

// Style for the identity view (modify the avatar size, font sizes and some
// margins).
typedef NS_ENUM(NSInteger, IdentityViewStyle) {
  // Default style.
  IdentityViewStyleDefault,
  // Style for the identity chooser from the signin view.
  IdentityViewStyleIdentityChooser,
  // Style for the consistency view.
  IdentityViewStyleConsistency,
  // Style for the consistency view with a small trailing margin, used when a
  // contained in a container with a status activity.
  IdentityViewStyleConsistencyContained,
  // Style for the consistency view with no trailing margin when showing the
  // default identity selected.
  IdentityViewStyleConsistencyDefaultIdentity,
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_VIEWS_VIEWS_CONSTANTS_H_
