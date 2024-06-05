// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_hooks.h"

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/arguments.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"
#include "gin/wrappable.h"

namespace extensions {

namespace {

// An interface to allow for registration of custom hooks from JavaScript.
// Contains registered hooks for a single API.
class JSHookInterface final : public gin::Wrappable<JSHookInterface> {
 public:
  explicit JSHookInterface(const std::string& api_name)
      : api_name_(api_name) {}

  JSHookInterface(const JSHookInterface&) = delete;
  JSHookInterface& operator=(const JSHookInterface&) = delete;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<JSHookInterface>::GetObjectTemplateBuilder(isolate)
        .SetMethod("setHandleRequest", &JSHookInterface::SetHandleRequest)
        .SetMethod("setUpdateArgumentsPreValidate",
                   &JSHookInterface::SetUpdateArgumentsPreValidate)
        .SetMethod("setUpdateArgumentsPostValidate",
                   &JSHookInterface::SetUpdateArgumentsPostValidate)
        .SetMethod("setCustomCallback", &JSHookInterface::SetCustomCallback);
  }

  void ClearHooks() {
    handle_request_hooks_.clear();
    pre_validation_hooks_.clear();
    post_validation_hooks_.clear();
  }

  v8::Local<v8::Function> GetHandleRequestHook(const std::string& method_name,
                                               v8::Isolate* isolate) const {
    return GetHookFromMap(handle_request_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetPreValidationHook(const std::string& method_name,
                                               v8::Isolate* isolate) const {
    return GetHookFromMap(pre_validation_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetPostValidationHook(const std::string& method_name,
                                                v8::Isolate* isolate) const {
    return GetHookFromMap(post_validation_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetCustomCallback(const std::string& method_name,
                                            v8::Isolate* isolate) const {
    return GetHookFromMap(custom_callback_hooks_, method_name, isolate);
  }

 private:
  using JSHooks = std::map<std::string, v8::Global<v8::Function>>;

  v8::Local<v8::Function> GetHookFromMap(const JSHooks& map,
                                         const std::string& method_name,
                                         v8::Isolate* isolate) const {
    auto iter = map.find(method_name);
    if (iter == map.end())
      return v8::Local<v8::Function>();
    return iter->second.Get(isolate);
  }

  void AddHookToMap(JSHooks* map,
                    v8::Isolate* isolate,
                    const std::string& method_name,
                    v8::Local<v8::Function> hook) {
    std::string qualified_method_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), method_name.c_str());
    v8::Global<v8::Function>& entry = (*map)[qualified_method_name];
    if (!entry.IsEmpty()) {
      NOTREACHED_IN_MIGRATION() << "Hooks can only be set once.";
      return;
    }
    entry.Reset(isolate, hook);
  }

  // Adds a hook to handle the implementation of the API method.
  void SetHandleRequest(v8::Isolate* isolate,
                        const std::string& method_name,
                        v8::Local<v8::Function> hook) {
    AddHookToMap(&handle_request_hooks_, isolate, method_name, hook);
  }

  // Adds a hook to update the arguments passed to the API method before we do
  // any kind of validation.
  void SetUpdateArgumentsPreValidate(v8::Isolate* isolate,
                                     const std::string& method_name,
                                     v8::Local<v8::Function> hook) {
    AddHookToMap(&pre_validation_hooks_, isolate, method_name, hook);
  }

  void SetUpdateArgumentsPostValidate(v8::Isolate* isolate,
                                      const std::string& method_name,
                                      v8::Local<v8::Function> hook) {
    AddHookToMap(&post_validation_hooks_, isolate, method_name, hook);
  }

  void SetCustomCallback(v8::Isolate* isolate,
                         const std::string& method_name,
                         v8::Local<v8::Function> hook) {
    AddHookToMap(&custom_callback_hooks_, isolate, method_name, hook);
  }

  std::string api_name_;

  JSHooks handle_request_hooks_;
  JSHooks pre_validation_hooks_;
  JSHooks post_validation_hooks_;
  JSHooks custom_callback_hooks_;
};

const char kExtensionAPIHooksPerContextKey[] = "extension_api_hooks";

struct APIHooksPerContextData : public base::SupportsUserData::Data {
  APIHooksPerContextData(v8::Isolate* isolate) : isolate(isolate) {}
  ~APIHooksPerContextData() override {
    v8::HandleScope scope(isolate);
    for (const auto& pair : hook_interfaces) {
      // We explicitly clear the hook data map here to remove all references to
      // v8 objects in order to avoid cycles.
      JSHookInterface* hooks = nullptr;
      gin::Converter<JSHookInterface*>::FromV8(
          isolate, pair.second.Get(isolate), &hooks);
      CHECK(hooks);
      hooks->ClearHooks();
    }
  }

  raw_ptr<v8::Isolate> isolate;

  std::map<std::string, v8::Global<v8::Object>> hook_interfaces;

  // For handle request hooks which need to be resolved asynchronously, we store
  // a map of the associated request IDs to the callbacks that will be used to
  // resolve them.
  using ActiveRequest = base::OnceCallback<void(bool, gin::Arguments*)>;
  std::map<int, ActiveRequest> active_requests;
};

gin::WrapperInfo JSHookInterface::kWrapperInfo =
    {gin::kEmbedderNativeGin};

// Gets the v8::Object of the JSHookInterface, optionally creating it if it
// doesn't exist.
v8::Local<v8::Object> GetJSHookInterfaceObject(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    bool should_create) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  if (!data) {
    if (!should_create)
      return v8::Local<v8::Object>();

    auto api_data =
        std::make_unique<APIHooksPerContextData>(context->GetIsolate());
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIHooksPerContextKey,
                                  std::move(api_data));
  }

  auto iter = data->hook_interfaces.find(api_name);
  if (iter != data->hook_interfaces.end())
    return iter->second.Get(context->GetIsolate());

  if (!should_create)
    return v8::Local<v8::Object>();

  gin::Handle<JSHookInterface> hooks =
      gin::CreateHandle(context->GetIsolate(), new JSHookInterface(api_name));
  CHECK(!hooks.IsEmpty());
  v8::Local<v8::Object> hooks_object = hooks.ToV8().As<v8::Object>();
  data->hook_interfaces[api_name].Reset(context->GetIsolate(), hooks_object);

  return hooks_object;
}

// Helper function used when completing requests for handle request hooks that
// had an associated asynchronous response expected.
void CompleteHandleRequestHelper(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    bool did_succeed) {
  gin::Arguments args(info);
  v8::Local<v8::Context> context = args.isolate()->GetCurrentContext();
  if (!binding::IsContextValid(context))
    return;
  int request_id = 0;
  bool got_request_id = args.GetData(&request_id);
  DCHECK(got_request_id);

  // The callback to complete the request is stored in a map on the
  // APIHooksPerContextData associated with the id of the request.
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  DCHECK(data) << "APIHooks PerContextData should always exist if we have an "
                  "active request";

  auto iter = data->active_requests.find(request_id);
  if (iter == data->active_requests.end()) {
    // In theory there should always be an associated stored request found, but
    // if one of our custom bindings erroneously calls the callbacks for
    // completing a request more than once the associated request will have
    // already been removed. If that is the case we bail early.
    // TODO(tjudkins): Audit existing handle request custom hooks to see if this
    // could happen in any of them. crbug.com/1298409 seemed to indicate this
    // was happening, hence why we fail gracefully here to avoid a crash.
    LOG(ERROR) << "No callback found for the specified request ID.";
    base::debug::DumpWithoutCrashing();
    return;
  }
  auto callback = std::move(iter->second);
  data->active_requests.erase(iter);
  std::move(callback).Run(did_succeed, &args);
}

// Helper function to add a success and failure callback to the arguments passed
// to handle request hooks that require an asynchronous response and add a
// pending request to handle resolving it. Updates |arguments| to replace the
// trailing callback with a custom handler function to resolve the request on a
// success and adds another handler function to the end of |arguments| for
// resolving in the case of a failure. Also adds the associated promise to the
// return on |result| if this is for a promise based request.
void AddSuccessAndFailureCallbacks(
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type,
    APIRequestHandler& request_handler,
    binding::ResultModifierFunction result_modifier,
    base::WeakPtr<APIBindingHooks> weak_ptr,
    v8::LocalVector<v8::Value>* arguments,
    APIBindingHooks::RequestResult& result) {
  DCHECK(!arguments->empty());

  // Since ParseArgumentsToV8 fills missing optional arguments with null, the
  // final argument should either be a function if the API was called with a
  // callback or null if it was left off.
  // Note: the response callback here can actually remain empty in the case
  // of an optional callback being left off in a context that doesn't support
  // promises.
  v8::Local<v8::Function> response_callback;
  if (async_type == binding::AsyncResponseType::kCallback) {
    DCHECK(arguments->back()->IsFunction());
    response_callback = arguments->back().As<v8::Function>();
  } else if (async_type == binding::AsyncResponseType::kPromise) {
    DCHECK(arguments->back()->IsNull());
  }

  APIRequestHandler::RequestDetails request_details =
      request_handler.AddPendingRequest(context, async_type, response_callback,
                                        std::move(result_modifier));
  DCHECK_EQ(async_type == binding::AsyncResponseType::kPromise,
            !request_details.promise.IsEmpty());
  result.return_value = request_details.promise;

  // We store the callbacks to complete the requests in a map on the
  // APIHooksPerContextData associated with the request id.
  v8::Local<v8::Value> v8_request_id =
      v8::Integer::New(context->GetIsolate(), request_details.request_id);
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  DCHECK(data) << "APIHooks PerContextData should always exist if we have an "
                  "active request";
  data->active_requests.emplace(
      request_details.request_id,
      base::BindOnce(&APIBindingHooks::CompleteHandleRequest,
                     std::move(weak_ptr), request_details.request_id));

  v8::Local<v8::Function> success_callback =
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            CompleteHandleRequestHelper(info, true);
          },
          v8_request_id)
          .ToLocalChecked();
  v8::Local<v8::Function> failure_callback =
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            CompleteHandleRequestHelper(info, false);
          },
          v8_request_id)
          .ToLocalChecked();
  // The success callback replaces any existing callback that may have
  // been at the end of the arguments and the failure callback is appended
  // to the end.
  arguments->back() = success_callback;
  arguments->push_back(failure_callback);
}

}  // namespace

