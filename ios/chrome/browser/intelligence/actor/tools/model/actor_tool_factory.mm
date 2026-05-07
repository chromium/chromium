// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_to_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/wait_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"

namespace actor {

ActorToolFactory::ActorToolFactory() = default;
ActorToolFactory::~ActorToolFactory() = default;

base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
ActorToolFactory::CreateTool(const optimization_guide::proto::Action& action,
                             ProfileIOS* profile) {
  // LINT.IfChange(CreateTool)
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kNavigate:
      return NavigateTool::Create(action.navigate(), profile);
    case optimization_guide::proto::Action::kClick:
      return ClickTool::Create(action.click(), profile);
    case optimization_guide::proto::Action::kBack:
      return HistoryTool::Create(action.back(), profile);
    case optimization_guide::proto::Action::kForward:
      return HistoryTool::Create(action.forward(), profile);
    case optimization_guide::proto::Action::kSelect:
      return SelectTool::Create(action.select(), profile);
    case optimization_guide::proto::Action::kType:
      return TypeTool::Create(action.type(), profile);
    case optimization_guide::proto::Action::kWait:
      return WaitTool::Create(action.wait(), profile);
    case optimization_guide::proto::Action::kScroll:
      return ScrollTool::Create(action.scroll(), profile);
    case optimization_guide::proto::Action::kScrollTo:
      return ScrollToTool::Create(action.scroll_to(), profile);
    default:
      return base::unexpected(
          ToolExecutionResult(InternalToolErrorCode::kUnsupportedAction));
  }
  // LINT.ThenChange(
  //   //ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:SupportedCapabilities,
  //   //ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.mm:InjectTabIdIntoAction
  // )
}

std::vector<optimization_guide::proto::Action::ActionCase>
ActorToolFactory::GetSupportedCapabilities() const {
  // LINT.IfChange(SupportedCapabilities)
  const optimization_guide::proto::Action::ActionCase kCandidates[] = {
      optimization_guide::proto::Action::kNavigate,
      optimization_guide::proto::Action::kClick,
      optimization_guide::proto::Action::kBack,
      optimization_guide::proto::Action::kForward,
      optimization_guide::proto::Action::kType,
      optimization_guide::proto::Action::kWait,
      optimization_guide::proto::Action::kScroll,
      optimization_guide::proto::Action::kScrollTo,
      optimization_guide::proto::Action::kSelect,
  };
  // LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)

  std::vector<optimization_guide::proto::Action::ActionCase> capabilities;
  for (const auto tool : kCandidates) {
    if (!IsToolDisabled(tool)) {
      capabilities.push_back(tool);
    }
  }
  return capabilities;
}

}  // namespace actor
