// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace optimization_guide::proto {
class TypeAction;
}  // namespace optimization_guide::proto

// A feature that provides methods to execute a type action in the web page.
class TypeToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static TypeToolJavaScriptFeature* GetInstance();

  // Executes the type action on the given WebFrame.
  void Type(web::WebFrame* target_frame,
            const optimization_guide::proto::TypeAction& action,
            ActuationTool::ActuationCallback callback);

 protected:
  TypeToolJavaScriptFeature();
  ~TypeToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<TypeToolJavaScriptFeature>;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
