// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"

#import <optional>

#import "base/notreached.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

using optimization_guide::proto::Action;

std::optional<std::string> ToolTypeToToolDisplayString(ToolType tool) {
  switch (tool) {
    case ToolType::kClick:
      return "Clicking";
    case ToolType::kType:
      return "Typing";
    case ToolType::kScroll:
      return "Scrolling";
    case ToolType::kSelect:
      return "Selecting";
    case ToolType::kDragAndRelease:
      return "Dragging";
    case ToolType::kNavigate:
      return "Navigating";
    case ToolType::kBack:
      return "Rewinding history";
    case ToolType::kForward:
      return "Forwarding history";
    case ToolType::kWait:
    case ToolType::kWaitZeroDuration:
      return "Waiting";
    case ToolType::kScrollTo:
      return "Scrolling";
    case ToolType::kAttemptLogin:
      return "Attempting login";
    case ToolType::kAttemptFormFilling:
      return "Attempting form filling";
    case ToolType::kCloseTab:
      return "Closing tab";
    case ToolType::kCreateTab:
      return "Creating tab";
    case ToolType::kActivateTab:
      return "Activating tab";
    case ToolType::kUnknown:
      return std::nullopt;
  }
  NOTREACHED();
}

// LINT.IfChange(IOSActorUiToolName)
std::string ToolTypeToToolName(ToolType tool) {
  switch (tool) {
    case ToolType::kClick:
      return "ClickTool";
    case ToolType::kType:
      return "TypeTool";
    case ToolType::kScroll:
      return "ScrollTool";
    case ToolType::kSelect:
      return "SelectTool";
    case ToolType::kDragAndRelease:
      return "DragAndReleaseTool";
    case ToolType::kNavigate:
      return "NavigateTool";
    case ToolType::kBack:
      return "HistoryBackTool";
    case ToolType::kForward:
      return "HistoryForwardTool";
    case ToolType::kWait:
    case ToolType::kWaitZeroDuration:
      return "WaitTool";
    case ToolType::kScrollTo:
      return "ScrollToTool";
    case ToolType::kAttemptLogin:
      return "AttemptLoginTool";
    case ToolType::kAttemptFormFilling:
      return "AttemptFormFillingTool";
    case ToolType::kCloseTab:
      return "CloseTabTool";
    case ToolType::kCreateTab:
      return "CreateTabTool";
    case ToolType::kActivateTab:
      return "ActivateTabTool";
    case ToolType::kUnknown:
      return "UnknownTool";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/histograms.xml:IOSActorUiToolName)

std::optional<std::string> ActorActionCaseToToolName(Action::ActionCase tool) {
  switch (tool) {
    case Action::kClick:
      return ToolTypeToToolName(ToolType::kClick);
    case Action::kType:
      return ToolTypeToToolName(ToolType::kType);
    case Action::kScroll:
      return ToolTypeToToolName(ToolType::kScroll);
    case Action::kDragAndRelease:
      return ToolTypeToToolName(ToolType::kDragAndRelease);
    case Action::kSelect:
      return ToolTypeToToolName(ToolType::kSelect);
    case Action::kNavigate:
      return ToolTypeToToolName(ToolType::kNavigate);
    case Action::kBack:
      return ToolTypeToToolName(ToolType::kBack);
    case Action::kForward:
      return ToolTypeToToolName(ToolType::kForward);
    case Action::kWait:
      return ToolTypeToToolName(ToolType::kWait);
    case Action::kCreateTab:
      return ToolTypeToToolName(ToolType::kCreateTab);
    case Action::kCloseTab:
      return ToolTypeToToolName(ToolType::kCloseTab);
    case Action::kActivateTab:
      return ToolTypeToToolName(ToolType::kActivateTab);
    case Action::kAttemptLogin:
      return ToolTypeToToolName(ToolType::kAttemptLogin);
    case Action::kScrollTo:
      return ToolTypeToToolName(ToolType::kScrollTo);
    case Action::kAttemptFormFilling:
      return ToolTypeToToolName(ToolType::kAttemptFormFilling);
    case Action::ACTION_NOT_SET:
    default:
      return std::nullopt;
  }
}

}  // namespace actor
