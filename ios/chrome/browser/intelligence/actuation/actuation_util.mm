// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"

#import <optional>

#import "components/optimization_guide/proto/features/actions_data.pb.h"

using optimization_guide::proto::Action;

std::optional<std::string> ActuationActionCaseToString(
    Action::ActionCase action) {
  switch (action) {
    case Action::kClick:
      return "ClickAction";
    case Action::kType:
      return "TypeAction";
    case Action::kScroll:
      return "ScrollAction";
    case Action::kMoveMouse:
      return "MoveMouseAction";
    case Action::kDragAndRelease:
      return "DragAndReleaseAction";
    case Action::kSelect:
      return "SelectAction";
    case Action::kNavigate:
      return "NavigateAction";
    case Action::kBack:
      return "HistoryBackAction";
    case Action::kForward:
      return "HistoryForwardAction";
    case Action::kWait:
      return "WaitAction";
    case Action::kCreateTab:
      return "CreateTabAction";
    case Action::kCloseTab:
      return "CloseTabAction";
    case Action::kActivateTab:
      return "ActivateTabAction";
    case Action::kCreateWindow:
      return "CreateWindowAction";
    case Action::kCloseWindow:
      return "CloseWindowAction";
    case Action::kActivateWindow:
      return "ActivateWindowAction";
    case Action::kYieldToUser:
      return "YieldToUserAction";
    case Action::kAttemptLogin:
      return "AttemptLoginAction";
    case Action::kScrollTo:
      return "ScrollToAction";
    case Action::kScriptTool:
      return "ScriptToolAction";
    case Action::kMediaControl:
      return "MediaControlAction";
    case Action::kAttemptFormFilling:
      return "AttemptFormFillingAction";
    case Action::ACTION_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}
