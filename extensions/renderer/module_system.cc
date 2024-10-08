// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/source_map.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace extensions {

using v8_helpers::GetPrivateProperty;
using v8_helpers::SetPrivateProperty;
using v8_helpers::ToV8String;
using v8_helpers::ToV8StringUnsafe;

namespace {

const char kModuleSystem[] = "module_system";
const char kModuleName[] = "module_name";
const char kModuleField[] = "module_field";
const char kModulesField[] = "modules";

// Determines if certain fatal extensions errors should be suppressed
// (i.e., only logged) or allowed (i.e., logged before crashing).
bool ShouldSuppressFatalErrors() {
  // Suppress fatal everywhere until the cause of bugs like http://crbug/471599
  // are fixed. This would typically be:
  // return GetCurrentChannel() > version_info::Channel::DEV;
  return true;
}

// Logs an error for the calling context in preparation for potentially
// crashing the renderer, with some added metadata about the context:
//  - Its type (privileged, unprivileged, etc).
//  - Whether it's valid.
//  - The extension ID, if one exists.
// Crashing won't happen in stable/beta releases, but is encouraged to happen
// in the less stable released to catch errors early.
void Fatal(ScriptContext* context, const std::string& message) {
  // Prepend some context metadata.
  std::string full_message = "(";
  if (!context->is_valid())
    full_message += "Invalid ";
  full_message += context->GetContextTypeDescription();
  full_message += " context";
  if (context->extension()) {
    full_message += " for ";
    full_message += context->extension()->id();
  }
  full_message += ") ";
  full_message += message;

  if (ShouldSuppressFatalErrors()) {
    console::AddMessage(context, blink::mojom::ConsoleMessageLevel::kError,
                        full_message);
  } else {
    console::Fatal(context, full_message);
  }
}

void Warn(v8::Isolate* isolate, const std::string& message) {
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(isolate->GetCurrentContext());
  console::AddMessage(script_context,
                      blink::mojom::ConsoleMessageLevel::kWarning, message);
}

// Default exception handler which logs the exception.
class DefaultExceptionHandler : public ModuleSystem::ExceptionHandler {
 public:
  explicit DefaultExceptionHandler(ScriptContext* context)
      : ModuleSystem::ExceptionHandler(context) {}

  // Fatally dumps the debug info from |try_catch| to the console.
  // Make sure this is never used for exceptions that originate in external
  // code!
  void HandleUncaughtException(const v8::TryCatch& try_catch) override {
    v8::HandleScope handle_scope(context_->isolate());
    std::string stack_trace = "<stack trace unavailable>";
    v8::Local<v8::Value> v8_stack_trace;
    if (try_catch.StackTrace(context_->v8_context()).ToLocal(&v8_stack_trace)) {
      v8::String::Utf8Value stack_value(context_->isolate(), v8_stack_trace);
      if (*stack_value)
        stack_trace.assign(*stack_value, stack_value.length());
      else
        stack_trace = "<could not convert stack trace to string>";
    }
    Fatal(context_, CreateExceptionString(try_catch) + "{" + stack_trace + "}");
  }
};

// Sets a property on the "exports" object for bindings. Called by JS with
// exports.$set(<key>, <value>).
void SetExportsProperty(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Object> obj = args.This();
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  v8::Maybe<bool> result =
      obj->DefineOwnProperty(args.GetIsolate()->GetCurrentContext(),
                             args[0].As<v8::String>(), args[1], v8::ReadOnly);
  if (!result.FromMaybe(false))
    LOG(ERROR) << "Failed to set private property on the export.";
}

bool ContextNeedsMojoBindings(ScriptContext* context) {
  // Mojo is only used from JS by some APIs so a context only needs the mojo
  // bindings if at least one is available.
  //
  // Prefer to use Mojo from C++ if possible rather than adding to this list.
  static const char* const kApisRequiringMojo[] = {
      "mediaPerceptionPrivate", "mimeHandlerPrivate", "mojoPrivate",
  };

  for (const auto* api : kApisRequiringMojo) {
    if (context->GetAvailability(api).is_available())
      return true;
  }
  return false;
}

}  // namespace

