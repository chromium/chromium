// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/v8_schema_registry.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "extensions/renderer/v8_helpers.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-value.h"

using content::V8ValueConverter;

namespace extensions {

namespace {

// Recursively freezes every v8 object on |object|.
void DeepFreeze(const v8::Local<v8::Object>& object,
                const v8::Local<v8::Context>& context) {
  // Don't let the object trace upwards via the prototype.
  v8::Maybe<bool> maybe =
      object->SetPrototype(context, v8::Null(context->GetIsolate()));
  CHECK(maybe.IsJust() && maybe.FromJust());
  v8::Local<v8::Array> property_names =
      object->GetOwnPropertyNames(context).ToLocalChecked();
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> child =
        object->Get(context, property_names->Get(context, i).ToLocalChecked())
            .ToLocalChecked();
    if (child->IsObject())
      DeepFreeze(v8::Local<v8::Object>::Cast(child), context);
  }
  object->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen);
}

class SchemaRegistryNativeHandler : public ObjectBackedNativeHandler {
 public:
  SchemaRegistryNativeHandler(V8SchemaRegistry* registry,
                              std::unique_ptr<ScriptContext> context)
      : ObjectBackedNativeHandler(context.get()),
        context_(std::move(context)),
        registry_(registry) {}

  // ObjectBackedNativeHandler:
  void AddRoutes() override {
    RouteHandlerFunction(
        "GetSchema",
        base::BindRepeating(&SchemaRegistryNativeHandler::GetSchema,
                            base::Unretained(this)));
    RouteHandlerFunction(
        "GetObjectType",
        base::BindRepeating(&SchemaRegistryNativeHandler::GetObjectType,
                            base::Unretained(this)));
  }

  ~SchemaRegistryNativeHandler() override { context_->Invalidate(); }

 private:
  void GetSchema(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(registry_->GetSchema(
        args.GetIsolate(), *v8::String::Utf8Value(args.GetIsolate(), args[0])));
  }

  void GetObjectType(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 1 && args[0]->IsObject());
    std::string type;
    if (args[0]->IsArray())
      type = "array";
    else if (args[0]->IsArrayBuffer() || args[0]->IsArrayBufferView())
      type = "binary";
    else
      type = "object";
    args.GetReturnValue().Set(
        v8_helpers::ToV8StringUnsafe(context()->isolate(), type.c_str()));
  }

  std::unique_ptr<ScriptContext> context_;
  raw_ptr<V8SchemaRegistry> registry_;
};

}  // namespace

V8SchemaRegistry::V8SchemaRegistry() {
}

V8SchemaRegistry::~V8SchemaRegistry() {
}

std::unique_ptr<NativeHandler> V8SchemaRegistry::AsNativeHandler(
    v8::Isolate* isolate) {
  std::unique_ptr<ScriptContext> context(
      new ScriptContext(GetOrCreateContext(isolate),
                        nullptr,          // no frame
                        mojom::HostID(),  // no host_id
                        nullptr,          // no extension
                        /*blink_isolated_world_id=*/std::nullopt,
                        mojom::ContextType::kUnspecified,
                        nullptr,  // no effective extension
                        mojom::ContextType::kUnspecified));
  return std::unique_ptr<NativeHandler>(
      new SchemaRegistryNativeHandler(this, std::move(context)));
}

v8::Local<v8::Array> V8SchemaRegistry::GetSchemas(
    v8::Isolate* isolate,
    const std::vector<std::string>& apis) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetOrCreateContext(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Array> v8_apis(v8::Array::New(isolate, apis.size()));
  size_t api_index = 0;
  for (auto i = apis.cbegin(); i != apis.cend(); ++i) {
    bool set_result =
        v8_apis->Set(context, api_index++, GetSchema(isolate, *i)).ToChecked();
    // Set() should never return false without throwing an exception (which
    // would be caught by the ToChecked() above).
    DCHECK(set_result);
  }
  return handle_scope.Escape(v8_apis);
}

v8::Local<v8::Object> V8SchemaRegistry::GetSchema(v8::Isolate* isolate,
                                                  const std::string& api) {
  if (schema_cache_ != nullptr) {
    v8::Local<v8::Object> cached_schema = schema_cache_->Get(api);
    if (!cached_schema.IsEmpty()) {
      return cached_schema;
    }
  }

  // Slow path: Need to build schema first.

  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetOrCreateContext(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);

  std::string_view schema_string =
      ExtensionAPI::GetSharedInstance()->GetSchemaStringPiece(api);
  CHECK(!schema_string.empty());
  v8::MaybeLocal<v8::String> v8_maybe_string = v8::String::NewExternalOneByte(
      isolate, new StaticV8ExternalOneByteStringResource(schema_string));
  v8::Local<v8::String> v8_schema_string;
  CHECK(v8_maybe_string.ToLocal(&v8_schema_string));

  v8::MaybeLocal<v8::Value> v8_maybe_schema_value =
      v8::JSON::Parse(context, v8_schema_string);
  v8::Local<v8::Value> v8_schema_value;
  CHECK(v8_maybe_schema_value.ToLocal(&v8_schema_value));
  CHECK(v8_schema_value->IsObject());

  v8::Local<v8::Object> v8_schema_object(
      v8::Local<v8::Object>::Cast(v8_schema_value));
  DeepFreeze(v8_schema_object, context);

  schema_cache_->Set(api, v8_schema_object);

  return handle_scope.Escape(v8_schema_object);
}

v8::Local<v8::Context> V8SchemaRegistry::GetOrCreateContext(
    v8::Isolate* isolate) {
  // It's ok to create local handles in this function, since this is only called
  // when we have a HandleScope.
  if (!context_holder_) {
    context_holder_ = std::make_unique<gin::ContextHolder>(isolate);
    context_holder_->SetContext(v8::Context::New(isolate));
    schema_cache_ = std::make_unique<SchemaCache>(isolate);
    return context_holder_->context();
  }
  return context_holder_->context();
}

}  // namespace extensions
