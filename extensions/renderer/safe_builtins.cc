// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/safe_builtins.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

struct SafeBuiltins::BuiltinInfo {
  v8::Global<v8::Function> ctor;
  V8MethodMap proto_methods;
  V8MethodMap static_methods;
};

struct SafeBuiltins::UntaintedBuiltins {
  UntaintedBuiltins() = default;
  ~UntaintedBuiltins() = default;

  BuiltinInfo array;
  BuiltinInfo function;
  BuiltinInfo object;
  BuiltinInfo string;
  BuiltinInfo regexp;
  BuiltinInfo error;
  BuiltinInfo promise;

  v8::Global<v8::Value> symbol_to_string_tag;

  v8::Global<v8::Function> json_parse;
  v8::Global<v8::Function> json_stringify;
  v8::Global<v8::Object> json_object;

  struct TypeAndToJSON {
    v8::Global<v8::Object> prototype;
    v8::Global<v8::Value> original_to_json;
  };
  std::vector<TypeAndToJSON> builtin_to_jsons;
};

namespace {

void ThrowSafeObjectCalledCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8::Exception::TypeError(
      v8_helpers::ToV8StringUnsafe(info.GetIsolate(),
                                   "Safe objects cannot be called nor "
                                   "constructed. Use $Foo.self() or new "
                                   "$Foo.self() instead.")));
}

void CallInstanceMethodCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Function> target_fn = info.Data().As<v8::Function>();

  if (info.Length() == 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helpers::ToV8StringUnsafe(isolate,
                                     "There must be at least one argument, "
                                     "the receiver")));
    return;
  }

  v8::Local<v8::Value> recv = info[0];
  int argc = info.Length() - 1;
  std::vector<v8::Local<v8::Value>> argv(argc);
  for (int i = 0; i < argc; ++i) {
    argv[i] = info[i + 1];
  }

  v8::Local<v8::Value> return_value;
  if (target_fn->Call(context, recv, argc, argv.data())
          .ToLocal(&return_value)) {
    info.GetReturnValue().Set(return_value);
  }
}

void CallStaticMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::PrimitiveArray> data = info.Data().As<v8::PrimitiveArray>();
  v8::Local<v8::Value> target_val = data->Get(isolate, 0);
  CHECK(target_val->IsFunction());
  v8::Local<v8::Function> target_fn = target_val.As<v8::Function>();
  v8::Local<v8::Value> recv = data->Get(isolate, 1);

  int argc = info.Length();
  std::vector<v8::Local<v8::Value>> argv(argc);
  for (int i = 0; i < argc; ++i) {
    argv[i] = info[i];
  }

  v8::Local<v8::Value> return_value;
  if (target_fn->Call(context, recv, argc, argv.data())
          .ToLocal(&return_value)) {
    info.GetReturnValue().Set(return_value);
  }
}

