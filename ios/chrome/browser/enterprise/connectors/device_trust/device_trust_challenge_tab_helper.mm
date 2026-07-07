// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/device_trust_challenge_tab_helper.h"

#import "base/check_deref.h"
#import "base/notimplemented.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

DeviceTrustChallengeTabHelper::DeviceTrustChallengeTabHelper(
    web::WebState* web_state)
    : web_state_(CHECK_DEREF(web_state)) {}

DeviceTrustChallengeTabHelper::~DeviceTrustChallengeTabHelper() = default;

void DeviceTrustChallengeTabHelper::BuildChallengeResponse(
    const std::string& request_id,
    const std::string& challenge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::BrowserState* browser_state = web_state_->GetBrowserState();
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  CHECK(profile);

  // TODO(crbug.com/498472825): Implement the logic to build the device trust
  // challenge response.
  NOTIMPLEMENTED();
}

void DeviceTrustChallengeTabHelper::OnChallengeResponseReady(
    const std::string& request_id,
    const std::string& signed_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/517125162): Implement the logic to handle the received
  // challenge response.
  NOTIMPLEMENTED();
}
