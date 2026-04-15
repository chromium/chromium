// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace optimization_guide::proto {
class TypeAction;
}  // namespace optimization_guide::proto

namespace actor {

// A feature that provides methods to execute a type action in the web page.
class TypeToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static TypeToolJavaScriptFeature* GetInstance();

  // Executes a type action on the given WebFrame.
  void Type(base::WeakPtr<web::WebFrame> target_frame,
            const optimization_guide::proto::TypeAction& action,
            ToolExecutionCallback callback);

 protected:
  TypeToolJavaScriptFeature();
  ~TypeToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<TypeToolJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
