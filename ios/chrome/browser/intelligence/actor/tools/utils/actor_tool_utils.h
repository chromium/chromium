// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_TOOL_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_TOOL_UTILS_H_

#import <optional>
#import <string>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// Returns the string representation of the given ToolType to be displayed to
// users when showing the execution status.
std::optional<std::string> ToolTypeToToolDisplayString(ToolType tool);

// Returns the string representation of the given Actor tool if supported.
// This is used for mapping the enum to the "DisabledTools" feature parameter.
std::optional<std::string> ActorActionCaseToToolName(
    optimization_guide::proto::Action::ActionCase tool);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_ACTOR_TOOL_UTILS_H_
