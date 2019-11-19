// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_js_util.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "extensions/renderer/bindings/declarative_event.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"

namespace extensions {

gin::WrapperInfo APIBindingJSUtil::kWrapperInfo = {gin::kEmbedderNativeGin};

APIBindingJSUtil::APIBindingJSUtil(APITypeReferenceMap* type_refs,
                                   APIRequestHandler* request_handler,
                                   APIEventHandler* event_handler,
                                   ExceptionHandler* exception_handler)
    : type_refs_(type_refs),
      request_handler_(request_handler),
      event_handler_(event_handler),
      exception_handler_(exception_handler) {}

APIBindingJSUtil::~APIBindingJSUtil() {}

gin::ObjectTemplateBuilder APIBindingJSUtil::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<APIBindingJSUtil>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendRequest", &APIBindingJSUtil::SendRequest)
      .SetMethod("registerEventArgumentMassager",
                 &APIBindingJSUtil::RegisterEventArgumentMassager)
      .SetMethod("createCustomEvent", &APIBindingJSUtil::CreateCustomEvent)
      .SetMethod("createCustomDeclarativeEvent",
                 &APIBindingJSUtil::CreateCustomDeclarativeEvent)
      .SetMethod("invalidateEvent", &APIBindingJSUtil::InvalidateEvent)
      .SetMethod("setLastError", &APIBindingJSUtil::SetLastError)
      .SetMethod("clearLastError", &APIBindingJSUtil::ClearLastError)
      .SetMethod("hasLastError", &APIBindingJSUtil::HasLastError)
      .SetMethod("runCallbackWithLastError",
                 &APIBindingJSUtil::RunCallbackWithLastError)
      .SetMethod("handleException", &APIBindingJSUtil::HandleException)
      .SetMethod("setExceptionHandler", &APIBindingJSUtil::SetExceptionHandler)
      .SetMethod("validateType", &APIBindingJSUtil::ValidateType)
      .SetMethod("validateCustomSignature",
                 &APIBindingJSUtil::ValidateCustomSignature)
      .SetMethod("addCustomSignature", &APIBindingJSUtil::AddCustomSignature);
}

void APIBindingJSUtil::SendRequest(
    gin::Arguments* arguments,
    const std::string& name,
    const std::vector<v8::Local<v8::Value>>& request_args,
    v8::Local<v8::Value> options) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  const APISignature* signature = type_refs_->GetAPIMethodSignature(name);
  DCHECK(signature);

  v8::Local<v8::Function> custom_callback;
  if (!options.IsEmpty() && !options->IsUndefined() && !options->IsNull()) {
    if (!options->IsObject()) {
      NOTREACHED();
      return;
    }
    v8::Local<v8::Object> options_obj = options.As<v8::Object>();
    if (!options_obj->GetPrototype()->IsNull()) {
      NOTREACHED();
      return;
    }
    gin::Dictionary options_dict(isolate, options_obj);
    // NOTE: We don't throw any errors here if customCallback is of an invalid
    // type. We could, if we wanted to be a bit more verbose.
    options_dict.Get("customCallback", &custom_callback);
  }

  std::unique_ptr<base::ListValue> converted_arguments;
  v8::Local<v8::Function> callback;

  // Some APIs (like fileSystem and contextMenus) don't provide arguments that
  // match the expected schema. For now, we need to ignore these and trust the
  // JS gives us something we expect.
  // TODO(devlin): We should ideally always be able to validate these, meaning
  // that we either need to make the APIs give us the expected signature, or
  // need to have a way of indicating an internal signature.
  APISignature::JSONParseResult parse_result =
      signature->ConvertArgumentsIgnoringSchema(context, request_args);
  CHECK(parse_result.succeeded());

  request_handler_->StartRequest(context, name,
                                 std::move(parse_result.arguments),
                                 parse_result.callback, custom_callback);
}

void APIBindingJSUtil::RegisterEventArgumentMassager(
    gin::Arguments* arguments,
    const std::string& event_name,
    v8::Local<v8::Function> massager) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  event_handler_->RegisterArgumentMassager(context, event_name, massager);
}

void APIBindingJSUtil::CreateCustomEvent(gin::Arguments* arguments,
                                         v8::Local<v8::Value> v8_event_name,
                                         bool supports_filters,
                                         bool supports_lazy_listeners) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  std::string event_name;
  if (!v8_event_name->IsUndefined()) {
    if (!v8_event_name->IsString()) {
      NOTREACHED();
      return;
    }
    event_name = gin::V8ToString(isolate, v8_event_name);
  }

  DCHECK(!supports_filters || !event_name.empty())
      << "Events that support filters cannot be anonymous.";
  DCHECK(!supports_lazy_listeners || !event_name.empty())
      << "Events that support lazy listeners cannot be anonymous.";

  v8::Local<v8::Value> event;
  if (event_name.empty()) {
    event = event_handler_->CreateAnonymousEventInstance(context);
  } else {
    bool notify_on_change = true;
    event = event_handler_->CreateEventInstance(
        event_name, supports_filters, supports_lazy_listeners,
        binding::kNoListenerMax, notify_on_change, context);
  }

  arguments->Return(event);
}