std::string ModuleSystem::ExceptionHandler::CreateExceptionString(
    const v8::TryCatch& try_catch) {
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    return "try_catch has no message";
  }

  std::string resource_name = "<unknown resource>";
  if (!message->GetScriptOrigin().ResourceName().IsEmpty()) {
    v8::String::Utf8Value resource_name_v8(
        context_->isolate(), message->GetScriptOrigin().ResourceName());
    resource_name.assign(*resource_name_v8, resource_name_v8.length());
  }

  std::string error_message = "<no error message>";
  if (!message->Get().IsEmpty()) {
    v8::String::Utf8Value error_message_v8(context_->isolate(), message->Get());
    error_message.assign(*error_message_v8, error_message_v8.length());
  }

  int line_number = 0;
  if (context_) {  // |context_| can be null in unittests.
    auto maybe = message->GetLineNumber(context_->v8_context());
    line_number = maybe.IsJust() ? maybe.FromJust() : 0;
  }
  return base::StringPrintf("%s:%d: %s",
                            resource_name.c_str(),
                            line_number,
                            error_message.c_str());
}

ModuleSystem::ModuleSystem(ScriptContext* context, const SourceMap* source_map)
    : ObjectBackedNativeHandler(context),
      context_(context),
      source_map_(source_map),
      natives_enabled_(0),
      exception_handler_(new DefaultExceptionHandler(context)) {
  v8::Local<v8::Object> global(context->v8_context()->Global());
  v8::Isolate* isolate = context->isolate();
  // Note: Ensure setting private succeeds with CHECK.
  // TODO(crbug.com/40058107): remove checks once investigation finished.
  CHECK(SetPrivate(global, kModulesField, v8::Object::New(isolate)));
  CHECK(SetPrivate(global, kModuleSystem, v8::External::New(isolate, this)));
  {
    // Note: Ensure privates that were set above can be read immediately.
    // TODO(crbug.com/40058107): remove checks once investigation finished.
    v8::Local<v8::Value> dummy_value;
    CHECK(GetPrivate(global, kModulesField, &dummy_value));
    CHECK(GetPrivate(global, kModuleSystem, &dummy_value));
  }

  if (context_->GetRenderFrame() &&
      context_->context_type() == mojom::ContextType::kPrivilegedExtension &&
      !context_->IsForServiceWorker() && ContextNeedsMojoBindings(context_)) {
    // Valid enablement code path, so need to ensure MojoJS is allowed for the
    // process before attempting to enable it.
    blink::WebV8Features::AllowMojoJSForProcess();
    blink::WebV8Features::EnableMojoJS(context->v8_context(), true);
  }
}

ModuleSystem::~ModuleSystem() {
}

void ModuleSystem::AddRoutes() {
  RouteHandlerFunction(
      "require",
      base::BindRepeating(&ModuleSystem::RequireForJs, base::Unretained(this)));
  RouteHandlerFunction("requireNative",
                       base::BindRepeating(&ModuleSystem::RequireNative,
                                           base::Unretained(this)));
  RouteHandlerFunction(
      "loadScript",
      base::BindRepeating(&ModuleSystem::LoadScript, base::Unretained(this)));
  RouteHandlerFunction("privates", base::BindRepeating(&ModuleSystem::Private,
                                                       base::Unretained(this)));
}

void ModuleSystem::Invalidate() {
  // TODO(crbug.com/40058107): remove checks once investigation finished.
  CHECK(!has_been_invalidated_);
  has_been_invalidated_ = true;

  v8::Isolate* isolate = GetIsolate();
  // Clear the module system properties from the global context. It's polite,
  // and we use this as a signal in lazy handlers that we no longer exist.
  {
    // Note: It isn't safe to access v8::Private if IsExecutionTerminating
    // returns true. It crashes if we do so: http://crbug.com/1276144.
    if (!isolate->IsExecutionTerminating()) {
      v8::HandleScope scope(GetIsolate());
      v8::Local<v8::Object> global = context()->v8_context()->Global();
      // TODO(crbug.com/40058107): remove checks once investigation finished.
      v8::Local<v8::Value> dummy_value;
      CHECK(GetPrivate(global, kModulesField, &dummy_value));
      DeletePrivate(global, kModulesField);
      CHECK(GetPrivate(global, kModuleSystem, &dummy_value));
      DeletePrivate(global, kModuleSystem);
    }
  }

  // Invalidate all active and clobbered NativeHandlers we own.
  for (const auto& handler : native_handler_map_)
    handler.second->Invalidate();
  for (const auto& clobbered_handler : clobbered_native_handlers_)
    clobbered_handler->Invalidate();

  ObjectBackedNativeHandler::Invalidate();
}

