// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_TRUST_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_DEVICE_TRUST_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class ScriptMessage;
class WebFrame;
class WebState;
}  // namespace web

// JavaScriptFeature that loads the Device Trust support script,
// installs the public API on native request, and receives
// `getAttestation()` requests.
class DeviceTrustJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static DeviceTrustJavaScriptFeature* GetInstance();

  // Attaches the `window.chrome.enterprise.deviceTrust` API to `web_frame`.
  void SetupDeviceTrustAPI(web::WebFrame* web_frame);

  DeviceTrustJavaScriptFeature(const DeviceTrustJavaScriptFeature&) = delete;
  DeviceTrustJavaScriptFeature& operator=(const DeviceTrustJavaScriptFeature&) =
      delete;

 protected:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  bool GetFeatureRepliesToMessages() const override;
  void ScriptMessageReceivedWithReply(
      web::WebState* web_state,
      const web::ScriptMessage& message,
      ScriptMessageReplyCallback callback) override;

  // Handles a validated attestation request. Virtual for test interception.
  virtual void HandleAttestationRequest(web::WebState* web_state,
                                        const std::string& challenge_request,
                                        ScriptMessageReplyCallback callback);

  // Resolves the originating JS promise with the signed payload.
  void ResolveAttestationRequest(ScriptMessageReplyCallback callback,
                                 const std::string& signed_payload);

  // Rejects the originating JS promise with an error code and message.
  void RejectAttestationRequest(ScriptMessageReplyCallback callback,
                                const std::string& error_message,
                                const std::string& error_code);

  DeviceTrustJavaScriptFeature();
  ~DeviceTrustJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<DeviceTrustJavaScriptFeature>;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_TRUST_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_