void APIBindingJSUtil::CreateCustomDeclarativeEvent(
    gin::Arguments* arguments,
    const std::string& event_name,
    const std::vector<std::string>& actions_list,
    const std::vector<std::string>& conditions_list,
    int webview_instance_id) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  gin::Handle<DeclarativeEvent> event = gin::CreateHandle(
      isolate,
      new DeclarativeEvent(event_name, type_refs_, request_handler_,
                           actions_list, conditions_list, webview_instance_id));

  arguments->Return(event.ToV8());
}

void APIBindingJSUtil::InvalidateEvent(gin::Arguments* arguments,
                                       v8::Local<v8::Object> event) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  event_handler_->InvalidateCustomEvent(arguments->GetHolderCreationContext(),
                                        event);
}

void APIBindingJSUtil::SetLastError(gin::Arguments* arguments,
                                    const std::string& error) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  request_handler_->last_error()->SetError(
      arguments->GetHolderCreationContext(), error);
}

void APIBindingJSUtil::ClearLastError(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  bool report_if_unchecked = false;
  request_handler_->last_error()->ClearError(
      arguments->GetHolderCreationContext(), report_if_unchecked);
}

void APIBindingJSUtil::HasLastError(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  bool has_last_error = request_handler_->last_error()->HasError(
      arguments->GetHolderCreationContext());
  arguments->Return(has_last_error);
}

void APIBindingJSUtil::RunCallbackWithLastError(
    gin::Arguments* arguments,
    const std::string& error,
    v8::Local<v8::Function> callback) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  request_handler_->last_error()->SetError(context, error);
  JSRunner::Get(context)->RunJSFunction(callback, context, 0, nullptr);

  bool report_if_unchecked = true;
  request_handler_->last_error()->ClearError(context, report_if_unchecked);
}

void APIBindingJSUtil::HandleException(gin::Arguments* arguments,
                                       const std::string& message,
                                       v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  std::string full_message;
  if (!exception->IsUndefined() && !exception->IsNull()) {
    v8::TryCatch try_catch(isolate);
    std::string exception_string;
    v8::Local<v8::String> v8_exception_string;
    if (exception->ToString(context).ToLocal(&v8_exception_string))
      exception_string = gin::V8ToString(isolate, v8_exception_string);
    else
      exception_string = "(failed to get error message)";
    full_message =
        base::StringPrintf("%s: %s", message.c_str(), exception_string.c_str());
  } else {
    full_message = message;
  }

  exception_handler_->HandleException(context, full_message, exception);
}

void APIBindingJSUtil::SetExceptionHandler(gin::Arguments* arguments,
                                           v8::Local<v8::Function> handler) {
  exception_handler_->SetHandlerForContext(
      arguments->GetHolderCreationContext(), handler);
}

void APIBindingJSUtil::ValidateType(gin::Arguments* arguments,
                                    const std::string& type_name,
                                    v8::Local<v8::Value> value) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  const ArgumentSpec* spec = type_refs_->GetSpec(type_name);
  if (!spec) {
    // We shouldn't be asked to validate unknown specs, but since this comes
    // from JS, assume nothing.
    NOTREACHED();
    return;
  }

  std::string error;
  if (!spec->ParseArgument(context, value, *type_refs_, nullptr, nullptr,
                           &error)) {
    arguments->ThrowTypeError(error);
  }
}

void APIBindingJSUtil::AddCustomSignature(
    gin::Arguments* arguments,
    const std::string& custom_signature_name,
    v8::Local<v8::Value> signature) {
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  // JS bindings run per-context, so it's expected that they will call this
  // multiple times in the lifetime of the renderer process. Only handle the
  // first call.
  if (type_refs_->GetCustomSignature(custom_signature_name))
    return;

  if (!signature->IsArray()) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<base::Value> base_signature =
      content::V8ValueConverter::Create()->FromV8Value(signature, context);
  if (!base_signature->is_list()) {
    NOTREACHED();
    return;
  }

  type_refs_->AddCustomSignature(
      custom_signature_name,
      std::make_unique<APISignature>(
          *base::ListValue::From(std::move(base_signature))));
}

void APIBindingJSUtil::ValidateCustomSignature(
    gin::Arguments* arguments,
    const std::string& custom_signature_name,
    v8::Local<v8::Value> arguments_to_validate) {
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  const APISignature* signature =
      type_refs_->GetCustomSignature(custom_signature_name);
  if (!signature) {
    NOTREACHED();
  }

  std::vector<v8::Local<v8::Value>> vector_arguments;
  if (!gin::ConvertFromV8(isolate, arguments_to_validate, &vector_arguments)) {
    NOTREACHED();
    return;
  }

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, vector_arguments, *type_refs_);
  if (!parse_result.succeeded()) {
    arguments->ThrowTypeError(std::move(*parse_result.error));
  }
}

}  // namespace extensions