ModuleSystem::NativesEnabledScope::NativesEnabledScope(
    ModuleSystem* module_system)
    : module_system_(module_system) {
  module_system_->natives_enabled_++;
}

ModuleSystem::NativesEnabledScope::~NativesEnabledScope() {
  module_system_->natives_enabled_--;
  CHECK_GE(module_system_->natives_enabled_, 0);
}

void ModuleSystem::HandleException(const v8::TryCatch& try_catch) {
  exception_handler_->HandleUncaughtException(try_catch);
}

v8::MaybeLocal<v8::Object> ModuleSystem::Require(
    const std::string& module_name) {
  v8::Local<v8::String> v8_module_name;
  if (!ToV8String(GetIsolate(), module_name, &v8_module_name))
    return v8::MaybeLocal<v8::Object>();
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> value =
      RequireForJsInner(v8_module_name, true /* create */);
  if (value.IsEmpty() || !value->IsObject())
    return v8::MaybeLocal<v8::Object>();
  return handle_scope.Escape(value.As<v8::Object>());
}

void ModuleSystem::RequireForJs(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args[0]->IsString()) {
    NOTREACHED_IN_MIGRATION() << "require() called with a non-string argument";
    return;
  }
  v8::Local<v8::String> module_name = args[0].As<v8::String>();
  args.GetReturnValue().Set(RequireForJsInner(module_name, true /* create */));
}

v8::Local<v8::Value> ModuleSystem::RequireForJsInner(
    v8::Local<v8::String> module_name,
    bool create) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::Object> global(context()->v8_context()->Global());

  // The module system might have been deleted. This can happen if a different
  // context keeps a reference to us, but our frame is destroyed (e.g.
  // background page keeps reference to chrome object in a closed popup).
  v8::Local<v8::Value> modules_value;
  if (!GetPrivate(global, kModulesField, &modules_value) ||
      modules_value->IsUndefined()) {
    Warn(GetIsolate(), "Extension view no longer exists");
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Object> modules(v8::Local<v8::Object>::Cast(modules_value));
  v8::Local<v8::Value> exports;
  if (!GetPrivateProperty(v8_context, modules, module_name, &exports) ||
      !exports->IsUndefined())
    return handle_scope.Escape(exports);

  if (!create)
    return v8::Undefined(GetIsolate());

  exports = LoadModule(*v8::String::Utf8Value(GetIsolate(), module_name));
  SetPrivateProperty(v8_context, modules, module_name, exports);
  return handle_scope.Escape(exports);
}

void ModuleSystem::CallModuleMethodSafe(const std::string& module_name,
                                        const std::string& method_name) {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> no_args;
  CallModuleMethodSafe(module_name, method_name, 0, &no_args,
                       blink::WebScriptExecutionCallback());
}

void ModuleSystem::CallModuleMethodSafe(const std::string& module_name,
                                        const std::string& method_name,
                                        v8::LocalVector<v8::Value>* args) {
  CallModuleMethodSafe(module_name, method_name, args->size(), args->data(),
                       blink::WebScriptExecutionCallback());
}

void ModuleSystem::CallModuleMethodSafe(const std::string& module_name,
                                        const std::string& method_name,
                                        int argc,
                                        v8::Local<v8::Value> argv[]) {
  CallModuleMethodSafe(module_name, method_name, argc, argv,
                       blink::WebScriptExecutionCallback());
}

void ModuleSystem::CallModuleMethodSafe(
    const std::string& module_name,
    const std::string& method_name,
    int argc,
    v8::Local<v8::Value> argv[],
    blink::WebScriptExecutionCallback callback) {
  TRACE_EVENT2("v8", "v8.callModuleMethodSafe", "module_name", module_name,
               "method_name", method_name);

  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::Function> function =
      GetModuleFunction(module_name, method_name);
  if (function.IsEmpty()) {
    // This can legitimately happen when the module hasn't been loaded in the
    // context (since GetModuleFunction() does not load an unloaded module).
    // Typically, we won't do this, but we can in the case of, e.g., dispatching
    // events (where we'll try to dispatch to each context in a process). In
    // these cases, though, we can know that there are no listeners registered,
    // since the event module hasn't been loaded.
    return;
  }

  {
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetCaptureMessage(true);
    context_->SafeCallFunction(function, argc, argv, std::move(callback));
    if (try_catch.HasCaught())
      HandleException(try_catch);
  }
}

