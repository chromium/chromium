// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace security_interstitials {

// Listens for script command messages from error pages.
class IOSSecurityInterstitialJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static IOSSecurityInterstitialJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<IOSSecurityInterstitialJavaScriptFeature>;

  // JavaScriptFeature overrides
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;

  IOSSecurityInterstitialJavaScriptFeature();
  ~IOSSecurityInterstitialJavaScriptFeature() override;

  IOSSecurityInterstitialJavaScriptFeature(
      const IOSSecurityInterstitialJavaScriptFeature&) = delete;
  IOSSecurityInterstitialJavaScriptFeature& operator=(
      const IOSSecurityInterstitialJavaScriptFeature&) = delete;
};

}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_
