// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class Browser;
class ProfileIOS;

namespace web {
class WebState;
class WebFrame;
}  // namespace web

namespace actor {

// Abstract base class for all actor tools.
class ActorTool {
 public:
  // Result of resolving a tab ID to its associated objects.
  struct TabResolutionResult {
    TabResolutionResult();
    TabResolutionResult(const TabResolutionResult&);
    TabResolutionResult& operator=(const TabResolutionResult&);
    ~TabResolutionResult();

    // The browser containing the tab.
    raw_ptr<Browser> browser = nullptr;
    // The index of the tab in the browser's web state list.
    int tab_index = WebStateList::kInvalidIndex;
    // A weak pointer to the tab's WebState.
    base::WeakPtr<web::WebState> web_state;
  };

  virtual ~ActorTool() = default;

  // Executes the tool.
  virtual void Execute(ToolExecutionCallback callback) = 0;

  // Returns the target WebState for this tool, if any.
  virtual base::WeakPtr<web::WebState> GetTargetWebState() const = 0;

  // Returns the target WebFrame for this tool, if any.
  virtual base::WeakPtr<web::WebFrame> GetTargetWebFrame() const;

  // Returns the ActionCase (type of underlying action) for this tool.
  virtual optimization_guide::proto::Action::ActionCase GetActionCase()
      const = 0;

 protected:
  // Resolves the given `tab_id` to its associated objects in regular Browsers.
  // Returns an ToolExecutionResult if the tab or its associated objects are not
  // found.
  static base::expected<TabResolutionResult, ToolExecutionResult> ResolveTab(
      int32_t tab_id,
      ProfileIOS* profile);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_