void ModuleSystem::RegisterNativeHandler(
    const std::string& name,
    std::unique_ptr<NativeHandler> native_handler) {
  ClobberExistingNativeHandler(name);
  native_handler_map_[name] = std::move(native_handler);
}

void ModuleSystem::OverrideNativeHandlerForTest(const std::string& name) {
  ClobberExistingNativeHandler(name);
  overridden_native_handlers_.insert(name);
}

// static
void ModuleSystem::LazyFieldGetter(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(!info.Data().IsEmpty());
  CHECK(info.Data()->IsObject());
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> parameters = v8::Local<v8::Object>::Cast(info.Data());
  // This context should be the same as context()->v8_context().
  v8::Local<v8::Context> context =
      parameters->GetCreationContextChecked(isolate);
  v8::Local<v8::Object> global(context->Global());
  v8::Local<v8::Value> module_system_value;
  if (!GetPrivate(context, global, kModuleSystem, &module_system_value) ||
      !module_system_value->IsExternal()) {
    // ModuleSystem has been deleted.
    // TODO(kalman): See comment in header file.
    Warn(isolate,
         "Module system has been deleted, does extension view exist?");
    return;
  }

  ModuleSystem* module_system = static_cast<ModuleSystem*>(
      v8::Local<v8::External>::Cast(module_system_value)->Value());

  v8::Local<v8::Value> v8_module_name;
  if (!GetPrivateProperty(context, parameters, kModuleName, &v8_module_name)) {
    Warn(isolate, "Cannot find module.");
    return;
  }
  std::string name = *v8::String::Utf8Value(isolate, v8_module_name);

  // As part of instantiating a module, we delete the getter and replace it with
  // the property directly. If we're trying to load the same module a second
  // time, it means something went wrong. Bail out early rather than going
  // through the initialization process again (since bindings may not expect to
  // run multiple times).
  if (!module_system->loaded_modules_.insert(name).second) {
    Warn(isolate, "Previous API instantiation failed.");
    return;
  }

  // Switch to our v8 context because we need functions created while running
  // the require()d module to belong to our context, not the current one.
  v8::Context::Scope context_scope(context);
  NativesEnabledScope natives_enabled_scope(module_system);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> module_value;
  if (!module_system->Require(name).ToLocal(&module_value)) {
    module_system->HandleException(try_catch);
    return;
  }

  v8::Local<v8::Object> module = v8::Local<v8::Object>::Cast(module_value);
  v8::Local<v8::Value> field_value;
  if (!GetPrivateProperty(context, parameters, kModuleField, &field_value)) {
    module_system->HandleException(try_catch);
    return;
  }
  v8::Local<v8::String> field;
  if (!field_value->ToString(context).ToLocal(&field)) {
    module_system->HandleException(try_catch);
    return;
  }

  if (!v8_helpers::IsTrue(module->Has(context, field))) {
    std::string field_str = *v8::String::Utf8Value(isolate, field);
    Fatal(module_system->context_,
          "Lazy require of " + name + "." + field_str + " did not set the " +
              field_str + " field");
    return;
  }

  v8::Local<v8::Value> new_field;
  if (!v8_helpers::GetProperty(context, module, field, &new_field)) {
    module_system->HandleException(try_catch);
    return;
  }

  // Ok for it to be undefined, among other things it's how bindings signify
  // that the extension doesn't have permission to use them.
  CHECK(!new_field.IsEmpty());

  // v8::Object::SetLazyDataProperty() machinery will reconfigure the property
  // to a regular data property with |new_field| value.
  info.GetReturnValue().Set(new_field);
}

void ModuleSystem::SetLazyField(v8::Local<v8::Object> object,
                                const std::string& field,
                                const std::string& module_name,
                                const std::string& module_field) {
  CHECK(field.size() < v8::String::kMaxLength);
  CHECK(module_name.size() < v8::String::kMaxLength);
  CHECK(module_field.size() < v8::String::kMaxLength);
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Object> parameters = v8::Object::New(GetIsolate());
  v8::Local<v8::Context> context = context_->v8_context();
  // Since we reset the accessor here, we remove the record of having loaded the
  // module.
  loaded_modules_.erase(module_name);
  SetPrivateProperty(context, parameters, kModuleName,
                     ToV8StringUnsafe(GetIsolate(), module_name.c_str()));
  SetPrivateProperty(context, parameters, kModuleField,
                     ToV8StringUnsafe(GetIsolate(), module_field.c_str()));
  auto maybe = object->SetLazyDataProperty(
      context, ToV8StringUnsafe(GetIsolate(), field.c_str()),
      &ModuleSystem::LazyFieldGetter, parameters);
  CHECK(v8_helpers::IsTrue(maybe));
}

