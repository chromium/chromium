// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/runtime_hooks_delegate.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/converter.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace extensions {

namespace {
using RequestResult = APIBindingHooks::RequestResult;

// Handler for the extensionId property on chrome.runtime.
void GetExtensionId(v8::Local<v8::Name> property_name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      info.Holder()->GetCreationContextChecked(isolate);

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  // This could potentially be invoked after the script context is removed
  // (unlike the handler calls, which should only be invoked for valid
  // contexts).
  if (script_context && script_context->extension()) {
    info.GetReturnValue().Set(
        gin::StringToSymbol(isolate, script_context->extension()->id()));
  }
}

// Handler for the dynamicId property on chrome.runtime.
void GetDynamicId(v8::Local<v8::Name> property_name,
                  const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      info.Holder()->GetCreationContextChecked(isolate);

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  // This could potentially be invoked after the script context is removed
  // (unlike the handler calls, which should only be invoked for valid
  // contexts).
  if (script_context && script_context->extension()) {
    info.GetReturnValue().Set(
        gin::StringToSymbol(isolate, script_context->extension()->guid()));
  }
}

void EmptySetter(v8::Local<v8::Name> name,
                 v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<void>& info) {
  // Empty setter is required to keep the native data property in "accessor"
  // state even in case the value is updated by user code.
}

constexpr char kGetManifest[] = "runtime.getManifest";
constexpr char kGetURL[] = "runtime.getURL";
constexpr char kConnect[] = "runtime.connect";
constexpr char kConnectNative[] = "runtime.connectNative";
constexpr char kSendMessage[] = "runtime.sendMessage";
constexpr char kSendNativeMessage[] = "runtime.sendNativeMessage";
constexpr char kGetBackgroundPage[] = "runtime.getBackgroundPage";
constexpr char kGetPackageDirectoryEntry[] = "runtime.getPackageDirectoryEntry";
constexpr char kRequestUpdateCheck[] = "runtime.requestUpdateCheck";

// The custom callback supplied to runtime.getBackgroundPage to find and return
// the background page to the original callback.
void GetBackgroundPageCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      info.This()->GetCreationContextChecked(isolate);

  // Custom callbacks are called with the arguments of the callback function and
  // the response from the API. Since the custom callback here handles all the
  // logic we should have just received the callback function to call with the
  // result.
  DCHECK_EQ(1, info.Length());
  CHECK(info[0]->IsFunction());

  // The ScriptContext should always be valid, because otherwise the
  // getBackgroundPage() request should have been invalidated (and this should
  // never run).
  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  v8::Local<v8::Value> background_page =
      ExtensionFrameHelper::GetV8BackgroundPageMainFrame(
          isolate, script_context->extension()->id());
  v8::Local<v8::Value> args[] = {background_page};
  script_context->SafeCallFunction(info[0].As<v8::Function>(), std::size(args),
                                   args);
}

// Function used by runtime.requestUpdateCheck to modify the arguments returned
// for an asynchronous request, splitting the returned object into separate
// arguments for callback based requests, with the version wrapped in a details
// object.
// Note: This is to allow the promise version of the API to return a single
// object, while still supporting the previous callback version which expects
// multiple parameters to be passed to the callback.
v8::LocalVector<v8::Value> MassageRequestUpdateCheckResults(
    const v8::LocalVector<v8::Value>& result_args,
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type) {
  // If this is not a callback based API call, we don't need to modify anything.
  if (async_type != binding::AsyncResponseType::kCallback) {
    return result_args;
  }

  DCHECK_EQ(1u, result_args.size());
  DCHECK(result_args[0]->IsObject());
  v8::Local<v8::Object> result_obj = result_args[0].As<v8::Object>();

  // The object sent back has two properties on it which we need to split into
  // two separate arguments.
  v8::Local<v8::Value> status;
  bool success =
      v8_helpers::GetProperty(context, result_obj, "status", &status);
  DCHECK(success);
  v8::Local<v8::Value> version;
  success = v8_helpers::GetProperty(context, result_obj, "version", &version);
  DCHECK(success);

  // Version is wrapped as a parameter on a details object.
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> details = v8::Object::New(isolate);
  auto key = gin::StringToV8(isolate, "version");
  details->CreateDataProperty(context, key, version).Check();
  return v8::LocalVector<v8::Value>(isolate, {status, details});
}

}  // namespace

