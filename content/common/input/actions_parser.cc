// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include <utility>

#include "base/format_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace content {

namespace {

SyntheticPointerActionParams::PointerActionType ToSyntheticPointerActionType(
    std::string action_type) {
  if (action_type == "pointerDown")
    return SyntheticPointerActionParams::PointerActionType::PRESS;
  if (action_type == "pointerMove")
    return SyntheticPointerActionParams::PointerActionType::MOVE;
  if (action_type == "pointerUp")
    return SyntheticPointerActionParams::PointerActionType::RELEASE;
  if (action_type == "pointerLeave")
    return SyntheticPointerActionParams::PointerActionType::LEAVE;
  if (action_type == "pause")
    return SyntheticPointerActionParams::PointerActionType::IDLE;
  return SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED;
}

SyntheticGestureParams::GestureSourceType ToSyntheticGestureSourceType(
    std::string pointer_type) {
  if (pointer_type == "touch")
    return SyntheticGestureParams::TOUCH_INPUT;
  else if (pointer_type == "mouse")
    return SyntheticGestureParams::MOUSE_INPUT;
  else if (pointer_type == "pen")
    return SyntheticGestureParams::PEN_INPUT;
  else
    return SyntheticGestureParams::DEFAULT_INPUT;
}

SyntheticPointerActionParams::Button ToSyntheticMouseButton(int button) {
  if (button == 0)
    return SyntheticPointerActionParams::Button::LEFT;
  if (button == 1)
    return SyntheticPointerActionParams::Button::MIDDLE;
  if (button == 2)
    return SyntheticPointerActionParams::Button::RIGHT;
  if (button == 3)
    return SyntheticPointerActionParams::Button::BACK;
  if (button == 4)
    return SyntheticPointerActionParams::Button::FORWARD;
  NOTREACHED() << "Unexpected button";
  return SyntheticPointerActionParams::Button();
}

int ToKeyModifiers(std::string key) {
  if (key == "Alt")
    return blink::WebInputEvent::kAltKey;
  if (key == "Control")
    return blink::WebInputEvent::kControlKey;
  if (key == "Meta")
    return blink::WebInputEvent::kMetaKey;
  if (key == "Shift")
    return blink::WebInputEvent::kShiftKey;
  if (key == "CapsLock")
    return blink::WebInputEvent::kCapsLockOn;
  if (key == "NumLock")
    return blink::WebInputEvent::kNumLockOn;
  if (key == "AltGraph")
    return blink::WebInputEvent::kAltGrKey;
  return 0;
}

}  // namespace

ActionsParser::ActionsParser(base::Value pointer_actions_value)
    : longest_action_sequence_(0),
      pointer_actions_value_(std::move(pointer_actions_value)),
      action_index_(0),
      use_testdriver_api_(true) {}

ActionsParser::~ActionsParser() {}

bool ActionsParser::ParsePointerActionSequence() {
  if (!pointer_actions_value_.is_list()) {
    error_message_ = std::string("provided value is not a list");
    return false;
  }

  int index = 0;
  for (const auto& pointer_actions : pointer_actions_value_.GetList()) {
    if (!pointer_actions.is_dict()) {
      error_message_ =
          std::string("pointer actions is missing or not a dictionary");
      return false;
    } else if (!ParsePointerActions(pointer_actions, index)) {
      return false;
    }
    index++;
    if (source_type_ == "pointer" || !use_testdriver_api_)
      action_index_++;
  }

  gesture_params_.gesture_source_type =
      ToSyntheticGestureSourceType(pointer_type_);
  // Group a list of actions from all pointers into a
  // SyntheticPointerActionListParams object, which is a list of actions, which
  // will be dispatched together.
  for (size_t action_index = 0; action_index < longest_action_sequence_;
       ++action_index) {
    SyntheticPointerActionListParams::ParamList param_list;
    size_t longest_pause_frame = 0;
    for (const auto pointer_action_list : pointer_actions_list_) {
      if (action_index < pointer_action_list.size()) {
        param_list.push_back(pointer_action_list[action_index]);
        if (pointer_action_list[action_index].pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::IDLE) {
          size_t num_pause_frame = static_cast<size_t>(std::ceil(
              pointer_action_list[action_index].duration().InMilliseconds() /
              viz::BeginFrameArgs::DefaultInterval().InMilliseconds()));
          longest_pause_frame = std::max(longest_pause_frame, num_pause_frame);
        }
      }
    }
    gesture_params_.PushPointerActionParamsList(param_list);

    for (size_t pause_index = 1; pause_index < longest_pause_frame;
         ++pause_index) {
      SyntheticPointerActionListParams::ParamList pause_param_list;
      SyntheticPointerActionParams pause_action_param(
          SyntheticPointerActionParams::PointerActionType::IDLE);
      for (size_t i = 0; i < param_list.size(); ++i) {
        pause_param_list.push_back(pause_action_param);
      }
      gesture_params_.PushPointerActionParamsList(pause_param_list);
    }
  }
  return true;
}

