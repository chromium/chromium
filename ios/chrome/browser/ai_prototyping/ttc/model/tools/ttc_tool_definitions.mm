// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import <utility>
#import <vector>

#import "base/values.h"

namespace {

// JSON schema keys and type strings.
constexpr char kDescriptionKey[] = "description";
constexpr char kTypeKey[] = "type";
constexpr char kPropertiesKey[] = "properties";
constexpr char kRequiredKey[] = "required";
constexpr char kUrlKey[] = "url";
constexpr char kNewTabKey[] = "new_tab";
constexpr char kObjectType[] = "object";
constexpr char kStringType[] = "string";
constexpr char kBooleanType[] = "boolean";

}  // namespace

namespace ttc {

// Note: All tool descriptions below are schema definitions passed to the
// backend for function calling, not user-facing UI strings.
// User-visible UI elements displaying tool activity must use l10n_util.
std::vector<ToolDefinition> GetDefaultToolDefinitions() {
  std::vector<ToolDefinition> tools;

  ToolDefinition go_back;
  go_back.name = kToolGoBack;
  go_back.description = "Go back to the previous page in history.";
  // The tool takes no arguments, so its schema is left empty.
  go_back.behavior = ToolDefinition::Behavior::kBlocking;
  go_back.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(go_back));

  ToolDefinition go_forward;
  go_forward.name = kToolGoForward;
  go_forward.description = "Go forward to the next page in history.";
  // The tool takes no arguments, so its schema is left empty.
  go_forward.behavior = ToolDefinition::Behavior::kBlocking;
  go_forward.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(go_forward));

  ToolDefinition open_url;
  open_url.name = kToolOpenUrl;
  open_url.description = "Opens a URL in the browser.";
  open_url.parameters_json_schema =
      base::DictValue()
          .Set(kTypeKey, kObjectType)
          .Set(kPropertiesKey,
               base::DictValue()
                   .Set(kUrlKey, base::DictValue()
                                     .Set(kTypeKey, kStringType)
                                     .Set(kDescriptionKey,
                                          "The complete URL to open (e.g. "
                                          "\"https://example.com\")."))
                   .Set(kNewTabKey,
                        base::DictValue()
                            .Set(kTypeKey, kBooleanType)
                            .Set(kDescriptionKey,
                                 "If true, opens the URL in a new tab; "
                                 "otherwise, navigates the current tab.")))
          .Set(kRequiredKey,
               base::ListValue().Append(kUrlKey).Append(kNewTabKey));
  open_url.behavior = ToolDefinition::Behavior::kBlocking;
  open_url.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(open_url));

  return tools;
}

}  // namespace ttc