RuntimeHooksDelegate::RuntimeHooksDelegate(
    NativeRendererMessagingService* messaging_service)
    : messaging_service_(messaging_service) {}
RuntimeHooksDelegate::~RuntimeHooksDelegate() = default;

// static
RequestResult RuntimeHooksDelegate::GetURL(
    ScriptContext* script_context,
    const v8::LocalVector<v8::Value>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  DCHECK(script_context->extension());

  v8::Isolate* isolate = script_context->isolate();
  std::string path = gin::V8ToString(isolate, arguments[0]);
  const auto* extension = script_context->extension();
  bool use_dynamic_url = false;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionDynamicURLRedirection)) {
    use_dynamic_url =
        WebAccessibleResourcesInfo::ShouldUseDynamicUrl(extension, path);
  }
  std::string id = use_dynamic_url ? extension->guid() : extension->id();

  RequestResult result(RequestResult::HANDLED);
  std::string url = base::StringPrintf(
      "chrome-extension://%s%s%s", id.c_str(),
      !path.empty() && path[0] == '/' ? "" : "/", path.c_str());
  // GURL considers any possible path valid. Since the argument is only appended
  // as part of the path, there should be no way this could conceivably fail.
  DCHECK(GURL(url).is_valid());
  result.return_value = gin::StringToV8(isolate, url);
  return result;
}

RequestResult RuntimeHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (RuntimeHooksDelegate::*)(
      ScriptContext*, const APISignature::V8ParseResult&);
  static const struct {
    Handler handler;
    std::string_view method;
  } kHandlers[] = {
      {&RuntimeHooksDelegate::HandleSendMessage, kSendMessage},
      {&RuntimeHooksDelegate::HandleConnect, kConnect},
      {&RuntimeHooksDelegate::HandleGetURL, kGetURL},
      {&RuntimeHooksDelegate::HandleGetManifest, kGetManifest},
      {&RuntimeHooksDelegate::HandleConnectNative, kConnectNative},
      {&RuntimeHooksDelegate::HandleSendNativeMessage, kSendNativeMessage},
      {&RuntimeHooksDelegate::HandleGetBackgroundPage, kGetBackgroundPage},
      {&RuntimeHooksDelegate::HandleGetPackageDirectoryEntryCallback,
       kGetPackageDirectoryEntry},
      {&RuntimeHooksDelegate::HandleRequestUpdateCheck, kRequestUpdateCheck},
  };

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  bool should_massage = false;
  bool allow_options = false;
  if (method_name == kSendMessage) {
    should_massage = true;
    allow_options = true;
  } else if (method_name == kSendNativeMessage) {
    should_massage = true;
  }

  if (should_massage) {
    messaging_util::MassageSendMessageArguments(context->GetIsolate(),
                                                allow_options, arguments);
  }

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  return (this->*handler)(script_context, parse_result);
}

void RuntimeHooksDelegate::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  object_template->SetNativeDataProperty(gin::StringToSymbol(isolate, "id"),
                                         &GetExtensionId, &EmptySetter);
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionDynamicURLRedirection)) {
    object_template->SetNativeDataProperty(
        gin::StringToSymbol(isolate, "dynamicId"), &GetDynamicId, &EmptySetter);
  }
}

RequestResult RuntimeHooksDelegate::HandleGetManifest(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  CHECK(script_context->extension());
  CHECK(script_context->extension()->manifest());
  CHECK(script_context->extension()->manifest()->value());

  RequestResult result(RequestResult::HANDLED);
  result.return_value = content::V8ValueConverter::Create()->ToV8Value(
      *script_context->extension()->manifest()->value(),
      script_context->v8_context());

  return result;
}

RequestResult RuntimeHooksDelegate::HandleGetURL(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  return GetURL(script_context, *parse_result.arguments);
}

