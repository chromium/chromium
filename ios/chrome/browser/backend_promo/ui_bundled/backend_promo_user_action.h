// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_USER_ACTION_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_USER_ACTION_H_

#import <Foundation/Foundation.h>

// Enum representing the user action taken on the backend promo UI.
typedef NS_ENUM(NSInteger, BackendPromoUserAction) {
  BackendPromoUserActionAccepted,
  BackendPromoUserActionDismissed,
  BackendPromoUserActionCancelled,
};

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_USER_ACTION_H_
