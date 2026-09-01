// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_trust/device_trust_java_script_feature.h"

#import <utility>

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"

namespace {
constexpr char kScriptHandlerName[] = "DeviceTrustMessageHandler";
constexpr char kDeviceTrustAPIName[] = "device_trust";
// LINT.IfChange(MaxChallengeRequestLength)
constexpr size_t kMaxChallengeRequestLength = 1024;
// LINT.ThenChange(//ios/chrome/browser/device_trust/device_trust.ts:MaxChallengeRequestLength)
}  // namespace

DeviceTrustJavaScriptFeature* DeviceTrustJavaScriptFeature::GetInstance() {
  static base::NoDestructor<DeviceTrustJavaScriptFeature> instance;
  return instance.get();
}

// Injected into `kPageContentWorld` so that the
// `window.chrome.enterprise.deviceTrust` API is exposed to the webpage's
// JavaScript context (e.g. Identity Providers executing Device Trust
// attestation during authentication).
DeviceTrustJavaScriptFeature::DeviceTrustJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {web::JavaScriptFeature::FeatureScript::CreateWithFilename(
              kDeviceTrustAPIName,
              web::JavaScriptFeature::FeatureScript::InjectionTime::
                  kDocumentStart,
              web::JavaScriptFeature::FeatureScript::TargetFrames::
                  kMainFrame)}) {}

DeviceTrustJavaScriptFeature::~DeviceTrustJavaScriptFeature() = default;

void DeviceTrustJavaScriptFeature::SetupDeviceTrustAPI(
    web::WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, "deviceTrust.setupDeviceTrustAPI",
                         /*parameters=*/{});
}

std::optional<std::string>
DeviceTrustJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

bool DeviceTrustJavaScriptFeature::GetFeatureRepliesToMessages() const {
  return true;
}

void DeviceTrustJavaScriptFeature::ScriptMessageReceivedWithReply(
    web::WebState* web_state,
    const web::ScriptMessage& message,
    ScriptMessageReplyCallback callback) {
  if (!message.is_main_frame() || !message.legacy_body() ||
      !message.legacy_body()->is_dict()) {
    RejectAttestationRequest(std::move(callback), "Invalid challenge request.",
                             "INVALID_CHALLENGE_REQUEST");
    return;
  }

  const std::string* challenge =
      message.legacy_body()->GetDict().FindString("challengeRequest");
  if (!challenge || challenge->empty() ||
      challenge->size() > kMaxChallengeRequestLength) {
    RejectAttestationRequest(std::move(callback), "Invalid challenge request.",
                             "INVALID_CHALLENGE_REQUEST");
    return;
  }

  HandleAttestationRequest(web_state, *challenge, std::move(callback));
}

void DeviceTrustJavaScriptFeature::HandleAttestationRequest(
    web::WebState* web_state,
    const std::string& challenge_request,
    ScriptMessageReplyCallback callback) {
  // TODO(crbug.com/517112324): Route to the Device Trust attestation flow
  // (TabHelper) in a follow up CL. Until then, reject so that callers fail
  // fast.
  RejectAttestationRequest(std::move(callback),
                           "Device attestation is not available.",
                           "INTERNAL_ERROR");
}

void DeviceTrustJavaScriptFeature::ResolveAttestationRequest(
    ScriptMessageReplyCallback callback,
    const std::string& signed_payload) {
  base::DictValue reply;
  reply.Set("signedPayload", signed_payload);
  base::Value reply_value(std::move(reply));
  std::move(callback).Run(&reply_value, nil);
}

void DeviceTrustJavaScriptFeature::RejectAttestationRequest(
    ScriptMessageReplyCallback callback,
    const std::string& error_message,
    const std::string& error_code) {
  base::DictValue reply;
  reply.Set("errorCode", error_code);
  reply.Set("errorMessage", error_message);
  base::Value reply_value(std::move(reply));
  std::move(callback).Run(&reply_value, nil);
}
