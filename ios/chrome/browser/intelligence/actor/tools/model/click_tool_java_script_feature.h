// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace optimization_guide {
namespace proto {
class ClickAction;
}  // namespace proto
}  // namespace optimization_guide

// A feature that provides methods to execute a click action in the web page.
class ClickToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static ClickToolJavaScriptFeature* GetInstance();

  // Executes the click action on the given WebFrame.
  void Click(web::WebFrame* target_frame,
             const optimization_guide::proto::ClickAction& action,
             ActorTool::ActorCallback callback);

 protected:
  ClickToolJavaScriptFeature();
  ~ClickToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<ClickToolJavaScriptFeature>;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
