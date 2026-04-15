// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace actor {

ActionResult::ActionResult() = default;
ActionResult::~ActionResult() = default;
ActionResult::ActionResult(ActionResult&&) = default;
ActionResult& ActionResult::operator=(ActionResult&&) = default;
ActionResult::ActionResult(ToolExecutionResult result)
    : tool_result(std::move(result)) {}

}  // namespace actor
