// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace url {
class Origin;
}  // namespace url

class GURL;

namespace web {
class ScriptMessage;
class WebFrame;
class WebState;
}  // namespace web

// JavaScriptFeature that receives `getAttestation()` requests from the
// `window.chrome.enterprise.deviceTrust` API. The script implementing the API
// is not injected into every page: it is executed on browser-side request in
// the main frame of pages allowlisted by the Device Trust policies.
class DeviceTrustJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static DeviceTrustJavaScriptFeature* GetInstance();

  // Executes the script installing the `window.chrome.enterprise.deviceTrust`
  // API in `web_frame`, which must be a main frame. Executing it more than
  // once in the same window has no effect. Returns true if the execution was
  // requested.
  bool SetupDeviceTrustAPI(web::WebFrame* web_frame);

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

  DeviceTrustJavaScriptFeature();
  ~DeviceTrustJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<DeviceTrustJavaScriptFeature>;

  // Handles a validated attestation request.
  void HandleAttestationRequest(web::WebState* web_state,
                                const url::Origin& security_origin,
                                const std::optional<GURL>& request_url,
                                const std::string& challenge_request,
                                ScriptMessageReplyCallback callback);
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_JAVA_SCRIPT_FEATURE_H_