// Custom wrapper for `JSON.stringify`.
//
// Web pages can clobber `toJSON` methods on built-in prototypes (e.g.,
// `Array.prototype.toJSON = ...`), which would be called by `JSON.stringify`.
// To prevent this, this callback temporarily restores the original (untainted)
// `toJSON` methods on the prototypes of standard built-ins before invoking
// the real `JSON.stringify`, and restores the page's overrides afterward.
//
// Gotchas:
// 1. Prototype Pollution/Inheritance: We must check the entire prototype chain
//    of the target prototype to see if `toJSON` resolves to an overridden
//    value, as the override could be inherited (e.g., if defined on
//    `Object.prototype`).
// 2. Proxy Traps: Querying or modifying prototype properties might trigger
//    page-defined Proxy traps, which can execute arbitrary JS and throw
//    exceptions. We use safe `ToLocal` checks and abort early if V8 fails.
// 3. Property Attributes: When restoring, we must re-apply the original
//    descriptor's attributes (configurable, enumerable, writable, get, set)
//    rather than resetting to defaults, to preserve page expectations.
// 4. Microtasks: We suppress microtask execution during this callback to
//    prevent unrelated page scripts from running while prototypes are
//    temporarily mutated.
void JSONStringifyCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  // State needed to restore a single prototype's original `toJSON` property.
  struct SavedToJSON {
    v8::Local<v8::Object> proto;
    bool had_own_property = false;
    v8::Local<v8::Value> original_descriptor;
  };

  // RAII helper to automatically restore all modified `toJSON` properties
  // when exiting this function (even in case of exceptions or early returns).
  struct ToJSONRestorer {
    v8::Local<v8::Context> context;
    v8::Local<v8::String> to_json_key;
    std::vector<SavedToJSON> saved;

    ~ToJSONRestorer() {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      // Use TryCatch to swallow exceptions. Restoring prototype properties
      // might trigger page-defined setter traps or fail if prototypes were
      // frozen/sealed by the page. We must attempt to restore all properties
      // even if some fail.
      v8::TryCatch try_catch(isolate);
      for (const auto& item : saved) {
        // Delete the temporary property we added.
        std::ignore = item.proto->Delete(context, to_json_key);

        // If the prototype originally had its own `toJSON`, restore it using
        // the original property descriptor.
        if (item.had_own_property && !item.original_descriptor.IsEmpty() &&
            item.original_descriptor->IsObject()) {
          v8::Local<v8::Object> desc_obj =
              item.original_descriptor.As<v8::Object>();
          bool configurable = false;
          bool enumerable = false;
          bool writable = false;
          v8::Local<v8::Value> tmp_val;
          if (desc_obj
                  ->GetRealNamedProperty(context, v8_helpers::ToV8StringUnsafe(
                                                      isolate, "configurable"))
                  .ToLocal(&tmp_val) &&
              tmp_val->IsBoolean()) {
            configurable = tmp_val.As<v8::Boolean>()->Value();
          }
          if (desc_obj
                  ->GetRealNamedProperty(context, v8_helpers::ToV8StringUnsafe(
                                                      isolate, "enumerable"))
                  .ToLocal(&tmp_val) &&
              tmp_val->IsBoolean()) {
            enumerable = tmp_val.As<v8::Boolean>()->Value();
          }
          if (desc_obj
                  ->GetRealNamedProperty(context, v8_helpers::ToV8StringUnsafe(
                                                      isolate, "writable"))
                  .ToLocal(&tmp_val) &&
              tmp_val->IsBoolean()) {
            writable = tmp_val.As<v8::Boolean>()->Value();
          }

          v8::Local<v8::Value> get_fn, set_fn, val_val;
          bool has_get =
              desc_obj
                  ->GetRealNamedProperty(
                      context, v8_helpers::ToV8StringUnsafe(isolate, "get"))
                  .ToLocal(&get_fn) &&
              !get_fn->IsUndefined();
          bool has_set =
              desc_obj
                  ->GetRealNamedProperty(
                      context, v8_helpers::ToV8StringUnsafe(isolate, "set"))
                  .ToLocal(&set_fn) &&
              !set_fn->IsUndefined();
          if (has_get || has_set) {
            // Restore accessor descriptor.
            v8::PropertyDescriptor desc(
                get_fn.IsEmpty() ? v8::Undefined(isolate).As<v8::Value>()
                                 : get_fn,
                set_fn.IsEmpty() ? v8::Undefined(isolate).As<v8::Value>()
                                 : set_fn);
            desc.set_configurable(configurable);
            desc.set_enumerable(enumerable);
            std::ignore =
                item.proto->DefineProperty(context, to_json_key, desc);
          } else if (desc_obj
                         ->GetRealNamedProperty(
                             context,
                             v8_helpers::ToV8StringUnsafe(isolate, "value"))
                         .ToLocal(&val_val)) {
            // Restore data descriptor.
            v8::PropertyDescriptor desc(val_val, writable);
            desc.set_configurable(configurable);
            desc.set_enumerable(enumerable);
            std::ignore =
                item.proto->DefineProperty(context, to_json_key, desc);
          }
        }
      }
    }
  };

  v8::TryCatch try_catch(isolate);
  {
    ToJSONRestorer restorer{context,
                            v8_helpers::ToV8StringUnsafe(isolate, "toJSON")};

    [&]() {
      // Unpack internal data: [target_function, receiver,
      // to_json_entries_array].
      v8::Local<v8::PrimitiveArray> data = info.Data().As<v8::PrimitiveArray>();
      v8::Local<v8::Value> target_val = data->Get(isolate, 0);
      CHECK(target_val->IsFunction());
      v8::Local<v8::Function> target_fn = target_val.As<v8::Function>();

      v8::Local<v8::Value> recv = data->Get(isolate, 1);

      // Temporarily restore original `toJSON` methods on prototypes if they
      // have been overridden, so that `JSON.stringify` uses the untainted
      // versions.
      int count = (data->Length() - 2) / 2;
      for (int i = 0; i < count; ++i) {
        v8::Local<v8::Value> proto_val = data->Get(isolate, 2 + 2 * i);
        CHECK(proto_val->IsObject());
        v8::Local<v8::Object> proto = proto_val.As<v8::Object>();
        v8::Local<v8::Value> orig_to_json_val =
            data->Get(isolate, 2 + 2 * i + 1);

        // Check if the prototype chain currently resolves `toJSON` to something
        // other than the original method. We walk the prototype chain because
        // the override might be inherited (e.g. if defined on
        // `Object.prototype`).
        bool needs_override = false;
        v8::Local<v8::Object> curr = proto;
        while (!curr.IsEmpty() && !curr->IsNull()) {
          v8::Local<v8::Value> desc_val;
          // GetOwnPropertyDescriptor can run page JS if the prototype is a
          // Proxy, which might throw. Return early to abort and let the
          // exception propagate.
          if (!curr->GetOwnPropertyDescriptor(context, restorer.to_json_key)
                   .ToLocal(&desc_val)) {
            return;
          }
          if (desc_val->IsObject()) {
            v8::Local<v8::Object> desc_obj = desc_val.As<v8::Object>();
            v8::Local<v8::Value> val_val;
            if (desc_obj
                    ->GetRealNamedProperty(
                        context, v8_helpers::ToV8StringUnsafe(isolate, "value"))
                    .ToLocal(&val_val)) {
              if (val_val != orig_to_json_val) {
                // The property exists and has a value different from the
                // original.
                needs_override = true;
              }
            } else {
              // It's an accessor descriptor (getter/setter) instead of a data
              // property, which is also considered an override.
              needs_override = true;
            }
            break;
          }
          v8::Local<v8::Value> parent = curr->GetPrototype();
          curr = (!parent.IsEmpty() && parent->IsObject())
                     ? parent.As<v8::Object>()
                     : v8::Local<v8::Object>();
        }

        if (needs_override) {
          // Save the original property descriptor if the prototype had its own
          // `toJSON` property, so we can restore it exactly.
          v8::Local<v8::Value> descriptor;
          if (!proto->GetOwnPropertyDescriptor(context, restorer.to_json_key)
                   .ToLocal(&descriptor)) {
            return;
          }
          bool had_own = !descriptor.IsEmpty() && descriptor->IsObject();
          if (!had_own) {
            descriptor.Clear();
          }
          // Temporarily define the original `toJSON` as a plain data property.
          // This will shadow any inherited overrides and replace any own
          // overrides.
          bool created = false;
          if (!proto
                   ->CreateDataProperty(context, restorer.to_json_key,
                                        orig_to_json_val)
                   .To(&created) ||
              !created) {
            return;
          }
          restorer.saved.push_back({proto, had_own, descriptor});
        }
      }

      // Forward the arguments to the original JSON.stringify call.
      int argc = info.Length();
      std::vector<v8::Local<v8::Value>> argv(argc);
      for (int i = 0; i < argc; ++i) {
        argv[i] = info[i];
      }

      v8::Local<v8::Value> return_value;
      if (target_fn->Call(context, recv, argc, argv.data())
              .ToLocal(&return_value)) {
        info.GetReturnValue().Set(return_value);
      }
    }();
  }
}

}  // namespace

