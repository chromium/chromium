// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_PROMO_TYPES_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_PROMO_TYPES_H_

#import <Foundation/Foundation.h>

// Enum representing the different contexts in which the sign-in promo can
// appear.
enum class SignInPromoType {
  // Sign-in promo triggered from bookmarks.
  kBookmark,
  // Sign-in promo triggered from password manager.
  kPassword
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_SIGNIN_PROMO_TYPES_H_
