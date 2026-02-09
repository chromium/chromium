// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_JSON_ACTION_PARSER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_JSON_ACTION_PARSER_H_

#include "base/values.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace ai_prototyping {

// Parses a JSON dictionary representing a optimization_guide::proto::Action
// and populates the proto.
// `dict`: The dictionary containing the action data (e.g.,
//         {"navigate": {"url": "https://www.google.com", "tab_id": 123}} ).
// `action`: The proto to populate.
// Returns true if parsing was successful, false otherwise.
bool ParseActionFromDict(const base::DictValue& dict,
                         optimization_guide::proto::Action* action);

}  // namespace ai_prototyping

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_JSON_ACTION_PARSER_H_
