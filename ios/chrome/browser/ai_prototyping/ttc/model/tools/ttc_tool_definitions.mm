// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import <utility>
#import <vector>

#import "base/values.h"

namespace {

// JSON schema keys and type strings, sorted alphabetically.
constexpr char kBooleanType[] = "boolean";
constexpr char kDescriptionKey[] = "description";
constexpr char kDirectionKey[] = "direction";
constexpr char kEnumKey[] = "enum";
constexpr char kGranularityKey[] = "granularity";
constexpr char kIntegerType[] = "integer";
constexpr char kNewTabKey[] = "new_tab";
constexpr char kNodeIdKey[] = "node_id";
constexpr char kObjectType[] = "object";
constexpr char kPropertiesKey[] = "properties";
constexpr char kRequiredKey[] = "required";
constexpr char kStringType[] = "string";
constexpr char kTextKey[] = "text";
constexpr char kTypeKey[] = "type";
constexpr char kUrlKey[] = "url";
constexpr char kValueKey[] = "value";

}  // namespace

namespace ttc {

// Note: All tool descriptions below are schema definitions passed to the
// backend for function calling, not user-facing UI strings.
// User-visible UI elements displaying tool activity must use l10n_util.
std::vector<ToolDefinition> GetDefaultToolDefinitions() {
  std::vector<ToolDefinition> tools;

  ToolDefinition click_element;
  click_element.name = kToolClickElement;
  click_element.description =
      "Clicks an interactive element on the current page.";
  click_element.parameters_json_schema =
      base::DictValue()
          .Set(kTypeKey, kObjectType)
          .Set(kPropertiesKey,
               base::DictValue().Set(
                   kNodeIdKey,
                   base::DictValue()
                       .Set(kTypeKey, kIntegerType)
                       .Set(kDescriptionKey,
                            "The DOM node ID of the element to click.")))
          .Set(kRequiredKey, base::ListValue().Append(kNodeIdKey));
  click_element.behavior = ToolDefinition::Behavior::kBlocking;
  click_element.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(click_element));
  ToolDefinition close_current_tab;
  close_current_tab.name = kToolCloseCurrentTab;
  close_current_tab.description = "Closes the currently active tab.";
  // The tool takes no arguments, so its schema is left empty.
  close_current_tab.behavior = ToolDefinition::Behavior::kBlocking;
  close_current_tab.verbalization =
      ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(close_current_tab));

  ToolDefinition close_session;
  close_session.name = kToolCloseSession;
  close_session.description = "Ends and closes the active session.";
  // The tool takes no arguments, so its schema is left empty.
  close_session.behavior = ToolDefinition::Behavior::kBlocking;
  close_session.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(close_session));

  ToolDefinition create_tab;
  create_tab.name = kToolCreateTab;
  create_tab.description = "Creates and opens a new tab in the browser.";
  // The tool takes no arguments, so its schema is left empty.
  create_tab.behavior = ToolDefinition::Behavior::kBlocking;
  create_tab.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(create_tab));

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

  ToolDefinition scroll_page;
  scroll_page.name = kToolScrollPage;
  scroll_page.description =
      "Scrolls the current page or a scrollable element in the specified "
      "direction.";
  scroll_page.parameters_json_schema =
      base::DictValue()
          .Set(kTypeKey, kObjectType)
          .Set(kPropertiesKey,
               base::DictValue()
                   .Set(kGranularityKey,
                        base::DictValue()
                            .Set(kTypeKey, kStringType)
                            .Set(kEnumKey,
                                 base::ListValue().Append("page").Append(
                                     "document"))
                            .Set(kDescriptionKey,
                                 "Scroll distance granularity: \"page\" or "
                                 "\"document\"."))
                   .Set(kDirectionKey,
                        base::DictValue()
                            .Set(kTypeKey, kStringType)
                            .Set(kEnumKey, base::ListValue()
                                               .Append("up")
                                               .Append("down")
                                               .Append("left")
                                               .Append("right"))
                            .Set(kDescriptionKey,
                                 "The direction to scroll: \"up\", \"down\", "
                                 "\"left\", or \"right\"."))
                   .Set(kNodeIdKey,
                        base::DictValue()
                            .Set(kTypeKey, kIntegerType)
                            .Set(kDescriptionKey,
                                 "The DOM node ID of the scrollable element, "
                                 "or 0 to scroll the main viewport.")))
          .Set(kRequiredKey, base::ListValue()
                                 .Append(kGranularityKey)
                                 .Append(kDirectionKey)
                                 .Append(kNodeIdKey));
  scroll_page.behavior = ToolDefinition::Behavior::kBlocking;
  scroll_page.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(scroll_page));

  ToolDefinition select_option;
  select_option.name = kToolSelectOption;
  select_option.description =
      "Selects an option in a dropdown or select element on the current page.";
  select_option.parameters_json_schema =
      base::DictValue()
          .Set(kTypeKey, kObjectType)
          .Set(kPropertiesKey,
               base::DictValue()
                   .Set(kNodeIdKey,
                        base::DictValue()
                            .Set(kTypeKey, kIntegerType)
                            .Set(kDescriptionKey,
                                 "The DOM node ID of the select element."))
                   .Set(kValueKey,
                        base::DictValue()
                            .Set(kTypeKey, kStringType)
                            .Set(kDescriptionKey,
                                 "The value of the option to select.")))
          .Set(kRequiredKey,
               base::ListValue().Append(kNodeIdKey).Append(kValueKey));
  select_option.behavior = ToolDefinition::Behavior::kBlocking;
  select_option.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(select_option));

  ToolDefinition set_text;
  set_text.name = kToolSetText;
  set_text.description =
      "Enters text into an input or editable element on the current page.";
  set_text.parameters_json_schema =
      base::DictValue()
          .Set(kTypeKey, kObjectType)
          .Set(kPropertiesKey,
               base::DictValue()
                   .Set(kNodeIdKey,
                        base::DictValue()
                            .Set(kTypeKey, kIntegerType)
                            .Set(kDescriptionKey,
                                 "The DOM node ID of the input element."))
                   .Set(kTextKey,
                        base::DictValue()
                            .Set(kTypeKey, kStringType)
                            .Set(kDescriptionKey,
                                 "The text to enter into the element.")))
          .Set(kRequiredKey,
               base::ListValue().Append(kNodeIdKey).Append(kTextKey));
  set_text.behavior = ToolDefinition::Behavior::kBlocking;
  set_text.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(set_text));

  return tools;
}

}  // namespace ttc