SafeBuiltins::SafeBuiltins(ScriptContext* context) : context_(context) {
  v8::Isolate* isolate = context_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context_->v8_context();
  v8::Context::Scope context_scope(v8_context);

  untainted_ = std::make_unique<UntaintedBuiltins>();

  v8::Local<v8::Object> global = v8_context->Global();

  auto capture_builtin = [&](const char* name, BuiltinInfo* info,
                             base::span<const char* const> proto_methods,
                             base::span<const char* const> static_methods) {
    v8::Local<v8::Value> ctor_val;
    CHECK(global->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, name))
              .ToLocal(&ctor_val));
    CHECK(ctor_val->IsFunction());
    v8::Local<v8::Function> ctor = ctor_val.As<v8::Function>();
    info->ctor.Reset(isolate, ctor);

    v8::Local<v8::Value> proto_val;
    CHECK(ctor->Get(v8_context,
                    v8_helpers::ToV8StringUnsafe(isolate, "prototype"))
              .ToLocal(&proto_val));
    CHECK(proto_val->IsObject());
    v8::Local<v8::Object> proto = proto_val.As<v8::Object>();

    for (const char* method_name : proto_methods) {
      v8::Local<v8::Value> method_val;
      CHECK(proto
                ->Get(v8_context,
                      v8_helpers::ToV8StringUnsafe(isolate, method_name))
                .ToLocal(&method_val));
      CHECK(method_val->IsFunction());
      info->proto_methods[method_name].Reset(isolate,
                                             method_val.As<v8::Function>());
    }

    for (const char* method_name : static_methods) {
      v8::Local<v8::Value> method_val;
      CHECK(ctor->Get(v8_context,
                      v8_helpers::ToV8StringUnsafe(isolate, method_name))
                .ToLocal(&method_val));
      CHECK(method_val->IsFunction());
      info->static_methods[method_name].Reset(isolate,
                                              method_val.As<v8::Function>());
    }
  };

  static constexpr const char* const kArrayProto[] = {
      "concat", "forEach", "includes", "indexOf", "join",
      "push",   "slice",   "splice",   "map",     "filter",
      "shift",  "unshift", "pop",      "reverse", "find"};
  static constexpr const char* const kArrayStatic[] = {"from", "isArray"};
  capture_builtin("Array", &untainted_->array, kArrayProto, kArrayStatic);

  static constexpr const char* const kFunctionProto[] = {"apply", "bind",
                                                         "call"};
  capture_builtin("Function", &untainted_->function, kFunctionProto, {});

  static constexpr const char* const kObjectProto[] = {"hasOwnProperty"};
  static constexpr const char* const kObjectStatic[] = {
      "assign",         "create", "defineProperty",
      "entries",        "freeze", "getOwnPropertyDescriptor",
      "getPrototypeOf", "keys",   "setPrototypeOf"};
  capture_builtin("Object", &untainted_->object, kObjectProto, kObjectStatic);

  static constexpr const char* const kStringProto[] = {
      "indexOf",     "slice",       "split",  "substr",
      "toLowerCase", "toUpperCase", "replace"};
  static constexpr const char* const kStringStatic[] = {"fromCharCode"};
  capture_builtin("String", &untainted_->string, kStringProto, kStringStatic);

  // Use exec rather than test to defend against clobbering in the
  // presence of ES2015 semantics, which read RegExp.prototype.exec.
  static constexpr const char* const kRegExpProto[] = {"exec"};
  capture_builtin("RegExp", &untainted_->regexp, kRegExpProto, {});

  static constexpr const char* const kErrorStatic[] = {"captureStackTrace"};
  capture_builtin("Error", &untainted_->error, {}, kErrorStatic);

  static constexpr const char* const kPromiseProto[] = {"then", "catch"};
  static constexpr const char* const kPromiseStatic[] = {"race", "resolve"};
  capture_builtin("Promise", &untainted_->promise, kPromiseProto,
                  kPromiseStatic);

  v8::Local<v8::Value> symbol_val;
  CHECK(global->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, "Symbol"))
            .ToLocal(&symbol_val));
  CHECK(symbol_val->IsObject());
  v8::Local<v8::Object> symbol_obj = symbol_val.As<v8::Object>();
  v8::Local<v8::Value> tag_val;
  CHECK(symbol_obj
            ->Get(v8_context,
                  v8_helpers::ToV8StringUnsafe(isolate, "toStringTag"))
            .ToLocal(&tag_val));
  untainted_->symbol_to_string_tag.Reset(isolate, tag_val);

  v8::Local<v8::Value> json_val;
  CHECK(global->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, "JSON"))
            .ToLocal(&json_val));
  CHECK(json_val->IsObject());
  v8::Local<v8::Object> json_obj = json_val.As<v8::Object>();
  v8::Local<v8::Value> parse_val, stringify_val;
  CHECK(
      json_obj->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, "parse"))
          .ToLocal(&parse_val));
  CHECK(parse_val->IsFunction());
  untainted_->json_parse.Reset(isolate, parse_val.As<v8::Function>());

  CHECK(
      json_obj
          ->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, "stringify"))
          .ToLocal(&stringify_val));
  CHECK(stringify_val->IsFunction());
  untainted_->json_stringify.Reset(isolate, stringify_val.As<v8::Function>());
  untainted_->json_object.Reset(isolate, json_obj);

  const char* const builtin_type_names[] = {"Object", "Function", "Array",
                                            "String", "Boolean",  "Number",
                                            "Date",   "RegExp"};
  for (const char* type_name : builtin_type_names) {
    v8::Local<v8::Value> type_val;
    CHECK(
        global
            ->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, type_name))
            .ToLocal(&type_val));
    CHECK(type_val->IsFunction());
    v8::Local<v8::Function> type_ctor = type_val.As<v8::Function>();
    v8::Local<v8::Value> proto_val;
    CHECK(type_ctor
              ->Get(v8_context,
                    v8_helpers::ToV8StringUnsafe(isolate, "prototype"))
              .ToLocal(&proto_val));
    CHECK(proto_val->IsObject());
    v8::Local<v8::Object> proto = proto_val.As<v8::Object>();
    v8::Local<v8::Value> to_json;
    CHECK(
        proto->Get(v8_context, v8_helpers::ToV8StringUnsafe(isolate, "toJSON"))
            .ToLocal(&to_json));
    UntaintedBuiltins::TypeAndToJSON entry;
    entry.prototype.Reset(isolate, proto);
    entry.original_to_json.Reset(isolate, to_json);
    untainted_->builtin_to_jsons.push_back(std::move(entry));
  }
}

