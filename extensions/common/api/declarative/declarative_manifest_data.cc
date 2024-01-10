// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative/declarative_manifest_data.h"

#include <stddef.h>

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

class ErrorBuilder {
 public:
  explicit ErrorBuilder(std::u16string* error) : error_(error) {}

  ErrorBuilder(const ErrorBuilder&) = delete;
  ErrorBuilder& operator=(const ErrorBuilder&) = delete;

  // Appends a literal string |error|.
  void Append(std::string_view error) {
    if (!error_->empty())
      error_->append(u"; ");
    error_->append(base::UTF8ToUTF16(error));
  }

  // Appends a string |error| with the first %s replaced by |sub|.
  void Append(std::string_view error, std::string_view sub) {
    Append(base::StringPrintfNonConstexpr(error.data(), sub.data()));
  }

 private:
  const raw_ptr<std::u16string> error_;
};

// Converts a rule defined in the manifest into a JSON internal format. The
// difference is that actions and conditions use a "type" key to define the
// type of rule/condition, while the internal format uses a "instanceType" key
// for this. This function walks through all the conditions and rules to swap
// the manifest key for the internal key.
bool ConvertManifestRule(DeclarativeManifestData::Rule& rule,
                         ErrorBuilder* error_builder) {
  auto convert_list = [error_builder](base::Value::List& list) {
    for (base::Value& value : list) {
      base::Value::Dict* dictionary = value.GetIfDict();
      if (!dictionary) {
        error_builder->Append("expected dictionary, got %s",
                              base::Value::GetTypeName(value.type()));
        return false;
      }
      std::string* type = dictionary->FindString("type");
      if (!type) {
        error_builder->Append("'type' is required and must be a string");
        return false;
      }
      if (*type == declarative_content_constants::kLegacyShowAction) {
        dictionary->Set("instanceType",
                        declarative_content_constants::kShowAction);
      } else {
        dictionary->Set("instanceType", std::move(*type));
      }
      dictionary->Remove("type");
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
    std::u16string* error) {
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
  if (!value.is_list()) {
    error_builder.Append("'event_rules' expected list, got %s",
                         base::Value::GetTypeName(value.type()));
    return nullptr;
  }

  for (const auto& element : value.GetList()) {
    if (!element.is_dict()) {
      error_builder.Append("expected dictionary, got %s",
                           base::Value::GetTypeName(element.type()));
      return nullptr;
    }
    const base::Value::Dict& dict = element.GetDict();
    const std::string* event = dict.FindString("event");
    if (!event) {
      error_builder.Append("'event' is required");
      return nullptr;
    }

    auto rule = Rule::FromValue(dict);
    if (!rule) {
      error_builder.Append("rule failed to populate");
      return nullptr;
    }

    if (!ConvertManifestRule(*rule, &error_builder)) {
      return nullptr;
    }

    result->event_rules_map_[*event].push_back(std::move(rule).value());
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
    result.push_back(rule.Clone());
  }
  return result;
}

}  // namespace extensions
