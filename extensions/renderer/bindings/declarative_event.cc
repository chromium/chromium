// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/declarative_event.h"

#include <algorithm>
#include <memory>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

// Builds an ArgumentSpec that accepts the given |choices| as references.
std::unique_ptr<ArgumentSpec> BuildChoicesSpec(
    const std::vector<std::string>& choices_list) {
  auto item_type = std::make_unique<ArgumentSpec>(ArgumentType::CHOICES);
  std::vector<std::unique_ptr<ArgumentSpec>> choices;
  choices.reserve(choices_list.size());
  for (const std::string& value : choices_list) {
    auto choice = std::make_unique<ArgumentSpec>(ArgumentType::REF);
    choice->set_ref(value);
    choices.push_back(std::move(choice));
  }
  item_type->set_choices(std::move(choices));
  return item_type;
}

// Builds the ArgumentSpec for a events.Rule type, given a list of actions and
// conditions. It's insufficient to use the specification in events.Rule, since
// that provides argument types of "any" for actions and conditions, allowing
// individual APIs to specify them further. Alternatively, we could lookup the
// events.Rule spec and only override the actions and conditions properties,
// but that doesn't seem any less contrived and requires JSON parsing and
// complex spec initialization.
// TODO(devlin): Another target for generating these specs. Currently, the
// custom JS bindings do something similar, so this is no worse off, but that
// doesn't make it more desirable.
std::unique_ptr<ArgumentSpec> BuildRulesSpec(
    const std::vector<std::string>& actions_list,
    const std::vector<std::string>& conditions_list) {
  auto rule_spec = std::make_unique<ArgumentSpec>(ArgumentType::OBJECT);
  ArgumentSpec::PropertiesMap properties;
  {
    auto id_spec = std::make_unique<ArgumentSpec>(ArgumentType::STRING);
    id_spec->set_optional(true);
    properties["id"] = std::move(id_spec);
  }
  {
    auto tags_spec = std::make_unique<ArgumentSpec>(ArgumentType::LIST);
    tags_spec->set_list_element_type(
        std::make_unique<ArgumentSpec>(ArgumentType::STRING));
    tags_spec->set_optional(true);
    properties["tags"] = std::move(tags_spec);
  }
  {
    auto actions_spec = std::make_unique<ArgumentSpec>(ArgumentType::LIST);
    actions_spec->set_list_element_type(BuildChoicesSpec(actions_list));
    properties["actions"] = std::move(actions_spec);
  }
  {
    auto conditions_spec = std::make_unique<ArgumentSpec>(ArgumentType::LIST);
    conditions_spec->set_list_element_type(BuildChoicesSpec(conditions_list));
    properties["conditions"] = std::move(conditions_spec);
  }
  {
    auto priority_spec = std::make_unique<ArgumentSpec>(ArgumentType::INTEGER);
    priority_spec->set_optional(true);
    properties["priority"] = std::move(priority_spec);
  }
  rule_spec->set_properties(std::move(properties));
  return rule_spec;
}

// Builds the signature for events.addRules using a specific rule.
std::unique_ptr<APISignature> BuildAddRulesSignature(
    const std::string& rule_name) {
  std::vector<std::unique_ptr<ArgumentSpec>> params;
  params.push_back(std::make_unique<ArgumentSpec>(ArgumentType::STRING));
  params.push_back(std::make_unique<ArgumentSpec>(ArgumentType::INTEGER));
  {
    auto rules = std::make_unique<ArgumentSpec>(ArgumentType::LIST);
    auto ref = std::make_unique<ArgumentSpec>(ArgumentType::REF);
    ref->set_ref(rule_name);
    rules->set_list_element_type(std::move(ref));
    params.push_back(std::move(rules));
  }
  {
    auto callback = std::make_unique<ArgumentSpec>(ArgumentType::FUNCTION);
    callback->set_optional(true);
    params.push_back(std::move(callback));
  }

  return std::make_unique<APISignature>(std::move(params));
}

}  // namespace

