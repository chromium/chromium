// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace actor {

ActorEngine::ActorEngine() : state_(State::kInit) {}

ActorEngine::~ActorEngine() = default;

void ActorEngine::ExecuteTools(std::vector<std::unique_ptr<ActorTool>> tools,
                               ExecuteToolsCallback callback) {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::CancelOngoingAndPendingTools(ActorEngineResult reason) {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::AdvanceState() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::ExecuteNextTool() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandlePreExecutionChecks() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleToolVerify() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleUiPreInvoke() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleToolInvoke() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleUiPostInvoke() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleToolCompleted() {
  // TODO(crbug.com/496164779): Implement and test.
}

void ActorEngine::HandleToolFailed() {
  // TODO(crbug.com/496164779): Implement and test.
}

}  // namespace actor