bool ActionsParser::ParsePointerActions(const base::Value& pointer, int index) {
  int pointer_id = -1;
  // If the json format of each pointer has "type" element, it is from the new
  // Action API, otherwise it is from gpuBenchmarking.pointerActionSequence
  // API. We have to keep both formats for now, but later on once we switch to
  // the new Action API in all tests, we will remove the old format.
  const base::Value* type_key = pointer.FindKey("type");
  if (index == 0)
    use_testdriver_api_ = type_key != nullptr;

  if (use_testdriver_api_) {
    DCHECK_NE(nullptr, type_key);
    if (!type_key->is_string()) {
      error_message_ =
          std::string("action sequence type is missing or not a string");
      return false;
    }
    std::string source_type = type_key->GetString();
    if (source_type == "") {
      error_message_ = std::string("action sequence type cannot be empty");
      return false;
    } else if (source_type == "key") {
      error_message_ =
          std::string("we do not support action sequence type of key");
      return false;
    }

    if (source_type == "pointer") {
      if (source_type_.empty())
        source_type_ = source_type;

      const base::Value* parameters = pointer.FindKey("parameters");
      if (source_type_ == "pointer" && !parameters) {
        error_message_ = std::string(
            "action sequence parameters is missing for pointer type");
        return false;
      }

      if (!parameters->is_dict()) {
        error_message_ =
            std::string("action sequence parameters is not a dictionary");
        return false;
      }

      const std::string* pointer_type =
          parameters->FindStringKey("pointerType");
      if (!pointer_type) {
        error_message_ = std::string(
            "action sequence pointer type is missing or not a string");
        return false;
      } else if (*pointer_type != "touch" && *pointer_type != "mouse" &&
                 *pointer_type != "pen") {
        error_message_ = std::string(
            "action sequence pointer type is an unsupported input type");
        return false;
      }

      if (pointer_type_.empty()) {
        pointer_type_ = *pointer_type;
      }

      if (pointer_type_ != *pointer_type) {
        error_message_ = std::string(
            "currently multiple action sequence pointer type are not "
            "supported");
        return false;
      }

      if (*pointer_type != "touch" && action_index_ > 0) {
        error_message_ = std::string(
            "for input type of mouse and pen, we only support one device");
        return false;
      }

      const std::string* pointer_name = pointer.FindStringKey("id");
      if (!pointer_name) {
        error_message_ = std::string("pointer name is missing or not a string");
        return false;
      }

      if (pointer_name_set_.find(*pointer_name) != pointer_name_set_.end()) {
        error_message_ = std::string("pointer name already exists");
        return false;
      }

      pointer_name_set_.insert(*pointer_name);
      pointer_id_set_.insert(action_index_);
    }
    pointer_id = action_index_;
  } else {
    DCHECK_EQ(nullptr, type_key);
    const std::string* pointer_type = pointer.FindStringKey("source");
    if (!pointer_type) {
      error_message_ = std::string("source type is missing or not a string");
      return false;
    } else if (*pointer_type != "touch" && *pointer_type != "mouse" &&
               *pointer_type != "pen") {
      error_message_ =
          std::string("source type is an unsupported input source");
      return false;
    }

    if (pointer_type_.empty()) {
      pointer_type_ = *pointer_type;
    }

    if (pointer_type_ != *pointer_type) {
      error_message_ =
          std::string("currently multiple input sources are not not supported");
      return false;
    }

    if (*pointer_type != "touch" && action_index_ > 0) {
      error_message_ = std::string(
          "for input source type of mouse and pen, we only support one device "
          "in one sequence");
      return false;
    }

    const base::Value* id_key = pointer.FindKey("id");
    if (id_key) {
      if (!id_key->is_int()) {
        error_message_ = std::string("pointer id is not an integer");
        return false;
      }
      pointer_id = id_key->GetInt();

      if (pointer_id < 0) {
        error_message_ = std::string("pointer id can not be negative");
        return false;
      }
    }

    if (pointer_id != -1) {
      if (pointer_id_set_.find(pointer_id) != pointer_id_set_.end()) {
        error_message_ = std::string("pointer id already exists");
        return false;
      }

      if (action_index_ != static_cast<int>(pointer_id_set_.size())) {
        error_message_ = std::string("some pointers do not have a pointer id");
        return false;
      }

      pointer_id_set_.insert(pointer_id);
    } else {
      if (pointer_id_set_.size() > 0) {
        error_message_ = std::string("this pointer does not have a pointer id");
        return false;
      }
    }
  }

  const base::Value* actions =
      pointer.FindKeyOfType("actions", base::Value::Type::LIST);
  if (!actions) {
    error_message_ = base::StringPrintf(
        "pointer[%d].actions is missing or not a list", action_index_);
    return false;
  }

  if (!ParseActions(*actions, pointer_id))
    return false;

  return true;
}