SafeBuiltins::~SafeBuiltins() {
  Reset();
}

void SafeBuiltins::Reset() {
  untainted_.reset();
  safe_array_.Reset();
  safe_function_.Reset();
  safe_json_.Reset();
  safe_object_.Reset();
  safe_regexp_.Reset();
  safe_string_.Reset();
  safe_error_.Reset();
  safe_promise_.Reset();
  safe_symbol_.Reset();
}

v8::Local<v8::Object> SafeBuiltins::CreateSafeBuiltin(
    v8::Local<v8::Function> ctor,
    const V8MethodMap& proto_methods,
    const V8MethodMap& static_methods) const {
  v8::Isolate* isolate = context_->isolate();
  v8::Local<v8::Context> v8_context = context_->v8_context();
  v8::MicrotasksScope microtasks_scope(
      isolate, v8_context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Function> safe =
      v8::Function::New(v8_context, ThrowSafeObjectCalledCallback)
          .ToLocalChecked();

  if (!ctor.IsEmpty()) {
    std::ignore = safe->CreateDataProperty(
        v8_context, v8_helpers::ToV8StringUnsafe(isolate, "self"), ctor);
  }

  for (const auto& [name, global_fn] : proto_methods) {
    if (global_fn.IsEmpty()) {
      continue;
    }
    v8::Local<v8::Function> fn = global_fn.Get(isolate);
    v8::Local<v8::Function> method =
        v8::Function::New(v8_context, CallInstanceMethodCallback, fn)
            .ToLocalChecked();
    std::ignore = safe->CreateDataProperty(
        v8_context, v8_helpers::ToV8StringUnsafe(isolate, name.c_str()),
        method);
  }

  for (const auto& [name, global_fn] : static_methods) {
    if (global_fn.IsEmpty()) {
      continue;
    }
    v8::Local<v8::Function> fn = global_fn.Get(isolate);
    v8::Local<v8::PrimitiveArray> data = v8::PrimitiveArray::New(isolate, 2);
    data->Set(isolate, 0, fn.As<v8::Primitive>());
    data->Set(isolate, 1,
              (ctor.IsEmpty() ? v8::Undefined(isolate).As<v8::Value>()
                              : ctor.As<v8::Value>())
                  .As<v8::Primitive>());

    v8::Local<v8::Function> method =
        v8::Function::New(v8_context, CallStaticMethodCallback, data)
            .ToLocalChecked();
    std::ignore = safe->CreateDataProperty(
        v8_context, v8_helpers::ToV8StringUnsafe(isolate, name.c_str()),
        method);
  }

  return safe;
}

v8::Local<v8::Object> SafeBuiltins::GetOrCreateSafeBuiltin(
    v8::Global<v8::Object>& safe_object,
    const BuiltinInfo& info) const {
  if (safe_object.IsEmpty()) {
    v8::Isolate* isolate = context_->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> v8_context = context_->v8_context();
    v8::Context::Scope context_scope(v8_context);

    v8::Local<v8::Function> ctor;
    if (!info.ctor.IsEmpty()) {
      ctor = info.ctor.Get(isolate);
    }

    v8::Local<v8::Object> safe =
        CreateSafeBuiltin(ctor, info.proto_methods, info.static_methods);
    safe_object.Reset(isolate, safe);
  }
  return safe_object.Get(context_->isolate());
}

v8::Local<v8::Object> SafeBuiltins::GetArray() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_array_, untainted_->array);
}

