// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/wait_tool.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {
// The default wait duration if not specified in the action. This is the same as
// the one being used on Desktop:
// chrome/browser/actor/actor_proto_conversion.cc:CreateWaitRequest.
constexpr base::TimeDelta kDefaultWaitDuration = base::Seconds(3);
}  // namespace

WaitTool::~WaitTool() = default;

// static
base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> WaitTool::Create(
    const optimization_guide::proto::WaitAction& action,
    ProfileIOS* profile) {
  base::TimeDelta wait_duration = kDefaultWaitDuration;
  if (action.has_wait_time_ms()) {
    wait_duration = base::Milliseconds(action.wait_time_ms());
  }

  base::WeakPtr<web::WebState> observe_web_state;
  if (action.has_observe_tab_id()) {
    auto resolution_result = ResolveTab(action.observe_tab_id(), profile);
    if (resolution_result.has_value()) {
      observe_web_state = resolution_result.value().web_state;
    }
  }
  return std::unique_ptr<WaitTool>(
      new WaitTool(wait_duration, observe_web_state));
}

void WaitTool::Execute(ToolExecutionCallback callback) {
  if (observe_web_state_ && observe_web_state_->IsRealized()) {
    // TODO(crbug.com/496164697): Observe the tab and monitor page stability,
    // see also crbug.com/498991756.
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitTool::OnDelayFinished, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)),
      wait_duration_);
}

base::WeakPtr<web::WebState> WaitTool::GetTargetWebState() const {
  return observe_web_state_;
}

optimization_guide::proto::Action::ActionCase WaitTool::GetActionCase() const {
  return optimization_guide::proto::Action::kWait;
}

void WaitTool::OnDelayFinished(ToolExecutionCallback callback) {
  std::move(callback).Run(ToolExecutionResult::Ok());
}

WaitTool::WaitTool(base::TimeDelta wait_duration,
                   base::WeakPtr<web::WebState> observe_web_state)
    : wait_duration_(wait_duration), observe_web_state_(observe_web_state) {}

}  // namespace actor