bool ActionsParser::ParseActions(const base::Value& actions, int pointer_id) {
  SyntheticPointerActionListParams::ParamList param_list;
  for (const auto& action : actions.GetList()) {
    if (!action.is_dict()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions is missing or not a dictionary", action_index_);
      return false;
    } else if (!ParseAction(action, param_list, pointer_id)) {
      return false;
    }
  }

  if (param_list.size() > longest_action_sequence_)
    longest_action_sequence_ = param_list.size();

  pointer_actions_list_.push_back(param_list);
  return true;
}

bool ActionsParser::ParseAction(
    const base::Value& action,
    SyntheticPointerActionListParams::ParamList& param_list,
    int pointer_id) {
  SyntheticPointerActionParams::PointerActionType pointer_action_type =
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED;
  std::string type;
  const base::Value* type_key = action.FindKey("type");
  if (type_key) {
    if (!type_key->is_string()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.name is missing or not a string", action_index_);
      return false;
    }
    type = type_key->GetString();
  } else {
    const std::string* name_key = action.FindStringKey("name");
    if (!name_key) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.name is missing or not a string", action_index_);
      return false;
    }
    type = *name_key;
  }

  pointer_action_type = ToSyntheticPointerActionType(type);
  if (pointer_action_type ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    error_message_ = base::StringPrintf(
        "actions[%d].actions.name is an unsupported action name",
        action_index_);
    return false;
  }

  double position_x = 0;
  double position_y = 0;
  const base::Value* position_x_key = action.FindKey("x");
  const base::Value* position_y_key = action.FindKey("y");
  if (position_x_key) {
    if (!position_x_key->is_int() && !position_x_key->is_double()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.x is not a number", action_index_);
      return false;
    }
    position_x = position_x_key->GetDouble();
  }

  if (position_y_key) {
    if (!position_y_key->is_int() && !position_y_key->is_double()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.y is not a number", action_index_);
      return false;
    }
    position_y = position_y_key->GetDouble();
  }

  int button_id = 0;
  const base::Value* button_id_key = action.FindKey("button");
  if (button_id_key) {
    if (!button_id_key->is_int()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.button is not a string", action_index_);
      return false;
    }
    button_id = button_id_key->GetInt();
  }
  if (button_id < 0 || button_id > 4) {
    error_message_ = base::StringPrintf(
        "actions[%d].actions.button is an unsupported button", action_index_);
    return false;
  }
  SyntheticPointerActionParams::Button button =
      ToSyntheticMouseButton(button_id);

  std::string keys;
  const base::Value* keys_key = action.FindKey("keys");
  if (keys_key) {
    if (!keys_key->is_string()) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.key is not a string", action_index_);
      return false;
    }
    keys = keys_key->GetString();
  }

  int key_modifiers = 0;
  std::vector<std::string> key_list =
      base::SplitString(keys, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string& key : key_list) {
    int key_modifier = ToKeyModifiers(key);
    if (key_modifier == 0) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.key is not a valid key", action_index_);
      return false;
    }
    key_modifiers |= key_modifier;
  }

  int duration = viz::BeginFrameArgs::DefaultInterval().InMilliseconds();
  if (pointer_action_type ==
      SyntheticPointerActionParams::PointerActionType::IDLE) {
    const base::Value* duration_key = action.FindKey("duration");
    if (duration_key) {
      if (!duration_key->is_double() && !duration_key->is_int()) {
        error_message_ = base::StringPrintf(
            "actions[%d].actions.duration is not a number", action_index_);
        return false;
      }
      duration = duration_key->GetDouble();
    }
    if (duration < 0) {
      error_message_ = base::StringPrintf(
          "actions[%d].actions.duration should not be negative", action_index_);
      return false;
    }
  }

  SyntheticPointerActionParams action_param(pointer_action_type);
  if (pointer_id == -1)
    action_param.set_pointer_id(action_index_);
  else
    action_param.set_pointer_id(pointer_id);
  switch (pointer_action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      action_param.set_position(gfx::PointF(position_x, position_y));
      action_param.set_button(button);
      action_param.set_key_modifiers(key_modifiers);
      break;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      action_param.set_position(gfx::PointF(position_x, position_y));
      action_param.set_key_modifiers(key_modifiers);
      break;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      action_param.set_button(button);
      action_param.set_key_modifiers(key_modifiers);
      break;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
      action_param.set_duration(base::TimeDelta::FromMilliseconds(duration));
      break;
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      break;
  }
  param_list.push_back(action_param);
  return true;
}

}  // namespace content