void ModuleSystem::OnNativeBindingCreated(
    const std::string& api_name,
    v8::Local<v8::Value> api_bridge_value) {
  DCHECK(!get_internal_api_.IsEmpty());
  v8::HandleScope scope(GetIsolate());
  if (source_map_->Contains(api_name)) {
    // We need to load the custom bindings and store them in our modules.
    // Storing them is important so that calls through CallModuleMethod() route
    // to the proper objects, if they share the same name as an API.
    v8::Local<v8::Value> modules;
    if (!GetPrivate(context()->v8_context()->Global(), kModulesField,
                    &modules) ||
        !modules->IsObject()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    NativesEnabledScope enabled(this);
    v8::Local<v8::Value> exports =
        LoadModuleWithNativeAPIBridge(api_name, api_bridge_value);
    SetPrivateProperty(context()->v8_context(), modules.As<v8::Object>(),
                       gin::StringToSymbol(GetIsolate(), api_name), exports);
  }
}

void ModuleSystem::SetGetInternalAPIHook(
    v8::Local<v8::FunctionTemplate> get_internal_api) {
  DCHECK(get_internal_api_.IsEmpty());
  get_internal_api_.Set(GetIsolate(), get_internal_api);
}

void ModuleSystem::SetJSBindingUtilGetter(const JSBindingUtilGetter& getter) {
  DCHECK(js_binding_util_getter_.is_null());
  js_binding_util_getter_ = getter;
}

v8::Local<v8::Value> ModuleSystem::RunString(v8::Local<v8::String> code,
                                             v8::Local<v8::String> name) {
  return context_->RunScript(
      name, code,
      base::BindOnce(&ExceptionHandler::HandleUncaughtException,
                     base::Unretained(exception_handler_.get())),
      v8::ScriptCompiler::NoCacheReason::kNoCacheBecauseExtensionModule);
}

void ModuleSystem::RequireNative(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  std::string native_name = *v8::String::Utf8Value(args.GetIsolate(), args[0]);
  v8::Local<v8::Object> object;
  if (RequireNativeFromString(native_name).ToLocal(&object))
    args.GetReturnValue().Set(object);
}

v8::MaybeLocal<v8::Object> ModuleSystem::RequireNativeFromString(
    const std::string& native_name) {
  if (natives_enabled_ == 0) {
    // HACK: if in test throw exception so that we can test the natives-disabled
    // logic; however, under normal circumstances, this is programmer error so
    // we could crash.
    if (exception_handler_) {
      GetIsolate()->ThrowException(
          ToV8StringUnsafe(GetIsolate(), "Natives disabled"));
      return v8::MaybeLocal<v8::Object>();
    }
    Fatal(context_, "Natives disabled for requireNative(" + native_name + ")");
    return v8::MaybeLocal<v8::Object>();
  }

  if (overridden_native_handlers_.count(native_name) > 0u) {
    v8::Local<v8::Value> value = RequireForJsInner(
        ToV8StringUnsafe(GetIsolate(), native_name.c_str()), true /* create */);
    if (value.IsEmpty() || !value->IsObject())
      return v8::MaybeLocal<v8::Object>();
    return value.As<v8::Object>();
  }

  auto i = native_handler_map_.find(native_name);
  if (i == native_handler_map_.end()) {
    Fatal(context_,
          "Couldn't find native for requireNative(" + native_name + ")");
    return v8::MaybeLocal<v8::Object>();
  }

  if (!i->second->IsInitialized())
    i->second->Initialize();

  return i->second->NewInstance();
}

void ModuleSystem::LoadScript(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  std::string module_name = *v8::String::Utf8Value(GetIsolate(), args[0]);

  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::String> source =
      source_map_->GetSource(GetIsolate(), module_name);
  if (source.IsEmpty())
    Fatal(context_, "No source for loadScript(" + module_name + ")");

  v8::Local<v8::String> v8_module_name;
  if (!ToV8String(GetIsolate(), module_name.c_str(), &v8_module_name))
    Warn(GetIsolate(), "module_name is too long");

  RunString(source, v8_module_name);
  args.GetReturnValue().Set(v8::Undefined(GetIsolate()));
}

v8::Local<v8::String> ModuleSystem::WrapSource(v8::Local<v8::String> source) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  // Keep in order with the arguments in RequireForJsInner.
  v8::Local<v8::String> left = ToV8StringUnsafe(
      GetIsolate(),
      "(function(require, requireNative, loadScript, exports, console, "
      "privates, apiBridge, bindingUtil, getInternalApi, $Array, $Function, "
      "$JSON, $Object, $RegExp, $String, $Error, $Promise) {"
      "'use strict';");
  v8::Local<v8::String> right = ToV8StringUnsafe(GetIsolate(), "\n})");
  return handle_scope.Escape(v8::Local<v8::String>(v8::String::Concat(
      GetIsolate(), left, v8::String::Concat(GetIsolate(), source, right))));
}

