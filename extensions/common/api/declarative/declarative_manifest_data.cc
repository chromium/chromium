// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative/declarative_manifest_data.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

class ErrorBuilder {
 public:
  explicit ErrorBuilder(base::string16* error) : error_(error) {}

  // Appends a literal string |error|.
  void Append(base::StringPiece error) {
    if (!error_->empty())
      error_->append(base::ASCIIToUTF16("; "));
    error_->append(base::UTF8ToUTF16(error));
  }

  // Appends a string |error| with the first %s replaced by |sub|.
  void Append(base::StringPiece error, base::StringPiece sub) {
    Append(base::StringPrintf(error.data(), sub.data()));
  }

 private:
  base::string16* const error_;
  DISALLOW_COPY_AND_ASSIGN(ErrorBuilder);
};

// Converts a rule defined in the manifest into a JSON internal format. The
// difference is that actions and conditions use a "type" key to define the
// type of rule/condition, while the internal format uses a "instanceType" key
// for this. This function walks through all the conditions and rules to swap
// the manifest key for the internal key.
bool ConvertManifestRule(const DeclarativeManifestData::Rule& rule,
                         ErrorBuilder* error_builder) {
  auto convert_list =
      [error_builder](const std::vector<std::unique_ptr<base::Value>>& list) {
        for (const std::unique_ptr<base::Value>& value : list) {
          base::DictionaryValue* dictionary = nullptr;
          if (!value->GetAsDictionary(&dictionary)) {
            error_builder->Append("expected dictionary, got %s",
                                  base::Value::GetTypeName(value->type()));
            return false;
          }
          std::string type;
          if (!dictionary->GetString("type", &type)) {
            error_builder->Append("'type' is required and must be a string");
            return false;
          }
          if (type == declarative_content_constants::kLegacyShowAction)
            type = declarative_content_constants::kShowAction;
          dictionary->Remove("type", nullptr);
          dictionary->SetString("instanceType", type);
        }
        return true;
      };
  return convert_list(rule.actions) && convert_list(rule.conditions);
}

}  // namespace

DeclarativeManifestData::DeclarativeManifestData() {
}

DeclarativeManifestData::~DeclarativeManifestData() {
}

// static
DeclarativeManifestData* DeclarativeManifestData::Get(
    const Extension* extension) {
  return static_cast<DeclarativeManifestData*>(
      extension->GetManifestData(manifest_keys::kEventRules));
}

// static
std::unique_ptr<DeclarativeManifestData> DeclarativeManifestData::FromValue(
    const base::Value& value,
    base::string16* error) {
  //  The following is an example of how an event programmatic rule definition
  //  translates to a manifest definition.
  //
  //  From javascript:
  //
  //  chrome.declarativeContent.onPageChanged.addRules([{
  //    actions: [
  //      new chrome.declarativeContent.ShowPageAction()
  //    ],
  //    conditions: [
  //      new chrome.declarativeContent.PageStateMatcher({css: ["video"]})
  //    ]
  //  }]);
  //
  //  In manifest:
  //
  //  "event_rules": [{
  //    "event" : "declarativeContent.onPageChanged",
  //    "actions" : [{
  //      "type": "declarativeContent.ShowPageAction"
  //    }],
  //    "conditions" : [{
  //      "css": ["video"],
  //      "type" : "declarativeContent.PageStateMatcher"
  //    }]
  //  }]
  //
  //  The javascript objects get translated into JSON objects with a "type"
  //  field to indicate the instance type. Instead of adding rules to a
  //  specific event list, each rule has an "event" field to indicate which
  //  event it applies to.
  //
  ErrorBuilder error_builder(error);
  std::unique_ptr<DeclarativeManifestData> result(
      new DeclarativeManifestData());
  const base::ListValue* list = nullptr;
  if (!value.GetAsList(&list)) {
    error_builder.Append("'event_rules' expected list, got %s",
                         base::Value::GetTypeName(value.type()));
    return std::unique_ptr<DeclarativeManifestData>();
  }

  for (const auto& element : *list) {
    const base::DictionaryValue* dict = nullptr;
    if (!element.GetAsDictionary(&dict)) {
      error_builder.Append("expected dictionary, got %s",
                           base::Value::GetTypeName(element.type()));
      return std::unique_ptr<DeclarativeManifestData>();
    }
    std::string event;
    if (!dict->GetString("event", &event)) {
      error_builder.Append("'event' is required");
      return std::unique_ptr<DeclarativeManifestData>();
    }

    Rule rule;
    if (!Rule::Populate(*dict, &rule)) {
      error_builder.Append("rule failed to populate");
      return std::unique_ptr<DeclarativeManifestData>();
    }

    if (!ConvertManifestRule(rule, &error_builder))
      return std::unique_ptr<DeclarativeManifestData>();

    result->event_rules_map_[event].push_back(std::move(rule));
  }
  return result;
}

std::vector<DeclarativeManifestData::Rule>
DeclarativeManifestData::RulesForEvent(const std::string& event) {
  const auto& rules = event_rules_map_[event];
  std::vector<DeclarativeManifestData::Rule> result;
  result.reserve(rules.size());
  for (const auto& rule : rules) {
    // TODO(rdevlin.cronin): It would be nice if we could have the RulesRegistry
    // reference the rules owned here, but the ownership issues are a bit
    // tricky. Revisit this.
    std::unique_ptr<base::DictionaryValue> rule_value = rule.ToValue();
    std::unique_ptr<DeclarativeManifestData::Rule> rule_copy =
        DeclarativeManifestData::Rule::FromValue(*rule_value);
    result.push_back(std::move(*rule_copy));
  }
  return result;
}

}  // namespace extensions
