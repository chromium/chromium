// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"

#import <optional>

#import "components/optimization_guide/proto/features/actions_data.pb.h"

using optimization_guide::proto::Action;

std::optional<std::string> ActuationActionCaseToToolName(
    Action::ActionCase tool) {
  switch (tool) {
    case Action::kClick:
      return "ClickTool";
    case Action::kType:
      return "TypeTool";
    case Action::kScroll:
      return "ScrollTool";
    case Action::kMoveMouse:
      return "MoveMouseTool";
    case Action::kDragAndRelease:
      return "DragAndReleaseTool";
    case Action::kSelect:
      return "SelectTool";
    case Action::kNavigate:
      return "NavigateTool";
    case Action::kBack:
      return "HistoryBackTool";
    case Action::kForward:
      return "HistoryForwardTool";
    case Action::kWait:
      return "WaitTool";
    case Action::kCreateTab:
      return "CreateTabTool";
    case Action::kCloseTab:
      return "CloseTabTool";
    case Action::kActivateTab:
      return "ActivateTabTool";
    case Action::kCreateWindow:
      return "CreateWindowTool";
    case Action::kCloseWindow:
      return "CloseWindowTool";
    case Action::kActivateWindow:
      return "ActivateWindowTool";
    case Action::kYieldToUser:
      return "YieldToUserTool";
    case Action::kAttemptLogin:
      return "AttemptLoginTool";
    case Action::kScrollTo:
      return "ScrollToTool";
    case Action::kScriptTool:
      return "ScriptToolTool";
    case Action::kMediaControl:
      return "MediaControlTool";
    case Action::kAttemptFormFilling:
      return "AttemptFormFillingTool";
    case Action::kLoadAndExtractContent:
      return "LoadAndExtractContentTool";
    case Action::ACTION_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}
