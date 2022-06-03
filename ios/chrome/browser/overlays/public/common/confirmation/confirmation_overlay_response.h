// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_H_

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

// A generic response type for use by overlays that are asking for confirmation
// for a decision.
class ConfirmationOverlayResponse
    : public OverlayResponseInfo<ConfirmationOverlayResponse> {
 public:
  ~ConfirmationOverlayResponse() override;

  // Whether the user has taken the confirmation action on the overlay UI.
  bool confirmed() const { return confirmed_; }

 private:
  OVERLAY_USER_DATA_SETUP(ConfirmationOverlayResponse);
  explicit ConfirmationOverlayResponse(bool confirmed);

  bool confirmed_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_COMMON_CONFIRMATION_CONFIRMATION_OVERLAY_RESPONSE_H_