APIBindingHooks::RequestResult::RequestResult(ResultCode code) : code(code) {}
APIBindingHooks::RequestResult::RequestResult(
    ResultCode code,
    v8::Local<v8::Function> custom_callback)
    : code(code), custom_callback(custom_callback) {}
APIBindingHooks::RequestResult::RequestResult(
    ResultCode code,
    v8::Local<v8::Function> custom_callback,
    binding::ResultModifierFunction result_modifier)
    : code(code),
      custom_callback(custom_callback),
      result_modifier(std::move(result_modifier)) {}
APIBindingHooks::RequestResult::RequestResult(std::string invocation_error)
    : code(INVALID_INVOCATION), error(std::move(invocation_error)) {}
APIBindingHooks::RequestResult::~RequestResult() = default;
APIBindingHooks::RequestResult::RequestResult(RequestResult&& other) = default;

APIBindingHooks::APIBindingHooks(const std::string& api_name,
                                 APIRequestHandler* request_handler)
    : api_name_(api_name), request_handler_(request_handler) {}
APIBindingHooks::~APIBindingHooks() = default;

APIBindingHooks::RequestResult APIBindingHooks::RunHooks(
    const std::string& method_name,
    v8::Local<v8::Context> context,
    const APISignature* signature,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& type_refs) {
  binding::ResultModifierFunction result_modifier;
  // Easy case: a native custom hook.
  if (delegate_) {
    RequestResult result = delegate_->HandleRequest(
        method_name, signature, context, arguments, type_refs);
    // If the native hooks handled the call, set a custom callback or a result
    // modifier, use that.
    if (result.code != RequestResult::NOT_HANDLED ||
        !result.custom_callback.IsEmpty()) {
      return result;
    }
    // If the native hooks didn't handle the call but did set a result modifier,
    // grab it to be able to use it along with the custom hooks below.
    result_modifier = std::move(result.result_modifier);
  }

  // Harder case: looking up a custom hook registered on the context (since
  // these are JS, each context has a separate instance).
  v8::Local<v8::Object> hook_interface_object =
      GetJSHookInterfaceObject(api_name_, context, false);
  if (hook_interface_object.IsEmpty()) {
    return RequestResult(RequestResult::NOT_HANDLED, v8::Local<v8::Function>(),
                         std::move(result_modifier));
  }

  v8::Isolate* isolate = context->GetIsolate();

  JSHookInterface* hook_interface = nullptr;
  gin::Converter<JSHookInterface*>::FromV8(
      isolate,
      hook_interface_object, &hook_interface);
  CHECK(hook_interface);

  v8::Local<v8::Function> pre_validate_hook =
      hook_interface->GetPreValidationHook(method_name, isolate);
  v8::TryCatch try_catch(isolate);
  if (!pre_validate_hook.IsEmpty()) {
    // TODO(devlin): What to do with the result of this function call? Can it
    // only fail in the case we've already thrown?
    UpdateArguments(pre_validate_hook, context, arguments);
    if (!binding::IsContextValid(context))
      return RequestResult(RequestResult::CONTEXT_INVALIDATED);

    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
  }

  v8::Local<v8::Function> post_validate_hook =
      hook_interface->GetPostValidationHook(method_name, isolate);
  v8::Local<v8::Function> handle_request =
      hook_interface->GetHandleRequestHook(method_name, isolate);
  v8::Local<v8::Function> custom_callback =
      hook_interface->GetCustomCallback(method_name, isolate);

  // If both the post validation hook and the handle request hook are empty,
  // we're done...
  if (post_validate_hook.IsEmpty() && handle_request.IsEmpty()) {
    return RequestResult(RequestResult::NOT_HANDLED, custom_callback,
                         std::move(result_modifier));
  }

  // ... otherwise, we have to validate the arguments.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, type_refs);

  if (!binding::IsContextValid(context))
    return RequestResult(RequestResult::CONTEXT_INVALIDATED);

  if (try_catch.HasCaught()) {
    try_catch.ReThrow();
    return RequestResult(RequestResult::THROWN);
  }
  if (!parse_result.succeeded())
    return RequestResult(std::move(*parse_result.error));
  arguments->swap(*parse_result.arguments);

  bool updated_args = false;
  if (!post_validate_hook.IsEmpty()) {
    updated_args = true;
    UpdateArguments(post_validate_hook, context, arguments);

    if (!binding::IsContextValid(context))
      return RequestResult(RequestResult::CONTEXT_INVALIDATED);

    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
  }

  if (handle_request.IsEmpty()) {
    RequestResult::ResultCode result = updated_args
                                           ? RequestResult::ARGUMENTS_UPDATED
                                           : RequestResult::NOT_HANDLED;
    return RequestResult(result, custom_callback, std::move(result_modifier));
  }

  RequestResult result(RequestResult::HANDLED, custom_callback);

  if (signature->has_async_return()) {
    AddSuccessAndFailureCallbacks(context, parse_result.async_type,
                                  *request_handler_, std::move(result_modifier),
                                  weak_factory_.GetWeakPtr(), arguments,
                                  result);
  }

  // Safe to use synchronous JS since it's in direct response to JS calling
  // into the binding.
  v8::MaybeLocal<v8::Value> v8_result =
      JSRunner::Get(context)->RunJSFunctionSync(
          handle_request, context, arguments->size(), arguments->data());

  if (!binding::IsContextValid(context))
    return RequestResult(RequestResult::CONTEXT_INVALIDATED);

  if (try_catch.HasCaught()) {
    try_catch.ReThrow();
    return RequestResult(RequestResult::THROWN);
  }

  if (!v8_result.ToLocalChecked()->IsUndefined()) {
    DCHECK(result.return_value.IsEmpty())
        << "A handleRequest hook cannot return a synchronous result from an "
           "API that supports promises.";
    result.return_value = v8_result.ToLocalChecked();
  }
  return result;
}