RequestResult RuntimeHooksDelegate::HandleSendMessage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(4u, arguments.size());

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "runtime.sendMessage", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Context> v8_context = script_context->v8_context();

  v8::Local<v8::Value> v8_message = arguments[1];
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      v8_context, v8_message,
      messaging_util::GetSerializationFormat(*script_context), &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  // Note: arguments[2] is the options argument. However, the only available
  // option for sendMessage() is includeTlsChannelId. That option has no effect
  // since M72, but it is still part of the public spec for compatibility and is
  // parsed into |arguments|. See crbug.com/1045232.

  v8::Local<v8::Function> response_callback;
  if (!arguments[3]->IsNull())
    response_callback = arguments[3].As<v8::Function>();

  v8::Local<v8::Promise> promise = messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForExtension(target_id),
      mojom::ChannelType::kSendMessage, *message, parse_result.async_type,
      response_callback);
  DCHECK_EQ(parse_result.async_type == binding::AsyncResponseType::kPromise,
            !promise.IsEmpty())
      << "SendOneTimeMessage should only return a Promise for promise based "
         "API calls, otherwise it should be empty";

  RequestResult result(RequestResult::HANDLED);
  if (parse_result.async_type == binding::AsyncResponseType::kPromise) {
    result.return_value = promise;
  }
  return result;
}

RequestResult RuntimeHooksDelegate::HandleSendNativeMessage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(3u, arguments.size());

  std::string application_name =
      gin::V8ToString(script_context->isolate(), arguments[0]);

  v8::Local<v8::Value> v8_message = arguments[1];
  DCHECK(!v8_message.IsEmpty());
  std::string error;

  // Native messaging always uses JSON since a native host doesn't understand
  // structured cloning serialization.
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(script_context->v8_context(), v8_message,
                                    mojom::SerializationFormat::kJson, &error);
  if (!message) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  v8::Local<v8::Function> response_callback;
  if (!arguments[2]->IsNull())
    response_callback = arguments[2].As<v8::Function>();

  v8::Local<v8::Promise> promise = messaging_service_->SendOneTimeMessage(
      script_context, MessageTarget::ForNativeApp(application_name),
      mojom::ChannelType::kNative, *message, parse_result.async_type,
      response_callback);
  DCHECK_EQ(parse_result.async_type == binding::AsyncResponseType::kPromise,
            !promise.IsEmpty())
      << "SendOneTimeMessage should only return a Promise for promise based "
         "API calls, otherwise it should be empty";

  RequestResult result(RequestResult::HANDLED);
  if (parse_result.async_type == binding::AsyncResponseType::kPromise) {
    result.return_value = promise;
  }
  return result;
}

RequestResult RuntimeHooksDelegate::HandleConnect(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(2u, arguments.size());
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);

  std::string target_id;
  std::string error;
  if (!messaging_util::GetTargetExtensionId(script_context, arguments[0],
                                            "runtime.connect", &target_id,
                                            &error)) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(error);
    return result;
  }

  messaging_util::MessageOptions options;
  if (!arguments[1]->IsNull()) {
    options = messaging_util::ParseMessageOptions(
        script_context->v8_context(), arguments[1].As<v8::Object>(),
        messaging_util::PARSE_CHANNEL_NAME);
  }

  gin::Handle<GinPort> port = messaging_service_->Connect(
      script_context, MessageTarget::ForExtension(target_id),
      options.channel_name,
      messaging_util::GetSerializationFormat(*script_context));
  DCHECK(!port.IsEmpty());
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);

  RequestResult result(RequestResult::HANDLED);
  result.return_value = port.ToV8();
  return result;
}

RequestResult RuntimeHooksDelegate::HandleConnectNative(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);

  std::string application_name =
      gin::V8ToString(script_context->isolate(), arguments[0]);

  // Native messaging always uses JSON since a native host doesn't understand
  // structured cloning serialization.
  auto format = mojom::SerializationFormat::kJson;
  gin::Handle<GinPort> port = messaging_service_->Connect(
      script_context, MessageTarget::ForNativeApp(application_name),
      std::string(), format);

  RequestResult result(RequestResult::HANDLED);
  result.return_value = port.ToV8();
  return result;
}

RequestResult RuntimeHooksDelegate::HandleGetBackgroundPage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  DCHECK(script_context->extension());

  RequestResult result(RequestResult::NOT_HANDLED);
  if (!v8::Function::New(script_context->v8_context(),
                         &GetBackgroundPageCallback)
           .ToLocal(&result.custom_callback)) {
    return RequestResult(RequestResult::THROWN);
  }

  return result;
}

