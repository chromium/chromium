// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/declarative_content_hooks_delegate.h"

#include <string_view>

#include "base/functional/bind.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_selector.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-value.h"

namespace extensions {

namespace {

void RunDeclarativeContentHooksDelegateHandlerCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Data()->IsExternal());
  v8::Local<v8::External> external = info.Data().As<v8::External>();
  auto* callback =
      static_cast<DeclarativeContentHooksDelegate::HandlerCallback*>(
          external->Value());
  callback->Run(info);
}

// Copies the 'own' properties from src -> dst.
bool V8Assign(v8::Local<v8::Context> context,
              v8::Local<v8::Object> src,
              v8::Local<v8::Object> dst) {
  v8::Local<v8::Array> own_property_names;
  if (!src->GetOwnPropertyNames(context).ToLocal(&own_property_names))
    return false;

  uint32_t length = own_property_names->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> key;
    if (!own_property_names->Get(context, i).ToLocal(&key))
      return false;
    DCHECK(key->IsString() || key->IsUint32());

    v8::Local<v8::Value> prop_value;
    if (!src->Get(context, key).ToLocal(&prop_value))
      return false;

    v8::Maybe<bool> success =
        key->IsString()
            ? dst->CreateDataProperty(context, key.As<v8::String>(), prop_value)
            : dst->CreateDataProperty(context, key.As<v8::Uint32>()->Value(),
                                      prop_value);
    if (!success.IsJust() || !success.FromJust())
      return false;
  }

  return true;
}

// Canonicalizes any css selectors specified in a page state matcher, returning
// true on success.
bool CanonicalizeCssSelectors(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> object,
                              std::string* error) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> key =
      gin::StringToSymbol(isolate, declarative_content_constants::kCss);
  v8::Maybe<bool> has_css = object->HasOwnProperty(context, key);
  // Note: don't bother populating |error| if script threw an exception.
  if (!has_css.IsJust())
    return false;

  if (!has_css.FromJust())
    return true;

  v8::Local<v8::Value> css;
  if (!object->Get(context, key).ToLocal(&css))
    return false;

  if (css->IsUndefined())
    return true;

  if (!css->IsArray())
    return false;

  v8::Local<v8::Array> css_array = css.As<v8::Array>();
  uint32_t length = css_array->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> val;
    if (!css_array->Get(context, i).ToLocal(&val) || !val->IsString())
      return false;
    v8::String::Utf8Value selector(isolate, val.As<v8::String>());
    // Note: See the TODO in css_natives_handler.cc.
    std::string parsed =
        blink::CanonicalizeSelector(blink::WebString::FromUTF8(std::string_view(
                                        *selector, selector.length())),
                                    blink::kWebSelectorTypeCompound)
            .Utf8();
    if (parsed.empty()) {
      *error =
          "Invalid CSS selector: " + std::string(*selector, selector.length());
      return false;
    }
    v8::Maybe<bool> set_result =
        css_array->Set(context, i, gin::StringToSymbol(isolate, parsed));
    if (!set_result.IsJust() || !set_result.FromJust())
      return false;
  }

  return true;
}

// Validates the source object against the expected spec, and copies over values
// to |this_object|. Returns true on success.
bool Validate(const ArgumentSpec* spec,
              const APITypeReferenceMap& type_refs,
              v8::Local<v8::Context> context,
              v8::Local<v8::Object> this_object,
              v8::Local<v8::Object> source_object,
              const std::string& type_name,
              std::string* error) {
  if (!source_object.IsEmpty() &&
      !V8Assign(context, source_object, this_object)) {
    return false;
  }

  v8::Isolate* isolate = context->GetIsolate();
  v8::Maybe<bool> set_result = this_object->CreateDataProperty(
      context,
      gin::StringToSymbol(isolate,
                          declarative_content_constants::kInstanceType),
      gin::StringToSymbol(isolate, type_name));
  if (!set_result.IsJust() || !set_result.FromJust()) {
    return false;
  }

  if (!spec->ParseArgument(context, this_object, type_refs, nullptr, nullptr,
                           error)) {
    return false;
  }

  if (type_name == declarative_content_constants::kPageStateMatcherType &&
      !CanonicalizeCssSelectors(context, this_object, error)) {
    return false;
  }
  return true;
}

}  // namespace

DeclarativeContentHooksDelegate::DeclarativeContentHooksDelegate() = default;
DeclarativeContentHooksDelegate::~DeclarativeContentHooksDelegate() = default;

void DeclarativeContentHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  // Add constructors for the API types.
  // TODO(devlin): We'll need to extract out common logic here and share it with
  // declarativeWebRequest.
  struct {
    const char* full_name;
    const char* exposed_name;
  } kTypes[] = {
      {declarative_content_constants::kPageStateMatcherType,
       "PageStateMatcher"},
      {declarative_content_constants::kShowAction, "ShowAction"},
      {declarative_content_constants::kShowAction, "ShowPageAction"},
      {declarative_content_constants::kSetIcon, "SetIcon"},
      {declarative_content_constants::kRequestContentScript,
       "RequestContentScript"},
  };
  callbacks_.reserve(std::size(kTypes));
  for (const auto& type : kTypes) {
    const ArgumentSpec* spec = type_refs.GetSpec(type.full_name);
    DCHECK(spec);
    // This object should outlive any calls to the function, so this
    // base::Unretained and the callback itself are safe. Similarly, the same
    // bindings system owns all these objects, so the spec and type refs should
    // also be safe.
    callbacks_.push_back(std::make_unique<HandlerCallback>(base::BindRepeating(
        &DeclarativeContentHooksDelegate::HandleCall, base::Unretained(this),
        spec, &type_refs, type.full_name)));
    object_template->Set(
        gin::StringToSymbol(isolate, type.exposed_name),
        v8::FunctionTemplate::New(
            isolate, &RunDeclarativeContentHooksDelegateHandlerCallback,
            v8::External::New(isolate, callbacks_.back().get())));
  }
}

void DeclarativeContentHooksDelegate::HandleCall(
    const ArgumentSpec* spec,
    const APITypeReferenceMap* type_refs,
    const std::string& type_name,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments arguments(info);
  v8::Isolate* isolate = arguments.isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // TODO(devlin): It would be pretty nice to be able to throw an error if
  // Arguments::IsConstructCall() is false. That would ensure that the caller
  // used `new declarativeContent.Foo()`, which is a) the documented approach
  // and b) allows us (more) confidence that the |this| object we receive is
  // an unmodified instance. But we don't know how many extensions enforcing
  // that may break, and it's also incompatible with SetIcon().

  v8::Local<v8::Object> this_object = info.This();
  if (this_object.IsEmpty()) {
    // Crazy script (e.g. declarativeContent.Foo.apply(null, args);).
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // TODO(devlin): Find a way to use APISignature here? It's a little awkward
  // because of undocumented expected properties like instanceType and not
  // requiring an argument at all. We may need a better way of expressing these
  // in the JSON schema.
  if (arguments.Length() > 1) {
    arguments.ThrowTypeError("Invalid invocation.");
    return;
  }

  v8::Local<v8::Object> properties;
  if (arguments.Length() == 1 && !arguments.GetNext(&properties)) {
    arguments.ThrowTypeError("Invalid invocation.");
    return;
  }

  std::string error;
  bool success = false;
  {
    v8::TryCatch try_catch(isolate);
    success = Validate(spec, *type_refs, context, this_object, properties,
                       type_name, &error);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }
  }

  if (!success)
    arguments.ThrowTypeError("Invalid invocation: " + error);
}

}  // namespace extensions
