// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_ACTIONS_PARSER_H_
#define CONTENT_COMMON_INPUT_ACTIONS_PARSER_H_

#include <cstddef>
#include <set>
#include <string>

#include "base/values.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"

namespace content {

// This class takes the argument of JSON format from
// gpuBenchmarking.pointerActionSequence and testdriver Action API.
// gpuBenchmarking.pointerActionSequence is used in all the web tests, and
// testdriver Action API is used in the WPT tests. We will eventually
// transition to testdriver, so that the web tests and WPT tests will use
// a unified way to inject user inputs.
// Testdriver Action API is an implementation of Webdriver Action API
// https://www.w3.org/TR/webdriver/#processing-actions.
// TestDriver API input is a list of ActionSequences, each of which represents
// a single device. Each action sequence has device-properties and a list of
// ActionItems that define the action to take at each tick.
// ActionSequenceList = [
//   ActionSequence = {
//      type:
//      id:
//      parameters:
//      actions: [ ActionItem { type, duration, etc. }, * ]
//   }, *
// ]
// gpuBenchmarking.pointerActionSequence takes a different JSON format, an
// example can be found here in third_party/blink/web_tests/fast/events/
// pointerevents/multi-pointer-event-in-slop-region.html.
// This class parses the JSON string and warps it into a SyntehticGestureParams
// object that can be used to inject input into Chrome.
class CONTENT_EXPORT ActionsParser {
 public:
  explicit ActionsParser(base::Value value);

  ActionsParser(const ActionsParser&) = delete;
  ActionsParser& operator=(const ActionsParser&) = delete;

  ~ActionsParser();
  bool Parse();
  const std::string& error_message() const { return error_message_; }

  SyntheticGestureParams::GestureType parsed_gesture_type() const {
    CHECK(gesture_params_);
    return gesture_params_->GetGestureType();
  }

  const SyntheticPointerActionListParams& pointer_action_params() const {
    CHECK_EQ(parsed_gesture_type(),
             SyntheticGestureParams::POINTER_ACTION_LIST);
    return static_cast<const SyntheticPointerActionListParams&>(
        *gesture_params_.get());
  }

  const SyntheticSmoothScrollGestureParams& smooth_scroll_params() const {
    CHECK_EQ(parsed_gesture_type(),
             SyntheticGestureParams::SMOOTH_SCROLL_GESTURE);
    return static_cast<const SyntheticSmoothScrollGestureParams&>(
        *gesture_params_.get());
  }

 private:
  bool ActionsDictionaryUsesTestDriverApi(
      const base::Value::Dict& action_sequence);
  // For testdriver actions JSON format, please refer to Webdriver spec
  // https://www.w3.org/TR/webdriver/#actions.
  bool ParseTestDriverActionSequence(const base::Value::Dict& action_sequence);
  bool ParseGpuBenchmarkingActionSequence(
      const base::Value::Dict& action_sequence);
  bool ParseActionItemList(const base::Value::List& actions,
                           std::string source_type);
  bool ParseAction(const base::Value::Dict& action,
                   SyntheticPointerActionListParams::ParamList& param_list,
                   std::string source_type);
  bool ParsePointerParameters(const base::Value::Dict& action_sequence);
  bool ParseWheelAction(const base::Value::Dict& action, std::string subtype);
  bool ParsePointerAction(
      const base::Value::Dict& action,
      std::string subtype,
      SyntheticPointerActionListParams::ParamList& param_list);
  bool ParseNullAction(const base::Value::Dict& action,
                       std::string subtype,
                       SyntheticPointerActionListParams::ParamList& param_list);
  bool GetPosition(const base::Value::Dict& action,
                   double& position_x,
                   double& position_y);
  bool GetScrollDelta(const base::Value::Dict& action,
                      int& delta_x,
                      int& delta_y);
  bool GetPauseDuration(const base::Value::Dict& action, int& duration);

  std::unique_ptr<SyntheticGestureParams> gesture_params_;
  // This is a list of action sequence lists, which have all the actions for
  // all pointers.
  std::vector<SyntheticPointerActionListParams::ParamList>
      pointer_actions_lists_;
  size_t longest_action_sequence_ = 0;
  // This is the index of every input source action sequence in the actions
  // list.
  size_t action_index_ = 0;
  // This is the index of all the input source action sequences, whose type is
  // not "none".
  size_t input_source_count_ = 0;
  // size_t pointer_count_ = 0;
  // This is the input source type from testdriver Action API, such as "none",
  // "key", "pointer" or "wheel". With TestDriver, if the |source_type_| is
  // "pointer", we'll also set |pointer_type_| from the pointer parameters
  // dictionary.
  std::string source_type_;
  // This is the pointer type, such as "mouse", "touch" or "pen".
  // For TestDriver format, this will be empty for |source_type_| other than
  // "pointer".
  std::string pointer_type_;
  std::string error_message_;

  // This indicates if we want to parse the action sequence JSON string from
  // testdriver Action API or GpuBenchmarking::PointerActionSequence.
  bool use_testdriver_api_ = false;

  base::Value action_sequence_list_;
  std::set<std::string> pointer_name_set_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_ACTIONS_PARSER_H_
