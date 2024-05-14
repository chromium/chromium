// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/chrome_setting.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "v8/include/v8-object.h"

namespace extensions {

v8::Local<v8::Object> ChromeSetting::Create(
    v8::Isolate* isolate,
    const std::string& property_name,
    const base::Value::List* property_values,
    APIRequestHandler* request_handler,
    APIEventHandler* event_handler,
    APITypeReferenceMap* type_refs,
    const BindingAccessChecker* access_checker) {
  CHECK_GE(property_values->size(), 2u);
  CHECK((*property_values)[1u].is_dict());
  const std::string& pref_name = (*property_values)[0u].GetString();
  const base::Value::Dict& value_spec = (*property_values)[1u].GetDict();

  gin::Handle<ChromeSetting> handle = gin::CreateHandle(
      isolate, new ChromeSetting(request_handler, event_handler, type_refs,
                                 access_checker, pref_name, value_spec));
  return handle.ToV8().As<v8::Object>();
}

ChromeSetting::ChromeSetting(APIRequestHandler* request_handler,
                             APIEventHandler* event_handler,
                             const APITypeReferenceMap* type_refs,
                             const BindingAccessChecker* access_checker,
                             const std::string& pref_name,
                             const base::Value::Dict& set_value_spec)
    : request_handler_(request_handler),
      event_handler_(event_handler),
      type_refs_(type_refs),
      access_checker_(access_checker),
      pref_name_(pref_name),
      argument_spec_(ArgumentType::OBJECT) {
  // The set() call takes an object { value: { type: <t> }, ... }, where <t>
  // is the custom set() argument specified above by value_spec.
  ArgumentSpec::PropertiesMap properties;
  {
    auto scope_spec = std::make_unique<ArgumentSpec>(ArgumentType::REF);
    scope_spec->set_ref("types.ChromeSettingScope");
    scope_spec->set_optional(true);
    properties["scope"] = std::move(scope_spec);
  }
  properties["value"] = std::make_unique<ArgumentSpec>(set_value_spec);
  argument_spec_.set_properties(std::move(properties));
}

ChromeSetting::~ChromeSetting() = default;

gin::WrapperInfo ChromeSetting::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder ChromeSetting::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<ChromeSetting>::GetObjectTemplateBuilder(isolate)
      .SetMethod("get", &ChromeSetting::Get)
      .SetMethod("set", &ChromeSetting::Set)
      .SetMethod("clear", &ChromeSetting::Clear)
      .SetProperty("onChange", &ChromeSetting::GetOnChangeEvent);
}

const char* ChromeSetting::GetTypeName() {
  return "ChromeSetting";
}

void ChromeSetting::Get(gin::Arguments* arguments) {
  HandleFunction("get", arguments);
}

void ChromeSetting::Set(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  if (!binding::IsContextValidOrThrowError(context))
    return;

  v8::Local<v8::Value> value = arguments->PeekNext();
  // The set schema included in the Schema object is generic, since it varies
  // per-setting. However, this is only ever for a single setting, so we can
  // enforce the types more thoroughly.
  std::string error;
  if (!value.IsEmpty() &&
      !argument_spec_.ParseArgument(context, value, *type_refs_, nullptr,
                                    nullptr, &error)) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }
  HandleFunction("set", arguments);
}

void ChromeSetting::Clear(gin::Arguments* arguments) {
  HandleFunction("clear", arguments);
}

v8::Local<v8::Value> ChromeSetting::GetOnChangeEvent(
    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();

  if (!binding::IsContextValidOrThrowError(context))
    return v8::Undefined(isolate);

  v8::Local<v8::Private> key = v8::Private::ForApi(
      isolate, gin::StringToSymbol(isolate, "onChangeEvent"));
  v8::Local<v8::Value> event;
  if (!wrapper->GetPrivate(context, key).ToLocal(&event)) {
    NOTREACHED_IN_MIGRATION();
    return v8::Local<v8::Value>();
  }

  DCHECK(!event.IsEmpty());
  if (event->IsUndefined()) {
    bool supports_filters = false;
    bool supports_lazy_listeners = true;
    event = event_handler_->CreateEventInstance(
        base::StringPrintf("types.ChromeSetting.%s.onChange",
                           pref_name_.c_str()),
        supports_filters, supports_lazy_listeners, binding::kNoListenerMax,
        true, context);
    v8::Maybe<bool> set_result = wrapper->SetPrivate(context, key, event);
    if (!set_result.IsJust() || !set_result.FromJust()) {
      NOTREACHED_IN_MIGRATION();
      return v8::Local<v8::Value>();
    }
  }
  return event;
}

void ChromeSetting::HandleFunction(const std::string& method_name,
                                   gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  if (!binding::IsContextValidOrThrowError(context))
    return;

  v8::LocalVector<v8::Value> argument_list = arguments->GetAll();

  std::string full_name = "types.ChromeSetting." + method_name;

  if (!access_checker_->HasAccessOrThrowError(context, full_name))
    return;

  std::string error;
  const APISignature* signature = type_refs_->GetTypeMethodSignature(full_name);
  APISignature::JSONParseResult parse_result =
      signature->ParseArgumentsToJSON(context, argument_list, *type_refs_);
  if (!parse_result.succeeded()) {
    arguments->ThrowTypeError(api_errors::InvocationError(
        full_name, signature->GetExpectedSignature(), *parse_result.error));
    return;
  }

  parse_result.arguments_list->Insert(parse_result.arguments_list->begin(),
                                      base::Value(pref_name_));

  v8::Local<v8::Promise> promise = request_handler_->StartRequest(
      context, full_name, std::move(*parse_result.arguments_list),
      parse_result.async_type, parse_result.callback, v8::Local<v8::Function>(),
      binding::ResultModifierFunction());
  if (!promise.IsEmpty())
    arguments->Return(promise);
}

}  // namespace extensions