gin::WrapperInfo DeclarativeEvent::kWrapperInfo = {gin::kEmbedderNativeGin};

DeclarativeEvent::DeclarativeEvent(
    const std::string& name,
    APITypeReferenceMap* type_refs,
    APIRequestHandler* request_handler,
    const std::vector<std::string>& actions_list,
    const std::vector<std::string>& conditions_list,
    int webview_instance_id)
    : event_name_(name),
      type_refs_(type_refs),
      request_handler_(request_handler),
      webview_instance_id_(webview_instance_id) {
  // In declarative events, the specification of the rules can change. This only
  // matters for the events.addRules function. Check whether or not a
  // specialized version for this event exists, and, if not, create it.
  std::string add_rules_name = name + ".addRules";
  if (!type_refs->HasTypeMethodSignature(add_rules_name)) {
    // Create the specific rules spec and cache it under this type. This will
    // result in e.g. declarativeContent.onPageChanged.Rule, since the Rule
    // schema is only used for this event.
    std::unique_ptr<ArgumentSpec> rules_spec =
        BuildRulesSpec(actions_list, conditions_list);
    std::string rule_type_name = name + ".Rule";
    type_refs->AddSpec(rule_type_name, std::move(rules_spec));
    // Build a custom signature for the method, since this would be different
    // than adding rules for a different event.
    std::unique_ptr<APISignature> rules_signature =
        BuildAddRulesSignature(rule_type_name);
    type_refs->AddTypeMethodSignature(add_rules_name,
                                      std::move(rules_signature));
  }
}

DeclarativeEvent::~DeclarativeEvent() {}

gin::ObjectTemplateBuilder DeclarativeEvent::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<DeclarativeEvent>::GetObjectTemplateBuilder(isolate)
      .SetMethod("addRules", &DeclarativeEvent::AddRules)
      .SetMethod("removeRules", &DeclarativeEvent::RemoveRules)
      .SetMethod("getRules", &DeclarativeEvent::GetRules);
}

const char* DeclarativeEvent::GetTypeName() {
  // NOTE(devlin): Currently, our documentation does not differentiate between
  // "normal" events and declarative events. Use "Event" here so that developers
  // don't think there's separate documentation to look for.
  return "Event";
}

void DeclarativeEvent::AddRules(gin::Arguments* arguments) {
  // When adding rules, we use the signature we built for this event (e.g.
  // declarativeContent.onPageChanged.addRules).
  HandleFunction(event_name_ + ".addRules", "events.addRules", arguments);
}

void DeclarativeEvent::RemoveRules(gin::Arguments* arguments) {
  // The signatures for removeRules are always the same (they don't use the
  // event's Rule schema).
  HandleFunction("events.Event.removeRules", "events.removeRules", arguments);
}

void DeclarativeEvent::GetRules(gin::Arguments* arguments) {
  // The signatures for getRules are always the same (they don't use the
  // event's Rule schema).
  HandleFunction("events.Event.getRules", "events.getRules", arguments);
}

void DeclarativeEvent::HandleFunction(const std::string& signature_name,
                                      const std::string& request_name,
                                      gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  std::vector<v8::Local<v8::Value>> argument_list = arguments->GetAll();

  // The events API has two undocumented parameters for each function: the name
  // of the event, and the "webViewInstanceId". Currently, stub 0 for webview
  // instance id.
  argument_list.insert(argument_list.begin(),
                       {gin::StringToSymbol(isolate, event_name_),
                        v8::Integer::New(isolate, webview_instance_id_)});

  const APISignature* signature =
      type_refs_->GetTypeMethodSignature(signature_name);
  DCHECK(signature);
  APISignature::JSONParseResult parse_result =
      signature->ParseArgumentsToJSON(context, argument_list, *type_refs_);
  if (!parse_result.succeeded()) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  request_handler_->StartRequest(
      context, request_name, std::move(parse_result.arguments),
      parse_result.callback, v8::Local<v8::Function>());
}

}  // namespace extensions