void ModuleSystem::Private(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  if (!args[0]->IsObject() || args[0]->IsNull()) {
    GetIsolate()->ThrowException(v8::Exception::TypeError(ToV8StringUnsafe(
        GetIsolate(), args[0]->IsUndefined()
                          ? "Method called without a valid receiver (this). "
                            "Did you forget to call .bind()?"
                          : "Invalid invocation: receiver is not an object!")));
    return;
  }
  v8::Local<v8::Object> obj = args[0].As<v8::Object>();
  v8::Local<v8::Value> privates;
  if (!GetPrivate(obj, "privates", &privates) || !privates->IsObject()) {
    privates = v8::Object::New(args.GetIsolate());
    if (privates.IsEmpty()) {
      GetIsolate()->ThrowException(
          ToV8StringUnsafe(GetIsolate(), "Failed to create privates"));
      return;
    }
    v8::Maybe<bool> maybe =
        privates.As<v8::Object>()->SetPrototype(context()->v8_context(),
                                                v8::Null(args.GetIsolate()));
    CHECK(maybe.IsJust() && maybe.FromJust());
    SetPrivate(obj, "privates", privates);
  }
  args.GetReturnValue().Set(privates);
}

v8::Local<v8::Value> ModuleSystem::LoadModule(const std::string& module_name) {
  return LoadModuleWithNativeAPIBridge(module_name,
                                       v8::Undefined(GetIsolate()));
}