v8::Local<v8::Object> SafeBuiltins::GetFunction() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_function_, untainted_->function);
}

v8::Local<v8::Object> SafeBuiltins::GetJSON() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  if (!safe_json_.IsEmpty()) {
    return safe_json_.Get(context_->isolate());
  }

  v8::Isolate* isolate = context_->isolate();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context_->v8_context();
  v8::Context::Scope context_scope(v8_context);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8_context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> json_obj = v8::Object::New(isolate);
  v8::Local<v8::Object> global_json = untainted_->json_object.Get(isolate);

  if (!untainted_->json_parse.IsEmpty()) {
    v8::Local<v8::Function> parse_fn = untainted_->json_parse.Get(isolate);
    v8::Local<v8::PrimitiveArray> data = v8::PrimitiveArray::New(isolate, 2);
    data->Set(isolate, 0, parse_fn.As<v8::Primitive>());
    data->Set(isolate, 1, global_json.As<v8::Primitive>());
    v8::Local<v8::Function> method =
        v8::Function::New(v8_context, CallStaticMethodCallback, data)
            .ToLocalChecked();
    std::ignore = json_obj->CreateDataProperty(
        v8_context, v8_helpers::ToV8StringUnsafe(isolate, "parse"), method);
  }

  if (!untainted_->json_stringify.IsEmpty()) {
    v8::Local<v8::Function> stringify_fn =
        untainted_->json_stringify.Get(isolate);
    v8::Local<v8::PrimitiveArray> data = v8::PrimitiveArray::New(
        isolate, static_cast<int>(2 + 2 * untainted_->builtin_to_jsons.size()));
    data->Set(isolate, 0, stringify_fn.As<v8::Primitive>());
    data->Set(isolate, 1, global_json.As<v8::Primitive>());

    for (size_t i = 0; i < untainted_->builtin_to_jsons.size(); ++i) {
      v8::Local<v8::Object> proto =
          untainted_->builtin_to_jsons[i].prototype.Get(isolate);
      v8::Local<v8::Value> to_json =
          untainted_->builtin_to_jsons[i].original_to_json.Get(isolate);
      data->Set(isolate, static_cast<int>(2 + 2 * i),
                proto.As<v8::Primitive>());
      data->Set(isolate, static_cast<int>(2 + 2 * i + 1),
                to_json.As<v8::Primitive>());
    }

    v8::Local<v8::Function> method =
        v8::Function::New(v8_context, JSONStringifyCallback, data)
            .ToLocalChecked();
    std::ignore = json_obj->CreateDataProperty(
        v8_context, v8_helpers::ToV8StringUnsafe(isolate, "stringify"), method);
  }

  safe_json_.Reset(isolate, json_obj);
  return handle_scope.Escape(json_obj);
}

