// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

namespace password_infobar_modal_responses {

// Response info used to create dispatched OverlayResponses that update the
// credentials using the provided username and password.
class UpdateCredentialsInfo
    : public OverlayResponseInfo<UpdateCredentialsInfo> {
 public:
  ~UpdateCredentialsInfo() override;

  // The username for the credentials being updated.
  NSString* username() const { return username_; }
  // The password for the credentials being updated.
  NSString* password() const { return password_; }

 private:
  OVERLAY_USER_DATA_SETUP(UpdateCredentialsInfo);
  UpdateCredentialsInfo(NSString* username, NSString* password);

  NSString* username_ = nil;
  NSString* password_ = nil;
};

// Response info used to create dispatched OverlayResponses that notify the
// password infobar to never save credentials.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(NeverSaveCredentials);

// Response info used to create dispatched OverlayResponses that notify the
// password infobar to present the password settings.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(PresentPasswordSettings);

}  // password_infobar_modal_responses

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
