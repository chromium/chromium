// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace base {
class Value;
}

namespace web {
class WebFrame;
}  // namespace web

namespace optimization_guide {
namespace proto {
class ClickAction;
}  // namespace proto
}  // namespace optimization_guide

// A feature that provides methods to execute various actions in the web page.
class ClickToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static ClickToolJavaScriptFeature* GetInstance();

  // Executes the click action on the given WebFrame.
  void Click(web::WebFrame* web_frame,
             const optimization_guide::proto::ClickAction& action,
             ActuationTool::ActuationCallback callback);

 protected:
  ClickToolJavaScriptFeature();
  ~ClickToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<ClickToolJavaScriptFeature>;

  void ProcessClickResult(ActuationTool::ActuationCallback callback,
                          const base::Value* click_result);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
