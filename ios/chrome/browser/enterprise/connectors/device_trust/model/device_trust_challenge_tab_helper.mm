// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"

#import "base/check_deref.h"
#import "base/notimplemented.h"
#import "ios/chrome/browser/device_trust/device_trust_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

DeviceTrustChallengeTabHelper::DeviceTrustChallengeTabHelper(
    web::WebState* web_state)
    : web_state_(CHECK_DEREF(web_state)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  web_state->AddObserver(this);

  DeviceTrustJavaScriptFeature* feature =
      DeviceTrustJavaScriptFeature::GetInstance();
  web::WebFramesManager* web_frames_manager =
      feature->GetWebFramesManager(web_state);
  CHECK(web_frames_manager);

  web_frames_manager_observation_.Observe(web_frames_manager);
  MaybeSetupDeviceTrustAPI(web_frames_manager->GetMainWebFrame());
}

void DeviceTrustChallengeTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager*,
    web::WebFrame* web_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSetupDeviceTrustAPI(web_frame);
}

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

void DeviceTrustChallengeTabHelper::MaybeSetupDeviceTrustAPI(
    web::WebFrame* web_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_frame || !web_frame->IsMainFrame()) {
    return;
  }

  // TODO(crbug.com/517112324): Check DeviceTrustService::Watches(url).
  DeviceTrustJavaScriptFeature::GetInstance()->SetupDeviceTrustAPI(web_frame);
}

void DeviceTrustChallengeTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  web_state->RemoveObserver(this);
  web_frames_manager_observation_.Reset();
}
