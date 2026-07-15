// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

base::WeakPtr<web::WebFrame> ActorTool::GetTargetWebFrame() const {
  return nullptr;
}

void ActorTool::Cancel() {}

void ActorTool::Validate(ToolExecutionCallback callback) {
  std::move(callback).Run(ToolExecutionResult::Ok());
}

}  // namespace actor
