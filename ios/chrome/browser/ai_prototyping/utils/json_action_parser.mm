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
};

// Based on the field names in
// components/optimization_guide/proto/features/actions_data.proto.
ActionType GetActionType(const std::string& key) {
  if (key == "navigate") {
    return ActionType::kNavigate;
  }
  return ActionType::kUnknown;
}

void MapNavigateAction(const base::DictValue& dict,
                       optimization_guide::proto::Action* action) {
  auto* navigate = action->mutable_navigate();
  if (const std::string* url = dict.FindString("url")) {
    navigate->set_url(*url);
  }
  if (std::optional<int> tab_id = dict.FindInt("tab_id")) {
    navigate->set_tab_id(*tab_id);
  }
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
      MapNavigateAction(value.GetDict(), action);
      return true;
    case ActionType::kUnknown:
      return false;
  }
}

}  // namespace ai_prototyping
