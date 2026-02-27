// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

class ProfileIOS;
class ClickToolJavaScriptFeature;

namespace web {
class WebState;
}  // namespace web

// Tool to click an element on a page.
class ClickTool : public ActuationTool {
 public:
  ~ClickTool() override;

  static base::expected<std::unique_ptr<ClickTool>, ActuationError> Create(
      const optimization_guide::proto::ClickAction& action,
      ProfileIOS* profile);

  // ActuationTool:
  void Execute(ActuationCallback callback) override;

 private:
  ClickTool(const optimization_guide::proto::ClickAction& action,
            base::WeakPtr<web::WebState> web_state);

  optimization_guide::proto::ClickAction action_;
  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<ClickToolJavaScriptFeature> js_feature_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_CLICK_TOOL_H_
