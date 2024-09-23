// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_JAVA_SCRIPT_FEATURE_H_

#import <CoreGraphics/CoreGraphics.h>

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class BrowserState;
struct ContextMenuParams;
class WebState;

class ContextMenuJavaScriptFeature : public JavaScriptFeature,
                                     public base::SupportsUserData::Data {
 public:
  ContextMenuJavaScriptFeature();
  ~ContextMenuJavaScriptFeature() override;

  // Returns the ContextMenuJavaScriptFeature associated with `browser_state`,
  // creating one if necessary. `browser_state` must not be null.
  static ContextMenuJavaScriptFeature* FromBrowserState(
      BrowserState* browser_state);

  using ElementDetailsCallback =
      base::OnceCallback<void(const std::string& requestID,
                              const web::ContextMenuParams& params)>;
  // Retrieves details of the DOM element at `point` in `web_state`'s currently
  // loaded webpage. `requestID` must be unique and can be used to identify
  // this request as it is returned with the element details in `callback`.
  void GetElementAtPoint(WebState* web_state,
                         std::string requestID,
                         CGPoint point,
                         ElementDetailsCallback callback);

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

 private:
  // Outstanding `callbacks` keyed by requestIDs.
  std::map<std::string, ElementDetailsCallback> callbacks_;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_JAVA_SCRIPT_FEATURE_H_