void APIBindingHooks::CompleteHandleRequest(int request_id,
                                            bool did_succeed,
                                            gin::Arguments* arguments) {
  if (did_succeed) {
    request_handler_->CompleteRequest(request_id, arguments->GetAll(),
                                      /*error*/ std::string());
  } else {
    CHECK(arguments->Length() == 1);
    v8::Local<v8::Value> error = arguments->GetAll()[0];
    DCHECK(error->IsString());

    // In the case of an error we don't respond with any arguments.
    v8::LocalVector<v8::Value> response_list(arguments->isolate());
    request_handler_->CompleteRequest(
        request_id, response_list,
        gin::V8ToString(arguments->isolate(), error));
  }
}

v8::Local<v8::Object> APIBindingHooks::GetJSHookInterface(
    v8::Local<v8::Context> context) {
  return GetJSHookInterfaceObject(api_name_, context, true);
}

bool APIBindingHooks::CreateCustomEvent(v8::Local<v8::Context> context,
                                        const std::string& event_name,
                                        v8::Local<v8::Value>* event_out) {
  return delegate_ &&
         delegate_->CreateCustomEvent(context, event_name, event_out);
}

void APIBindingHooks::InitializeTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const APITypeReferenceMap& type_refs) {
  if (delegate_)
    delegate_->InitializeTemplate(isolate, object_template, type_refs);
}