v8::Local<v8::Object> SafeBuiltins::GetObjekt() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_object_, untainted_->object);
}

v8::Local<v8::Object> SafeBuiltins::GetRegExp() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_regexp_, untainted_->regexp);
}

v8::Local<v8::Object> SafeBuiltins::GetString() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_string_, untainted_->string);
}

v8::Local<v8::Object> SafeBuiltins::GetError() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_error_, untainted_->error);
}

v8::Local<v8::Object> SafeBuiltins::GetPromise() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  return GetOrCreateSafeBuiltin(safe_promise_, untainted_->promise);
}

v8::Local<v8::Object> SafeBuiltins::GetSymbol() const {
  if (!untainted_) {
    return v8::Local<v8::Object>();
  }
  if (safe_symbol_.IsEmpty()) {
    v8::Isolate* isolate = context_->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> v8_context = context_->v8_context();
    v8::Context::Scope context_scope(v8_context);
    v8::MicrotasksScope microtasks_scope(
        isolate, v8_context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);

    v8::Local<v8::Object> safe = v8::Object::New(isolate);
    if (!untainted_->symbol_to_string_tag.IsEmpty()) {
      std::ignore = safe->CreateDataProperty(
          v8_context, v8_helpers::ToV8StringUnsafe(isolate, "toStringTag"),
          untainted_->symbol_to_string_tag.Get(isolate));
    }
    safe_symbol_.Reset(isolate, safe);
  }
  return safe_symbol_.Get(context_->isolate());
}

}  // namespace extensions
