// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_UTIL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_UTIL_H_

#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"

// Utility function for creating a ResponseConverter that returns a
// ConfirmationOverlayResponse.  |confirm_button_index| is the button index of
// an AlertRequest's button configs that corresponds with a confirm action.
alert_overlays::ResponseConverter GetConfirmationResponseConverter(
    size_t confirm_button_index);

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_UTIL_H_