RequestResult RuntimeHooksDelegate::HandleGetPackageDirectoryEntryCallback(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  // TODO(devlin): This is basically just copied and translated from
  // the JS bindings, and still relies on the custom JS bindings for
  // getBindDirectoryEntryCallback. This entire API is a bit crazy, and needs
  // some help.
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> v8_context = script_context->v8_context();

  v8::MaybeLocal<v8::Value> maybe_custom_callback;
  {  // Begin natives enabled scope (for requiring the module).
    ModuleSystem::NativesEnabledScope enable_natives(
        script_context->module_system());
    content::RenderFrame* background_page =
        ExtensionFrameHelper::GetBackgroundPageFrame(
            script_context->extension()->id());

    // The JS function will sometimes use the background page's context to do
    // some work (see also
    // extensions/renderer/resources/file_entry_binding_util.js).  In order to
    // allow native code to run in the background page, we'll also need a
    // NativesEnabledScope for that context.
    DCHECK(v8_context == isolate->GetCurrentContext());
    std::optional<ModuleSystem::NativesEnabledScope> background_page_natives;
    if (background_page &&
        background_page != script_context->GetRenderFrame() &&
        blink::WebFrame::ScriptCanAccess(isolate,
                                         background_page->GetWebFrame())) {
      ScriptContext* background_page_script_context =
          GetScriptContextFromV8Context(
              background_page->GetWebFrame()->MainWorldScriptContext());
      if (background_page_script_context) {
        background_page_natives.emplace(
            background_page_script_context->module_system());
      }
    }

    v8::Local<v8::Object> file_entry_binding_util;
    // ModuleSystem::Require can return an empty Maybe when it fails for any
    // number of reasons. It *shouldn't* ever throw, but it is technically
    // possible. This makes the handling the failure result complicated. Since
    // this shouldn't happen at all, bail and consider it handled if it fails.
    if (!script_context->module_system()
             ->Require("fileEntryBindingUtil")
             .ToLocal(&file_entry_binding_util)) {
      NOTREACHED_IN_MIGRATION();
      // Abort, and consider the request handled.
      return RequestResult(RequestResult::HANDLED);
    }

    v8::Local<v8::Value> get_bind_directory_entry_callback_value;
    if (!file_entry_binding_util
             ->Get(v8_context, gin::StringToSymbol(
                                   isolate, "getBindDirectoryEntryCallback"))
             .ToLocal(&get_bind_directory_entry_callback_value)) {
      NOTREACHED_IN_MIGRATION();
      return RequestResult(RequestResult::THROWN);
    }

    if (!get_bind_directory_entry_callback_value->IsFunction()) {
      NOTREACHED_IN_MIGRATION();
      // Abort, and consider the request handled.
      return RequestResult(RequestResult::HANDLED);
    }

    v8::Local<v8::Function> get_bind_directory_entry_callback =
        get_bind_directory_entry_callback_value.As<v8::Function>();

    maybe_custom_callback =
        JSRunner::Get(v8_context)
            ->RunJSFunctionSync(get_bind_directory_entry_callback, v8_context,
                                0, nullptr);
  }  // End modules enabled scope.
  v8::Local<v8::Value> callback;
  if (!maybe_custom_callback.ToLocal(&callback)) {
    NOTREACHED_IN_MIGRATION();
    return RequestResult(RequestResult::THROWN);
  }

  if (!callback->IsFunction()) {
    NOTREACHED_IN_MIGRATION();
    // Abort, and consider the request handled.
    return RequestResult(RequestResult::HANDLED);
  }

  RequestResult result(RequestResult::NOT_HANDLED);
  result.custom_callback = callback.As<v8::Function>();
  return result;
}

RequestResult RuntimeHooksDelegate::HandleRequestUpdateCheck(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  return RequestResult(RequestResult::NOT_HANDLED,
                       v8::Local<v8::Function>() /*custom_callback*/,
                       base::BindOnce(MassageRequestUpdateCheckResults));
}

}  // namespace extensions
