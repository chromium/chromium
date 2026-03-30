// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"

ActorToolFactory::ActorToolFactory() = default;
ActorToolFactory::~ActorToolFactory() = default;

base::expected<std::unique_ptr<ActorTool>, ActorToolError>
ActorToolFactory::CreateTool(const optimization_guide::proto::Action& action,
                             ProfileIOS* profile) {
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kNavigate:
      return NavigateTool::Create(action.navigate(), profile);
    case optimization_guide::proto::Action::kClick:
      return ClickTool::Create(action.click(), profile);
    case optimization_guide::proto::Action::kBack:
      return HistoryTool::Create(action.back(), profile);
    case optimization_guide::proto::Action::kForward:
      return HistoryTool::Create(action.forward(), profile);
    default:
      return base::unexpected(
          ActorToolError{ActorToolErrorCode::kUnsupportedAction});
  }
}
