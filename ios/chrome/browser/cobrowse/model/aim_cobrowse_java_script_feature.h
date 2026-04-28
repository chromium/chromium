// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_AIM_COBROWSE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_AIM_COBROWSE_JAVA_SCRIPT_FEATURE_H_

#import <string>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

namespace lens {
class ClientToAimMessage;
}  // namespace lens

// A feature that allows native code to communicate with the AIM Cobrowse page.
class AimCobrowseJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static AimCobrowseJavaScriptFeature* GetInstance();

  AimCobrowseJavaScriptFeature(const AimCobrowseJavaScriptFeature&) = delete;
  AimCobrowseJavaScriptFeature& operator=(const AimCobrowseJavaScriptFeature&) =
      delete;

  // Posts a message to the AIM Cobrowse page.
  void PostMessage(web::WebState* web_state,
                   const lens::ClientToAimMessage& message);

 private:
  friend class base::NoDestructor<AimCobrowseJavaScriptFeature>;

  AimCobrowseJavaScriptFeature();
  ~AimCobrowseJavaScriptFeature() override;
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_AIM_COBROWSE_JAVA_SCRIPT_FEATURE_H_
