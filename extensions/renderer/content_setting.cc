// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/content_setting.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "v8/include/v8-object.h"

namespace extensions {

namespace {

// Content settings that are deprecated.
const char* const kDeprecatedTypesToAllow[] = {
    "fullscreen",
    "mouselock",
};
const char* const kDeprecatedTypesToBlock[] = {
    "plugins",
    "ppapi-broker",
};

const char* GetForcedValueForDeprecatedSetting(std::string_view type) {
  if (base::Contains(kDeprecatedTypesToAllow, type))
    return "allow";
  DCHECK(base::Contains(kDeprecatedTypesToBlock, type));
  return "block";
}

bool IsDeprecated(std::string_view type) {
  return base::Contains(kDeprecatedTypesToAllow, type) ||
         base::Contains(kDeprecatedTypesToBlock, type);
}

}  // namespace

v8::Local<v8::Object> ContentSetting::Create(
    v8::Isolate* isolate,
    const std::string& property_name,
    const base::Value::List* property_values,
    APIRequestHandler* request_handler,
    APIEventHandler* event_handler,
    APITypeReferenceMap* type_refs,
    const BindingAccessChecker* access_checker) {
  CHECK_GE(property_values->size(), 2u);
  CHECK((*property_values)[1u].is_dict());
  const std::string& pref_name = (*property_values)[0].GetString();
  const base::Value::Dict& value_spec = (*property_values)[1u].GetDict();

  gin::Handle<ContentSetting> handle = gin::CreateHandle(
      isolate, new ContentSetting(request_handler, type_refs, access_checker,
                                  pref_name, value_spec));
  return handle.ToV8().As<v8::Object>();
}

ContentSetting::ContentSetting(APIRequestHandler* request_handler,
                               const APITypeReferenceMap* type_refs,
                               const BindingAccessChecker* access_checker,
                               const std::string& pref_name,
                               const base::Value::Dict& set_value_spec)
    : request_handler_(request_handler),
      type_refs_(type_refs),
      access_checker_(access_checker),
      pref_name_(pref_name),
      argument_spec_(ArgumentType::OBJECT) {
  // The set() call takes an object { setting: { type: <t> }, ... }, where <t>
  // is the custom set() argument specified above by value_spec.
  ArgumentSpec::PropertiesMap properties;
  properties["primaryPattern"] =
      std::make_unique<ArgumentSpec>(ArgumentType::STRING);
  {
    auto secondary_pattern_spec =
        std::make_unique<ArgumentSpec>(ArgumentType::STRING);
    secondary_pattern_spec->set_optional(true);
    properties["secondaryPattern"] = std::move(secondary_pattern_spec);
  }
  {
    auto resource_identifier_spec =
        std::make_unique<ArgumentSpec>(ArgumentType::REF);
    resource_identifier_spec->set_ref("contentSettings.ResourceIdentifier");
    resource_identifier_spec->set_optional(true);
    properties["resourceIdentifier"] = std::move(resource_identifier_spec);
  }
  {
    auto scope_spec = std::make_unique<ArgumentSpec>(ArgumentType::REF);
    scope_spec->set_ref("contentSettings.Scope");
    scope_spec->set_optional(true);
    properties["scope"] = std::move(scope_spec);
  }
  properties["setting"] = std::make_unique<ArgumentSpec>(set_value_spec);
  argument_spec_.set_properties(std::move(properties));
}

ContentSetting::~ContentSetting() = default;

gin::WrapperInfo ContentSetting::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder ContentSetting::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<ContentSetting>::GetObjectTemplateBuilder(isolate)
      .SetMethod("get", &ContentSetting::Get)
      .SetMethod("set", &ContentSetting::Set)
      .SetMethod("clear", &ContentSetting::Clear)
      .SetMethod("getResourceIdentifiers",
                 &ContentSetting::GetResourceIdentifiers);
}

const char* ContentSetting::GetTypeName() {
  return "ContentSetting";
}

void ContentSetting::Get(gin::Arguments* arguments) {
  HandleFunction("get", arguments);
}

void ContentSetting::Set(gin::Arguments* arguments) {
  HandleFunction("set", arguments);
}

void ContentSetting::Clear(gin::Arguments* arguments) {
  HandleFunction("clear", arguments);
}

void ContentSetting::GetResourceIdentifiers(gin::Arguments* arguments) {
  HandleFunction("getResourceIdentifiers", arguments);
}

void ContentSetting::HandleFunction(const std::string& method_name,
                                    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  if (!binding::IsContextValidOrThrowError(context))
    return;

  v8::LocalVector<v8::Value> argument_list = arguments->GetAll();

  std::string full_name = "contentSettings.ContentSetting." + method_name;

  if (!access_checker_->HasAccessOrThrowError(context, full_name))
    return;

  const APISignature* signature = type_refs_->GetTypeMethodSignature(full_name);
  APISignature::JSONParseResult parse_result =
      signature->ParseArgumentsToJSON(context, argument_list, *type_refs_);
  if (!parse_result.succeeded()) {
    arguments->ThrowTypeError(api_errors::InvocationError(
        full_name, signature->GetExpectedSignature(), *parse_result.error));
    return;
  }

  if (IsDeprecated(pref_name_)) {
    console::AddMessage(ScriptContextSet::GetContextByV8Context(context),
                        blink::mojom::ConsoleMessageLevel::kWarning,
                        base::StringPrintf("contentSettings.%s is deprecated.",
                                           pref_name_.c_str()));
    // If a callback was provided, call it immediately.
    if (!parse_result.callback.IsEmpty()) {
      v8::LocalVector<v8::Value> args(isolate);
      if (method_name == "get") {
        // Populate the result to avoid breaking extensions.
        v8::Local<v8::Object> object = v8::Object::New(isolate);
        v8::Maybe<bool> result = object->CreateDataProperty(
            context, gin::StringToSymbol(isolate, "setting"),
            gin::StringToSymbol(
                isolate, GetForcedValueForDeprecatedSetting(pref_name_)));
        // Since we just defined this object, CreateDataProperty() should never
        // fail.
        CHECK(result.ToChecked());
        args.push_back(object);
      }
      JSRunner::Get(context)->RunJSFunction(parse_result.callback, context,
                                            args.size(), args.data());
    }
    return;
  }

  if (method_name == "set") {
    v8::Local<v8::Value> value = argument_list[0];
    // The set schema included in the Schema object is generic, since it varies
    // per-setting. However, this is only ever for a single setting, so we can
    // enforce the types more thoroughly.
    // Note: we do this *after* checking if the setting is deprecated, since
    // this validation will fail for deprecated settings.
    std::string error;
    if (!value.IsEmpty() &&
        !argument_spec_.ParseArgument(context, value, *type_refs_, nullptr,
                                      nullptr, &error)) {
      arguments->ThrowTypeError("Invalid invocation: " + error);
      return;
    }
  }

  parse_result.arguments_list->Insert(parse_result.arguments_list->begin(),
                                      base::Value(pref_name_));

  v8::Local<v8::Promise> promise = request_handler_->StartRequest(
      context, "contentSettings." + method_name,
      std::move(*parse_result.arguments_list), parse_result.async_type,
      parse_result.callback, v8::Local<v8::Function>(),
      binding::ResultModifierFunction());
  if (!promise.IsEmpty())
    arguments->Return(promise);
}

}  // namespace extensions
