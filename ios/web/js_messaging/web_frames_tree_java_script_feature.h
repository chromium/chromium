// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_TREE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_TREE_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class WebFramesTreeJavaScriptFeatureTest;

// A feature that manages frame tree hierarchy and child frame registration
// in the isolated world.
class WebFramesTreeJavaScriptFeature : public JavaScriptFeature {
 public:
  // Returns the singleton instance of `WebFramesTreeJavaScriptFeature`.
  static WebFramesTreeJavaScriptFeature* GetInstance();

  ~WebFramesTreeJavaScriptFeature() override;

  WebFramesTreeJavaScriptFeature(const WebFramesTreeJavaScriptFeature&) =
      delete;
  WebFramesTreeJavaScriptFeature& operator=(
      const WebFramesTreeJavaScriptFeature&) = delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;

 protected:
  WebFramesTreeJavaScriptFeature();

  // JavaScriptFeature:
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

 private:
  friend class base::NoDestructor<WebFramesTreeJavaScriptFeature>;
  friend class WebFramesTreeJavaScriptFeatureTest;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAMES_TREE_JAVA_SCRIPT_FEATURE_H_
