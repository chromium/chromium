// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/web_request_hooks.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/converter.h"

namespace extensions {

WebRequestHooks::WebRequestHooks() {}
WebRequestHooks::~WebRequestHooks() = default;

bool WebRequestHooks::CreateCustomEvent(
    v8::Local<v8::Context> context,
    const std::string& event_name,
    v8::Local<v8::Value>* event_out) {
  // Don't create a custom event for the "onActionIgnored" event.
  if (event_name == api::web_request::OnActionIgnored::kEventName)
    return false;

  v8::Isolate* isolate = context->GetIsolate();

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  v8::Local<v8::Object> internal_bindings;
  {
    ModuleSystem::NativesEnabledScope enable_natives(
        script_context->module_system());
    if (!script_context->module_system()
             ->Require("webRequestEvent")
             .ToLocal(&internal_bindings)) {
      return false;
    }
  }

  v8::Local<v8::Value> get_event_value;
  {
    v8::TryCatch try_catch(isolate);
    if (!internal_bindings
             ->Get(context,
                   gin::StringToSymbol(isolate, "createWebRequestEvent"))
             .ToLocal(&get_event_value) ||
        !get_event_value->IsFunction()) {
      NOTREACHED();
      return false;
    }
  }

  // The JS validates that the extra parameters passed to the web request event
  // match the expected schema. We need to initialize the event with that
  // schema.
  const base::DictionaryValue* event_spec =
      ExtensionAPI::GetSharedInstance()->GetSchema(event_name);
  DCHECK(event_spec);
  const base::ListValue* extra_params = nullptr;
  CHECK(event_spec->GetList("extraParameters", &extra_params));
  v8::Local<v8::Value> extra_parameters_spec =
      content::V8ValueConverter::Create()->ToV8Value(extra_params, context);

  v8::Local<v8::Function> get_event = get_event_value.As<v8::Function>();
  v8::Local<v8::Value> args[] = {
      gin::StringToSymbol(isolate, event_name),
      v8::Undefined(isolate),  // opt_argSchemas are ignored.
      extra_parameters_spec,
      // opt_eventOptions and opt_webViewInstanceId are ignored.
  };

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> event;
  if (!JSRunner::Get(context)
           ->RunJSFunctionSync(get_event, context, base::size(args), args)
           .ToLocal(&event)) {
    // TODO(devlin): Do we care about the error? In theory, this should never
    // happen, so probably not.
    event = v8::Undefined(isolate);
  }

  *event_out = event;
  return true;
}

}  // namespace extensions