v8::Local<v8::Value> ModuleSystem::LoadModuleWithNativeAPIBridge(
    const std::string& module_name,
    v8::Local<v8::Value> api_bridge) {
  v8::EscapableHandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Context::Scope context_scope(v8_context);

  v8::Local<v8::String> source =
      source_map_->GetSource(GetIsolate(), module_name);
  if (source.IsEmpty()) {
    Fatal(context_, "No source for require(" + module_name + ")");
    return v8::Undefined(GetIsolate());
  }
  v8::Local<v8::String> wrapped_source(WrapSource(source));
  v8::Local<v8::String> v8_module_name;
  if (!ToV8String(GetIsolate(), module_name.c_str(), &v8_module_name)) {
    NOTREACHED_IN_MIGRATION() << "module_name is too long";
    return v8::Undefined(GetIsolate());
  }
  // Modules are wrapped in (function(){...}) so they always return functions.
  v8::Local<v8::Value> func_as_value =
      RunString(wrapped_source, v8_module_name);
  if (func_as_value.IsEmpty() || func_as_value->IsUndefined()) {
    Fatal(context_, "Bad source for require(" + module_name + ")");
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(func_as_value);

  v8::Local<v8::Object> exports = v8::Object::New(GetIsolate());

  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      GetIsolate(), &SetExportsProperty, v8::Local<v8::Value>(),
      v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kThrow);
  v8::Local<v8::String> v8_key;
  if (!ToV8String(GetIsolate(), "$set", &v8_key)) {
    NOTREACHED_IN_MIGRATION();
    return v8::Undefined(GetIsolate());
  }

  v8::Local<v8::Function> function;
  if (!tmpl->GetFunction(v8_context).ToLocal(&function)) {
    NOTREACHED_IN_MIGRATION();
    return v8::Undefined(GetIsolate());
  }

  exports->DefineOwnProperty(v8_context, v8_key, function, v8::ReadOnly)
      .FromJust();

  v8::Local<v8::Object> natives(NewInstance());
  CHECK(!natives.IsEmpty());  // this can fail if v8 has issues

  v8::Local<v8::Value> get_internal_api;
  if (get_internal_api_.IsEmpty()) {
    get_internal_api = v8::Undefined(GetIsolate());
  } else {
    get_internal_api = get_internal_api_.Get(GetIsolate())
                           ->GetFunction(v8_context)
                           .ToLocalChecked();
  }

  v8::Local<v8::Value> binding_util;
  if (!js_binding_util_getter_.is_null()) {
    js_binding_util_getter_.Run(v8_context, &binding_util);
    if (binding_util.IsEmpty()) {
      // The NativeExtensionBindingsSystem was destroyed. This shouldn't happen,
      // but JS makes the impossible possible!
      NOTREACHED_IN_MIGRATION();
      return v8::Undefined(GetIsolate());
    }
  } else {
    binding_util = v8::Undefined(GetIsolate());
  }

  // These must match the argument order in WrapSource.
  v8::Local<v8::Value> args[] = {
      // CommonJS.
      v8_helpers::GetPropertyUnsafe(v8_context, natives, "require",
                                    v8::NewStringType::kInternalized),
      v8_helpers::GetPropertyUnsafe(v8_context, natives, "requireNative",
                                    v8::NewStringType::kInternalized),
      v8_helpers::GetPropertyUnsafe(v8_context, natives, "loadScript",
                                    v8::NewStringType::kInternalized),
      exports,
      // Libraries that we magically expose to every module.
      console::AsV8Object(GetIsolate()),
      v8_helpers::GetPropertyUnsafe(v8_context, natives, "privates",
                                    v8::NewStringType::kInternalized),
      api_bridge,        // exposed as apiBridge.
      binding_util,      // exposed as bindingUtil.
      get_internal_api,  // exposed as getInternalApi.
      // Each safe builtin. Keep in order with the arguments in WrapSource.
      context_->safe_builtins()->GetArray(),
      context_->safe_builtins()->GetFunction(),
      context_->safe_builtins()->GetJSON(),
      context_->safe_builtins()->GetObjekt(),
      context_->safe_builtins()->GetRegExp(),
      context_->safe_builtins()->GetString(),
      context_->safe_builtins()->GetError(),
      context_->safe_builtins()->GetPromise(),
  };
  {
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetCaptureMessage(true);
    context_->SafeCallFunction(func, std::size(args), args);
    if (try_catch.HasCaught()) {
      HandleException(try_catch);
      return v8::Undefined(GetIsolate());
    }
  }
  return handle_scope.Escape(exports);
}

void ModuleSystem::ClobberExistingNativeHandler(const std::string& name) {
  auto existing_handler = native_handler_map_.find(name);
  if (existing_handler != native_handler_map_.end()) {
    clobbered_native_handlers_.push_back(std::move(existing_handler->second));
    native_handler_map_.erase(existing_handler);
  }
}

v8::Local<v8::Function> ModuleSystem::GetModuleFunction(
    const std::string& module_name,
    const std::string& method_name) {
  v8::Local<v8::String> v8_module_name;
  v8::Local<v8::String> v8_method_name;
  if (!ToV8String(GetIsolate(), module_name.c_str(), &v8_module_name) ||
      !ToV8String(GetIsolate(), method_name.c_str(), &v8_method_name)) {
    return v8::Local<v8::Function>();
  }

  v8::Local<v8::Value> module;
  // Important: don't create the module if it doesn't exist. Doing so would
  // force a call into JS, which is something we want to avoid in case it has
  // been suspended. Additionally, we should only be calling module methods for
  // modules that have been instantiated.
  bool create = false;
  module = RequireForJsInner(v8_module_name, create);

  // RequireForJsInner() returns Undefined in the case of a module not being
  // loaded, since we don't create it here.
  if (!module.IsEmpty() && module->IsUndefined())
    return v8::Local<v8::Function>();

  if (module.IsEmpty() || !module->IsObject()) {
    Fatal(context_,
          "Failed to get module " + module_name + " to call " + method_name);
    return v8::Local<v8::Function>();
  }

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(module);
  v8::Local<v8::Value> value;
  if (!v8_helpers::GetProperty(context()->v8_context(), object, v8_method_name,
                               &value) ||
      !value->IsFunction()) {
    Fatal(context_, module_name + "." + method_name + " is not a function");
    return v8::Local<v8::Function>();
  }

  return v8::Local<v8::Function>::Cast(value);
}

}  // namespace extensions
