// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_java_script_feature.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {
constexpr char kScriptHandlerName[] = "DeviceTrustMessageHandler";
constexpr char kDeviceTrustAPIName[] = "device_trust";
// LINT.IfChange(MaxChallengeRequestLength)
constexpr size_t kMaxChallengeRequestLength = 1024;
// LINT.ThenChange(//ios/chrome/browser/enterprise/connectors/device_trust/model/resources/device_trust.ts:MaxChallengeRequestLength)

using ReplyCallback =
    base::OnceCallback<void(const base::Value* reply, NSString* error)>;

// Resolves the originating JS promise with the signed payload.
void ResolveAttestationRequest(ReplyCallback callback,
                               const std::string& signed_payload) {
  base::DictValue reply;
  reply.Set("signedPayload", signed_payload);
  base::Value reply_value(std::move(reply));
  std::move(callback).Run(&reply_value, nil);
}

// Returns the JavaScript error code for a DeviceTrustError.
const char* DeviceTrustErrorToJsErrorCode(
    enterprise_connectors::DeviceTrustError error) {
  switch (error) {
    case enterprise_connectors::DeviceTrustError::kFailedToParseChallenge:
      return "INVALID_CHALLENGE_REQUEST";
    case enterprise_connectors::DeviceTrustError::kTooManyRequests:
      return "TOO_MANY_REQUESTS";
    case enterprise_connectors::DeviceTrustError::kTimeout:
      return "ATTESTATION_TIMEOUT";
    case enterprise_connectors::DeviceTrustError::kUrlNotAllowed:
      return "URL_NOT_ALLOWED";
    case enterprise_connectors::DeviceTrustError::kServiceUnavailable:
      return "SERVICE_UNAVAILABLE";
    case enterprise_connectors::DeviceTrustError::kInvalidOrigin:
      return "INVALID_ORIGIN";
    case enterprise_connectors::DeviceTrustError::kUnknown:
    case enterprise_connectors::DeviceTrustError::kFailedToCreateResponse:
      return "INTERNAL_ERROR";
  }
  NOTREACHED();
}

// Returns the default JavaScript error message for a DeviceTrustError.
const char* DeviceTrustErrorToJsErrorMessage(
    enterprise_connectors::DeviceTrustError error) {
  switch (error) {
    case enterprise_connectors::DeviceTrustError::kFailedToParseChallenge:
      return "Failed to parse challenge.";
    case enterprise_connectors::DeviceTrustError::kTooManyRequests:
      return "Too many pending device attestation requests.";
    case enterprise_connectors::DeviceTrustError::kTimeout:
      return "Timed out waiting for attestation response.";
    case enterprise_connectors::DeviceTrustError::kUrlNotAllowed:
      return "The requesting URL is not allowed for device attestation.";
    case enterprise_connectors::DeviceTrustError::kInvalidOrigin:
      return "The requesting origin is invalid.";
    case enterprise_connectors::DeviceTrustError::kServiceUnavailable:
    case enterprise_connectors::DeviceTrustError::kUnknown:
    case enterprise_connectors::DeviceTrustError::kFailedToCreateResponse:
      return "Device attestation is not available.";
  }
  NOTREACHED();
}

// Errors specific to the iOS script-message bridge, not the shared device
// attestation flow.
enum class ScriptMessageError {
  kUnsupportedFrame,
};

// Returns the JavaScript error code for a ScriptMessageError.
const char* ScriptMessageErrorToErrorCode(ScriptMessageError error) {
  switch (error) {
    case ScriptMessageError::kUnsupportedFrame:
      return "UNSUPPORTED_FRAME";
  }
  NOTREACHED();
}

// Returns the default JavaScript error message for a ScriptMessageError.
const char* ScriptMessageErrorToErrorMessage(ScriptMessageError error) {
  switch (error) {
    case ScriptMessageError::kUnsupportedFrame:
      return "Device attestation is only supported in the main frame.";
  }
  NOTREACHED();
}

// Rejects the originating JS promise with an error code and message for a
// DeviceTrustError.
void RejectAttestationRequest(ReplyCallback callback,
                              enterprise_connectors::DeviceTrustError error) {
  base::DictValue reply;
  reply.Set("errorCode", DeviceTrustErrorToJsErrorCode(error));
  reply.Set("errorMessage", DeviceTrustErrorToJsErrorMessage(error));
  base::Value reply_value(std::move(reply));
  std::move(callback).Run(&reply_value, nil);
}

// Rejects the originating JS promise with an error code and message for a
// ScriptMessageError.
void RejectAttestationRequest(ReplyCallback callback,
                              ScriptMessageError error) {
  base::DictValue reply;
  reply.Set("errorCode", ScriptMessageErrorToErrorCode(error));
  reply.Set("errorMessage", ScriptMessageErrorToErrorMessage(error));
  base::Value reply_value(std::move(reply));
  std::move(callback).Run(&reply_value, nil);
}

// Converts an attestation result to a JavaScript reply callback invocation.
void OnAttestationResponse(
    ReplyCallback callback,
    const enterprise_connectors::DeviceTrustResponse& response) {
  if (response.error.has_value()) {
    RejectAttestationRequest(std::move(callback), *response.error);
    return;
  }

  if (response.challenge_response.empty()) {
    // An empty response with no error indicates an unexpected internal failure,
    // appropriately represented by kUnknown.
    RejectAttestationRequest(std::move(callback),
                             enterprise_connectors::DeviceTrustError::kUnknown);
    return;
  }

  ResolveAttestationRequest(std::move(callback), response.challenge_response);
}

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
  if (!message.is_main_frame()) {
    RejectAttestationRequest(std::move(callback),
                             ScriptMessageError::kUnsupportedFrame);
    return;
  }

  if (!message.legacy_body() || !message.legacy_body()->is_dict()) {
    RejectAttestationRequest(
        std::move(callback),
        enterprise_connectors::DeviceTrustError::kFailedToParseChallenge);
    return;
  }

  const std::string* challenge =
      message.legacy_body()->GetDict().FindString("challengeRequest");
  if (!challenge || challenge->empty() ||
      challenge->size() > kMaxChallengeRequestLength) {
    RejectAttestationRequest(
        std::move(callback),
        enterprise_connectors::DeviceTrustError::kFailedToParseChallenge);
    return;
  }

  HandleAttestationRequest(web_state, message.security_origin(),
                           message.request_url(), *challenge,
                           std::move(callback));
}

void DeviceTrustJavaScriptFeature::HandleAttestationRequest(
    web::WebState* web_state,
    const url::Origin& security_origin,
    const std::optional<GURL>& request_url,
    const std::string& challenge_request,
    ScriptMessageReplyCallback callback) {
  DeviceTrustChallengeTabHelper* tab_helper =
      DeviceTrustChallengeTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    RejectAttestationRequest(
        std::move(callback),
        enterprise_connectors::DeviceTrustError::kServiceUnavailable);
    return;
  }

  tab_helper->BuildChallengeResponse(
      security_origin, request_url, challenge_request,
      base::BindOnce(&OnAttestationResponse, std::move(callback)));
}
