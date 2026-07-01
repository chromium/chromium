// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace web {
class WebState;
class WebFrame;
}  // namespace web

namespace actor {

// Abstract base class for all actor tools.
class ActorTool {
 public:
  virtual ~ActorTool() = default;

  // Executes the tool.
  virtual void Execute(ToolExecutionCallback callback) = 0;

  // Cancels the tool execution.
  virtual void Cancel();

  // Returns the target WebState for this tool, if any.
  virtual base::WeakPtr<web::WebState> GetTargetWebState() const = 0;

  // Returns the target WebFrame for this tool, if any.
  virtual base::WeakPtr<web::WebFrame> GetTargetWebFrame() const;

  // Returns the type of this tool.
  virtual ToolType GetToolType() const = 0;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_H_
