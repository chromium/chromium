// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_

#import <string>

#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// DeviceTrustChallengeTabHelper is the orchestrator for the iOS Device Trust
// challenge-response flow. It is attached to a WebState and receives challenge
// requests forwarded from the DeviceTrustJavaScriptFeature, and routes the
// signed payload back to the originating JavaScript Promise via the JS feature
// layer.
class DeviceTrustChallengeTabHelper
    : public web::WebStateUserData<DeviceTrustChallengeTabHelper> {
 public:
  ~DeviceTrustChallengeTabHelper() override;

  DeviceTrustChallengeTabHelper(const DeviceTrustChallengeTabHelper&) = delete;
  DeviceTrustChallengeTabHelper& operator=(
      const DeviceTrustChallengeTabHelper&) = delete;

  // Initiates the asynchronous device attestation signing process.
  // `request_id` is passed along to route the payload back to the correct JS
  // Promise.
  void BuildChallengeResponse(const std::string& request_id,
                              const std::string& challenge);

 private:
  friend class web::WebStateUserData<DeviceTrustChallengeTabHelper>;
  explicit DeviceTrustChallengeTabHelper(web::WebState* web_state);

  // Callback invoked when the keychain finishes generating the signature.
  void OnChallengeResponseReady(const std::string& request_id,
                                const std::string& signed_payload);

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ref<web::WebState> web_state_;
  base::WeakPtrFactory<DeviceTrustChallengeTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_
