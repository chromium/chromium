// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_NAVIGATE_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_NAVIGATE_TOOL_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

class ProfileIOS;
class UrlLoadingBrowserAgent;
class WebStateList;

namespace optimization_guide {
namespace proto {
class NavigateAction;
}  // namespace proto
}  // namespace optimization_guide

// Command to navigate to a URL.
class NavigateTool : public ActuationTool {
 public:
  ~NavigateTool() override = default;

  static base::expected<std::unique_ptr<NavigateTool>, ActuationError> Create(
      const optimization_guide::proto::NavigateAction& action,
      ProfileIOS* profile);

  // ActuationTool:
  void Execute(ActuationCallback callback) override;

 private:
  NavigateTool(const std::string& url,
               int tab_index,
               WebStateList* web_state_list,
               UrlLoadingBrowserAgent* url_loader);

  const std::string url_;
  int tab_index_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<UrlLoadingBrowserAgent> url_loader_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_NAVIGATE_TOOL_H_
