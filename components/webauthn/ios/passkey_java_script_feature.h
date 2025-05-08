// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// Communicates with the JavaScript file passkey_controller.ts, which contains
// a shim of the navigator.credentials API.
//
// The main intent of the shim is to facilitate certain passkey functionality
// (e.g. assertion / creation) in Chromium, hence the name of this class. It is
// worth noting though, that the navigator.credentials API might be used for any
// type of credential. Requests for non-passkey credentials are not handled by
// this feature (with a possible exception of logging metrics).
class PasskeyJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static PasskeyJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<PasskeyJavaScriptFeature>;

  PasskeyJavaScriptFeature();
  PasskeyJavaScriptFeature(const PasskeyJavaScriptFeature&) = delete;
  PasskeyJavaScriptFeature& operator=(const PasskeyJavaScriptFeature&) = delete;
  ~PasskeyJavaScriptFeature() override;

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_
