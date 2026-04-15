// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// A feature that provides methods to execute a scroll action in the web page.
class ScrollToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static ScrollToolJavaScriptFeature* GetInstance();

  // Attempts to apply a directional scroll to a target element in the given
  // WebFrame.
  void Scroll(base::WeakPtr<web::WebFrame> target_frame,
              const optimization_guide::proto::ScrollAction& action,
              ToolExecutionCallback callback);

  // Attempts to bring a target element into visibility within the given
  // WebFrame by scrolling the page or any parent scroll views as needed.
  void ScrollTo(base::WeakPtr<web::WebFrame> target_frame,
                const optimization_guide::proto::ScrollToAction& action,
                ToolExecutionCallback callback);

 protected:
  ScrollToolJavaScriptFeature();
  ~ScrollToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<ScrollToolJavaScriptFeature>;

  // Helper to try executing a Scroll or ScrollTo action in the given WebFrame.
  //
  // If `direction_and_distance` is not provided, this scrolls the page in
  // `web_frame` until the `target` is in view. If it is provided, this
  // scrolls the `target` in `web_frame` by that distance in that direction.
  void ExecuteScrollAction(
      base::WeakPtr<web::WebFrame> web_frame,
      const optimization_guide::proto::ActionTarget& target,
      std::optional<
          std::pair<optimization_guide::proto::ScrollAction_ScrollDirection,
                    int>> direction_and_distance,
      ToolExecutionCallback callback);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SCROLL_TOOL_JAVA_SCRIPT_FEATURE_H_
