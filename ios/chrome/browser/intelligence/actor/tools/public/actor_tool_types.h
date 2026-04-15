// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

namespace actor {

// The result of a tool execution, either success or an `ActorToolError`.
using ToolExecutionResult = base::expected<void, ActorToolError>;

// The callback passed to a tool execution, used to report the result of the
// tool execution.
using ToolExecutionCallback = base::OnceCallback<void(ToolExecutionResult)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_
