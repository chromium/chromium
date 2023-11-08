// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// Listens for script command messages for handling supervised user web content.
class SupervisedUserInterstitialJavaScriptFeature
    : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static SupervisedUserInterstitialJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserInterstitialJavaScriptFeature>;
  // JavaScriptFeature implementation.
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;
  SupervisedUserInterstitialJavaScriptFeature();
  ~SupervisedUserInterstitialJavaScriptFeature() override;
  SupervisedUserInterstitialJavaScriptFeature(
      const SupervisedUserInterstitialJavaScriptFeature&) = delete;
  SupervisedUserInterstitialJavaScriptFeature& operator=(
      const SupervisedUserInterstitialJavaScriptFeature&) = delete;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_INTERSTITIAL_JAVA_SCRIPT_FEATURE_H_
