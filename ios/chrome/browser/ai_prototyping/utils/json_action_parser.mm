// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/json_action_parser.h"

#import <string>

#import "base/logging.h"

namespace ai_prototyping {

namespace {

enum class ActionType {
  kUnknown,
  kNavigate,
  kClick,
};

// Based on the field names in
// components/optimization_guide/proto/features/actions_data.proto.
ActionType GetActionType(const std::string& key) {
  if (key == "navigate") {
    return ActionType::kNavigate;
  }
  if (key == "click") {
    return ActionType::kClick;
  }
  return ActionType::kUnknown;
}

bool MapNavigateAction(const base::DictValue& dict,
                       optimization_guide::proto::Action* action) {
  auto* navigate = action->mutable_navigate();
  if (const std::string* url = dict.FindString("url")) {
    navigate->set_url(*url);
  }
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    navigate->set_tab_id(*tab_id);
  }
  return navigate->ByteSizeLong() > 0;
}

void MapCoordinate(const base::DictValue& dict,
                   optimization_guide::proto::Coordinate* coordinate) {
  if (std::optional<int> x = dict.FindInt("x")) {
    coordinate->set_x(*x);
  }
  if (std::optional<int> y = dict.FindInt("y")) {
    coordinate->set_y(*y);
  }
}

void MapActionTarget(const base::DictValue& dict,
                     optimization_guide::proto::ActionTarget* target) {
  if (const base::DictValue* coordinate = dict.FindDict("coordinate")) {
    MapCoordinate(*coordinate, target->mutable_coordinate());
  }
}

bool MapClickAction(const base::DictValue& dict,
                    optimization_guide::proto::Action* action) {
  auto* click = action->mutable_click();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    click->set_tab_id(*tab_id);
  }
  if (const base::DictValue* target = dict.FindDict("target")) {
    MapActionTarget(*target, click->mutable_target());
  }
  if (std::optional<int> click_type = dict.FindInt("click_type")) {
    if (optimization_guide::proto::ClickAction_ClickType_IsValid(*click_type)) {
      click->set_click_type(
          static_cast<optimization_guide::proto::ClickAction_ClickType>(
              *click_type));
    }
  }
  if (std::optional<int> click_count = dict.FindInt("click_count")) {
    if (optimization_guide::proto::ClickAction_ClickCount_IsValid(
            *click_count)) {
      click->set_click_count(
          static_cast<optimization_guide::proto::ClickAction_ClickCount>(
              *click_count));
    }
  }
  return click->ByteSizeLong() > 0;
}

}  // namespace

bool ParseActionFromDict(const base::DictValue& dict,
                         optimization_guide::proto::Action* action) {
  if (dict.empty()) {
    return false;
  }

  // Get the first entry in the dictionary.
  const auto it = dict.begin();
  const std::string& key = it->first;
  const base::Value& value = it->second;

  if (!value.is_dict()) {
    return false;
  }

  switch (GetActionType(key)) {
    case ActionType::kNavigate:
      return MapNavigateAction(value.GetDict(), action);
    case ActionType::kClick:
      return MapClickAction(value.GetDict(), action);
    case ActionType::kUnknown:
      return false;
  }
}

}  // namespace ai_prototyping
