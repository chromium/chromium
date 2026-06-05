// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_REQUEST_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_REQUEST_H_

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

// Represents a request to create and execute an ActorTool.
//
// This class is a lightweight wrapper around the
// optimization_guide::proto::Action proto used for actuating the browser. This
// is used in the orchestration layer to validate parameters, inspect metadata,
// and resolve target tab IDs before the tool is actually created and executed.
//
// In contrast, `ActorTool` represents the actual instance of a capability and
// holds the state necessary for execution.
class ActorToolRequest {
 public:
  explicit ActorToolRequest(optimization_guide::proto::Action action);
  ~ActorToolRequest();

  ActorToolRequest(const ActorToolRequest&) = delete;
  ActorToolRequest& operator=(const ActorToolRequest&) = delete;
  ActorToolRequest(ActorToolRequest&&) = default;
  ActorToolRequest& operator=(ActorToolRequest&&) = default;

  // Returns the action protobuf for this request.
  const optimization_guide::proto::Action& action() const { return action_; }

  // Returns the type of the tool requested.
  ToolType GetToolType() const;

  // Returns the identifier of the target WebState for this action, if any.
  //
  // Note that this does not validate that the ID points to an active WebState.
  web::WebStateID GetTargetWebStateId() const;

 private:
  // An owned copy of the action proto.
  optimization_guide::proto::Action action_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_REQUEST_H_