void APIBindingHooks::InitializeInstance(v8::Local<v8::Context> context,
                                         v8::Local<v8::Object> instance) {
  if (delegate_)
    delegate_->InitializeInstance(context, instance);
}

void APIBindingHooks::SetDelegate(
    std::unique_ptr<APIBindingHooksDelegate> delegate) {
  delegate_ = std::move(delegate);
}

bool APIBindingHooks::UpdateArguments(v8::Local<v8::Function> function,
                                      v8::Local<v8::Context> context,
                                      v8::LocalVector<v8::Value>* arguments) {
  v8::Local<v8::Value> result;
  {
    v8::TryCatch try_catch(context->GetIsolate());
    // Safe to use synchronous JS since it's in direct response to JS calling
    // into the binding.
    v8::MaybeLocal<v8::Value> maybe_result =
        JSRunner::Get(context)->RunJSFunctionSync(
            function, context, arguments->size(), arguments->data());
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return false;
    }
    result = maybe_result.ToLocalChecked();
  }
  v8::LocalVector<v8::Value> new_args(context->GetIsolate());
  if (result.IsEmpty() || !gin::Converter<v8::LocalVector<v8::Value>>::FromV8(
                              context->GetIsolate(), result, &new_args)) {
    return false;
  }
  arguments->swap(new_args);
  return true;
}

}  // namespace extensions
