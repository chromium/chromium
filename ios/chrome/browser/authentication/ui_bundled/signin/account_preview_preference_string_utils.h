// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_PREVIEW_PREFERENCE_STRING_UTILS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_PREVIEW_PREFERENCE_STRING_UTILS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/signin/core/browser/account_preview_data_service.h"

namespace signin {

// Returns the promo label string for web sign-in based on `preference`, or nil
// if no customized subtitle applies.
NSString* GetPromoLabelStringForWebSignin(
    const std::optional<AccountPreviewDataService::AccountPreviewPreference>&
        preference);

// Returns the promo label string for NTP sign-in based on `preference`, or nil
// if no customized subtitle applies.
NSString* GetPromoLabelStringForNtpSignedOutIcon(
    const std::optional<AccountPreviewDataService::AccountPreviewPreference>&
        preference);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_PREVIEW_PREFERENCE_STRING_UTILS_H_
