// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

namespace actor {

// Wraps the result of a tool execution.
//
// This is based on the ActionResult used on Desktop. See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/actor.mojom;l=647;drc=baa6da4a73b262b329328494bdbcaf088374b745
struct ToolExecutionResult {
  ToolExecutionResult(mojom::ActionResultCode external_code,
                      std::optional<std::string> message = std::nullopt)
      : result(ActorToolError(external_code, message)) {}
  ToolExecutionResult(mojom::ActionResultCode external_code,
                      ActorToolErrorCode internal_code,
                      std::optional<std::string> message = std::nullopt)
      : result(ActorToolError(external_code, internal_code, message)) {}
  ToolExecutionResult(ActorToolErrorCode internal_code,
                      std::optional<std::string> message = std::nullopt)
      : result(ActorToolError(internal_code, message)) {}

  mojom::ActionResultCode code() const { return result.external_code; }
  bool IsOk() const { return code() == mojom::ActionResultCode::kOk; }
  static ToolExecutionResult Ok() {
    return ToolExecutionResult(mojom::ActionResultCode::kOk);
  }
  // Temporary helpers while we migrate callsites away from expecting a
  // base::expected.
  // TODO(crbug.com/505037793): remove these helpers once callsites use the
  // above accessors instead.
  bool has_value() const { return IsOk(); }
  ActorToolError error() const {
    CHECK(!IsOk());
    return result;
  }

 private:
  // An internal struct that wraps the error codes and message.
  // TODO(crbug.com/506101134): remove this when no longer needed.
  ActorToolError result;
};

// The callback passed to a tool execution, used to report the result of the
// tool execution.
using ToolExecutionCallback = base::OnceCallback<void(ToolExecutionResult)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_
