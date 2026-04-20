// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/json_action_parser.h"

#import <string>

#import "base/logging.h"
#import "base/strings/string_number_conversions.h"

namespace ai_prototyping {

namespace {

// Based on available Actions in
// components/optimization_guide/proto/features/actions_data.proto.
enum class ActionType {
  kUnknown,
  kNavigate,
  kClick,
  kHistoryBack,
  kHistoryForward,
  kType,
  kWait,
  kScroll,
  kScrollTo,
  kSelect,
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
  if (key == "back") {
    return ActionType::kHistoryBack;
  }
  if (key == "forward") {
    return ActionType::kHistoryForward;
  }
  if (key == "type") {
    return ActionType::kType;
  }
  if (key == "wait") {
    return ActionType::kWait;
  }
  if (key == "scroll") {
    return ActionType::kScroll;
  }
  if (key == "scroll_to") {
    return ActionType::kScrollTo;
  }
  if (key == "select") {
    return ActionType::kSelect;
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

bool MapHistoryBackAction(const base::DictValue& dict,
                          optimization_guide::proto::Action* action) {
  auto* history_back = action->mutable_back();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    history_back->set_tab_id(*tab_id);
  }
  return history_back->ByteSizeLong() > 0;
}

bool MapHistoryForwardAction(const base::DictValue& dict,
                             optimization_guide::proto::Action* action) {
  auto* history_forward = action->mutable_forward();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    history_forward->set_tab_id(*tab_id);
  }
  return history_forward->ByteSizeLong() > 0;
}

// Helper method that retrieves the coordinates for point-based actions like
// click.
void MapCoordinate(const base::DictValue& dict,
                   optimization_guide::proto::Coordinate* coordinate) {
  if (std::optional<int> x = dict.FindInt("x")) {
    coordinate->set_x(*x);
  }
  if (std::optional<int> y = dict.FindInt("y")) {
    coordinate->set_y(*y);
  }
}

// Helper method that retrieves the target element for point-based actions like
// click.
void MapActionTarget(const base::DictValue& dict,
                     optimization_guide::proto::ActionTarget* target) {
  if (const base::DictValue* coordinate = dict.FindDict("coordinate")) {
    MapCoordinate(*coordinate, target->mutable_coordinate());
    return;
  }
  if (std::optional<int> content_node_id = dict.FindInt("content_node_id")) {
    target->set_content_node_id(*content_node_id);
  }
  if (const std::string* document_identifier =
          dict.FindString("document_identifier")) {
    target->mutable_document_identifier()->set_serialized_token(
        *document_identifier);
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

bool MapTypeAction(const base::DictValue& dict,
                   optimization_guide::proto::Action* action) {
  auto* type = action->mutable_type();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    type->set_tab_id(*tab_id);
  }
  if (const base::DictValue* target = dict.FindDict("target")) {
    MapActionTarget(*target, type->mutable_target());
  }
  if (const std::string* text = dict.FindString("text")) {
    type->set_text(*text);
  }
  if (std::optional<int> mode = dict.FindInt("mode")) {
    if (optimization_guide::proto::TypeAction_TypeMode_IsValid(*mode)) {
      type->set_mode(
          static_cast<optimization_guide::proto::TypeAction_TypeMode>(*mode));
    }
  }
  if (std::optional<bool> follow_by_enter = dict.FindBool("follow_by_enter")) {
    type->set_follow_by_enter(*follow_by_enter);
  }
  return type->ByteSizeLong() > 0;
}

bool MapWaitAction(const base::DictValue& dict,
                   optimization_guide::proto::Action* action) {
  auto* wait = action->mutable_wait();
  if (std::optional<int> wait_time_ms = dict.FindInt("wait_time_ms")) {
    wait->set_wait_time_ms(*wait_time_ms);
  }
  if (std::optional<int> observe_tab_id = dict.FindInt("observe_tab_id")) {
    wait->set_observe_tab_id(*observe_tab_id);
  }
  return wait->ByteSizeLong() > 0;
}

bool MapScrollAction(const base::DictValue& dict,
                     optimization_guide::proto::Action* action) {
  auto* scroll = action->mutable_scroll();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    scroll->set_tab_id(*tab_id);
  }
  if (const base::DictValue* target = dict.FindDict("target")) {
    MapActionTarget(*target, scroll->mutable_target());
  }
  if (std::optional<int> direction = dict.FindInt("direction")) {
    if (optimization_guide::proto::ScrollAction_ScrollDirection_IsValid(
            *direction)) {
      scroll->set_direction(
          static_cast<optimization_guide::proto::ScrollAction_ScrollDirection>(
              *direction));
    }
  }
  if (const std::string* distance = dict.FindString("distance")) {
    double distance_value;
    base::StringToDouble(*distance, &distance_value);
    scroll->set_distance(distance_value);
  }
  return scroll->ByteSizeLong() > 0;
}

bool MapScrollToAction(const base::DictValue& dict,
                       optimization_guide::proto::Action* action) {
  auto* scroll = action->mutable_scroll_to();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    scroll->set_tab_id(*tab_id);
  }
  if (const base::DictValue* target = dict.FindDict("target")) {
    MapActionTarget(*target, scroll->mutable_target());
  }
  return scroll->ByteSizeLong() > 0;
}

bool MapSelectAction(const base::DictValue& dict,
                     optimization_guide::proto::Action* action) {
  auto* select = action->mutable_select();
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    select->set_tab_id(*tab_id);
  }
  if (const base::DictValue* target = dict.FindDict("target")) {
    MapActionTarget(*target, select->mutable_target());
  }
  if (const std::string* value = dict.FindString("value")) {
    select->set_value(*value);
  }
  return select->ByteSizeLong() > 0;
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
    case ActionType::kHistoryBack:
      return MapHistoryBackAction(value.GetDict(), action);
    case ActionType::kHistoryForward:
      return MapHistoryForwardAction(value.GetDict(), action);
    case ActionType::kType:
      return MapTypeAction(value.GetDict(), action);
    case ActionType::kWait:
      return MapWaitAction(value.GetDict(), action);
    case ActionType::kScroll:
      return MapScrollAction(value.GetDict(), action);
    case ActionType::kScrollTo:
      return MapScrollToAction(value.GetDict(), action);
    case ActionType::kSelect:
      return MapSelectAction(value.GetDict(), action);
    case ActionType::kUnknown:
      return false;
  }
}

}  // namespace ai_prototyping
