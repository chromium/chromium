// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_JAVA_SCRIPT_FEATURE_H_

#import <optional>
#import <string>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class ScriptMessage;
class WebState;
}  // namespace web

// This JavaScriptFeature intercepts trusted text copies from web frames in
// order to evaluate them for potentially malicious commands. If potentially
// malicious commands are found, the text is used as a signal for client-side
// phishing detection.
class ClientSideDetectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Returns the singleton instance of `ClientSideDetectionJavaScriptFeature`.
  static ClientSideDetectionJavaScriptFeature* GetInstance();

  ~ClientSideDetectionJavaScriptFeature() override;

  ClientSideDetectionJavaScriptFeature(
      const ClientSideDetectionJavaScriptFeature&) = delete;
  ClientSideDetectionJavaScriptFeature& operator=(
      const ClientSideDetectionJavaScriptFeature&) = delete;

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 private:
  friend class base::NoDestructor<ClientSideDetectionJavaScriptFeature>;

  ClientSideDetectionJavaScriptFeature();
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_JAVA_SCRIPT_FEATURE_H_
