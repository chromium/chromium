// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "third_party/blink/public/web/web_blob.h"

namespace extensions {

// Keys used for passing data back through a custom callback;
constexpr const char kErrorKey[] = "error";
constexpr const char kResolverKey[] = "resolver";
constexpr const char kExceptionHandlerKey[] = "exceptionHandler";

// A helper class to adapt base::Value-style response arguments to v8 arguments
// lazily, or simply return v8 arguments directly (depending on which style of
// arguments were used in construction).
class APIRequestHandler::ArgumentAdapter {
 public:
  ArgumentAdapter(const base::Value::List* base_argumements,
                  mojom::ExtraResponseDataPtr extra_data);
  explicit ArgumentAdapter(const v8::LocalVector<v8::Value>& v8_arguments);

  ArgumentAdapter(ArgumentAdapter&) = delete;
  ArgumentAdapter& operator=(const ArgumentAdapter&) = delete;
  ArgumentAdapter(ArgumentAdapter&&) = default;
  ArgumentAdapter& operator=(ArgumentAdapter&&) = default;

  ~ArgumentAdapter();

  const v8::LocalVector<v8::Value>& GetArguments(
      v8::Local<v8::Context> context) const;

  mojom::ExtraResponseDataPtr TakeExtraData() { return std::move(extra_data_); }

 private:
  raw_ptr<const base::Value::List> base_arguments_ = nullptr;
  mutable std::optional<v8::LocalVector<v8::Value>> v8_arguments_;
  mojom::ExtraResponseDataPtr extra_data_ = nullptr;
};

APIRequestHandler::ArgumentAdapter::ArgumentAdapter(
    const base::Value::List* base_arguments,
    mojom::ExtraResponseDataPtr extra_data)
    : base_arguments_(base_arguments), extra_data_(std::move(extra_data)) {}
APIRequestHandler::ArgumentAdapter::ArgumentAdapter(
    const v8::LocalVector<v8::Value>& v8_arguments)
    : v8_arguments_(v8_arguments) {}
APIRequestHandler::ArgumentAdapter::~ArgumentAdapter() = default;

const v8::LocalVector<v8::Value>&
APIRequestHandler::ArgumentAdapter::GetArguments(
    v8::Local<v8::Context> context) const {
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(isolate->GetCurrentContext() == context);

  if (base_arguments_) {
    DCHECK(!v8_arguments_.has_value())
        << "GetArguments() should only be called once.";
    std::unique_ptr<content::V8ValueConverter> converter =
        content::V8ValueConverter::Create();
    v8_arguments_.emplace(isolate);
    v8_arguments_->reserve(base_arguments_->size());
    for (const auto& arg : *base_arguments_) {
      v8_arguments_->push_back(converter->ToV8Value(arg, context));
    }
  }

  DCHECK(v8_arguments_.has_value());
  return v8_arguments_.value();
}

// A helper class to handler delivering the results of an API call to a handler,
// which can be either a callback or a promise.
// TODO(devlin): The overlap in this class for handling a promise vs a callback
// is pretty minimal, leading to a lot of if/else branching. This might be
// cleaner with separate versions for the two cases.
class APIRequestHandler::AsyncResultHandler {
 public:
  // A callback-based result handler.
  AsyncResultHandler(v8::Isolate* isolate,
                     v8::Local<v8::Function> callback,
                     v8::Local<v8::Function> custom_callback,
                     binding::ResultModifierFunction result_modifier,
                     ExceptionHandler* exception_handler);
  // A promise-based result handler.
  AsyncResultHandler(v8::Isolate* isolate,
                     v8::Local<v8::Promise::Resolver> promise_resolver,
                     v8::Local<v8::Function> custom_callback,
                     binding::ResultModifierFunction result_modifier);

  AsyncResultHandler(const AsyncResultHandler&) = delete;
  AsyncResultHandler& operator=(const AsyncResultHandler&) = delete;

  ~AsyncResultHandler();

  // Resolve the request.
  // - Sets last error, if it's not promise-based.
  // - If the request had a custom callback, this calls the custom callback,
  // with a handle to then resolve the extension's callback or promise.
  // - Else, invokes the extension's callback or resolves the promise
  // immediately.
  void ResolveRequest(v8::Local<v8::Context> context,
                      APILastError* last_error,
                      const v8::LocalVector<v8::Value>& response_args,
                      const std::string& error,
                      mojom::ExtraResponseDataPtr extra_data);

  // Returns true if the request handler is using a custom callback.
  bool has_custom_callback() const { return !custom_callback_.IsEmpty(); }

 private:
  // Delivers the result to the promise resolver.
  static void ResolvePromise(v8::Local<v8::Context> context,
                             const v8::LocalVector<v8::Value>& response_args,
                             const std::string& error,
                             v8::Local<v8::Promise::Resolver> resolver);

  // Delivers the result to the callback provided by the extension.
  static void CallExtensionCallback(v8::Local<v8::Context> context,
                                    v8::LocalVector<v8::Value> response_args,
                                    v8::Local<v8::Function> extension_callback,
                                    ExceptionHandler* exception_handler);

  // Helper function to handle the result after the bindings' custom callback
  // has completed.
  static void CustomCallbackAdaptor(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  // Delivers the result to the custom callback.
  void CallCustomCallback(v8::Local<v8::Context> context,
                          const v8::LocalVector<v8::Value>& response_args,
                          const std::string& error);

  // The type of asynchronous response this handler is for.
  const binding::AsyncResponseType async_type_;

  // Callback-based handlers. Mutually exclusive with promise-based handlers.
  v8::Global<v8::Function> extension_callback_;

  // Promise-based handlers. Mutually exclusive with callback-based handlers.
  v8::Global<v8::Promise::Resolver> promise_resolver_;

  // The ExceptionHandler used to catch any exceptions thrown when handling the
  // extension's callback. Only non-null for callback-based requests.
  // This is guaranteed to be valid while the AsyncResultHandler is valid
  // because the ExceptionHandler lives for the duration of the bindings
  // system, similar to the APIRequestHandler (which owns this).
  raw_ptr<ExceptionHandler> exception_handler_ = nullptr;

  // Custom callback handlers.
  v8::Global<v8::Function> custom_callback_;

  // A OnceCallback that can be used to modify the return arguments.
  binding::ResultModifierFunction result_modifier_;
};

APIRequestHandler::AsyncResultHandler::AsyncResultHandler(
    v8::Isolate* isolate,
    v8::Local<v8::Function> extension_callback,
    v8::Local<v8::Function> custom_callback,
    binding::ResultModifierFunction result_modifier,
    ExceptionHandler* exception_handler)
    : async_type_(binding::AsyncResponseType::kCallback),
      exception_handler_(exception_handler),
      result_modifier_(std::move(result_modifier)) {
  DCHECK(!extension_callback.IsEmpty() || !custom_callback.IsEmpty());
  DCHECK(exception_handler_);
  if (!extension_callback.IsEmpty()) {
    extension_callback_.Reset(isolate, extension_callback);
  }
  if (!custom_callback.IsEmpty()) {
    custom_callback_.Reset(isolate, custom_callback);
  }
}

APIRequestHandler::AsyncResultHandler::AsyncResultHandler(
    v8::Isolate* isolate,
    v8::Local<v8::Promise::Resolver> promise_resolver,
    v8::Local<v8::Function> custom_callback,
    binding::ResultModifierFunction result_modifier)
    : async_type_(binding::AsyncResponseType::kPromise),
      result_modifier_(std::move(result_modifier)) {
  // NOTE(devlin): We'll need to handle an empty promise resolver if
  // v8::Promise::Resolver::New() isn't guaranteed.
  DCHECK(!promise_resolver.IsEmpty());
  promise_resolver_.Reset(isolate, promise_resolver);
  if (!custom_callback.IsEmpty()) {
    custom_callback_.Reset(isolate, custom_callback);
  }
}

APIRequestHandler::AsyncResultHandler::~AsyncResultHandler() = default;

void APIRequestHandler::AsyncResultHandler::ResolveRequest(
    v8::Local<v8::Context> context,
    APILastError* last_error,
    const v8::LocalVector<v8::Value>& response_args,
    const std::string& error,
    mojom::ExtraResponseDataPtr extra_data) {
  v8::Isolate* isolate = context->GetIsolate();

  // Set runtime.lastError if there is an error and this isn't a promise-based
  // request (promise-based requests instead reject to indicate failure).
  bool set_last_error = promise_resolver_.IsEmpty() && !error.empty();
  if (set_last_error) {
    last_error->SetError(context, error);
  }

  // If there is a result modifier for this async request and the response args
  // are not empty, run the result modifier and allow it to massage the return
  // arguments before we send them back.
  // Note: a request can end up with a result modifier and be returning an empty
  // set of args if we are responding that an error occurred.
  v8::LocalVector<v8::Value> args =
      result_modifier_.is_null() || response_args.empty()
          ? response_args
          : std::move(result_modifier_)
                .Run(response_args, context, async_type_);

  if (has_custom_callback()) {
    // Blobs that are part of the response are passed in as an extra parameter
    // to the custom callback. The custom callback can then incorporate these
    // blobs appropriately in its response.
    if (extra_data) {
      v8::LocalVector<v8::Value> v8_blobs(isolate);
      for (auto& blob : extra_data->blobs) {
        auto web_blob =
            blink::WebBlob::CreateFromSerializedBlob(std::move(blob));
        v8_blobs.push_back(web_blob.ToV8Value(context->GetIsolate()));
      }
      auto blobs = v8::Array::New(context->GetIsolate(), v8_blobs.data(),
                                  v8_blobs.size());
      args.push_back(std::move(blobs));
    }

    // Custom callback case; the custom callback will invoke a curried-in
    // callback, which will trigger the response in the extension (either
    // promise or callback).
    CallCustomCallback(context, args, error);
  } else if (!promise_resolver_.IsEmpty()) {  // Promise-based request.
    DCHECK(extension_callback_.IsEmpty());
    ResolvePromise(context, args, error, promise_resolver_.Get(isolate));
  } else {  // Callback-based request.
    DCHECK(!extension_callback_.IsEmpty());
    DCHECK(exception_handler_);
    CallExtensionCallback(context, std::move(args),
                          extension_callback_.Get(isolate), exception_handler_);
  }

  // Since arbitrary JS was run the context might have been invalidated, so only
  // clear last error if the context is still valid.
  if (set_last_error && binding::IsContextValid(context)) {
    last_error->ClearError(context, true);
  }
}

// static
void APIRequestHandler::AsyncResultHandler::ResolvePromise(
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& response_args,
    const std::string& error,
    v8::Local<v8::Promise::Resolver> resolver) {
  DCHECK_LE(response_args.size(), 1u);

  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  if (error.empty()) {
    v8::Local<v8::Value> result;
    if (!response_args.empty()) {
      result = response_args[0];
    } else {
      result = v8::Undefined(isolate);
    }

    v8::Maybe<bool> promise_result = resolver->Resolve(context, result);
    // TODO(devlin): It's potentially possible that this could throw if V8
    // is terminating on a worker thread; however, it's unclear what happens in
    // that scenario (we may appropriately shutdown the thread, or any future
    // access of v8 may cause crashes). Make this a CHECK() to flush out any
    // situations in which this is a concern. If there are no crashes after
    // some time, we may be able to downgrade this.
    CHECK(promise_result.IsJust());
  } else {
    v8::Local<v8::Value> v8_error =
        v8::Exception::Error(gin::StringToV8(isolate, error));
    v8::Maybe<bool> promise_result = resolver->Reject(context, v8_error);
    // See comment above.
    CHECK(promise_result.IsJust());
  }
}

// static
void APIRequestHandler::AsyncResultHandler::CallExtensionCallback(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value> args,
    v8::Local<v8::Function> extension_callback,
    ExceptionHandler* exception_handler) {
  DCHECK(exception_handler);
  // TODO(devlin): Integrate the API method name in the error message. This
  // will require currying it around a bit more.
  exception_handler->RunExtensionCallback(
      context, extension_callback, std::move(args), "Error handling response");
}

// static
void APIRequestHandler::AsyncResultHandler::CustomCallbackAdaptor(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments arguments(info);
  v8::Isolate* isolate = arguments.isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> data = info.Data().As<v8::Object>();

  v8::Local<v8::Value> resolver =
      data->Get(context, gin::StringToSymbol(isolate, kResolverKey))
          .ToLocalChecked();
  if (resolver->IsFunction()) {
    v8::Local<v8::Value> exception_handler_value =
        data->Get(context, gin::StringToSymbol(isolate, kExceptionHandlerKey))
            .ToLocalChecked();
    ExceptionHandler* exception_handler =
        ExceptionHandler::FromV8Wrapper(isolate, exception_handler_value);
    // `exception_handler` could be null if the context were invalidated.
    // Since this can be invoked arbitrarily from running JS, we need to
    // handle this case gracefully.
    if (!exception_handler) {
      return;
    }

    CallExtensionCallback(context, arguments.GetAll(),
                          resolver.As<v8::Function>(), exception_handler);
  } else {
    v8::Local<v8::Value> error_value =
        data->Get(context, gin::StringToSymbol(isolate, kErrorKey))
            .ToLocalChecked();
    const std::string error = gin::V8ToString(isolate, error_value);

    CHECK(resolver->IsPromise());
    ResolvePromise(context, arguments.GetAll(), error,
                   resolver.As<v8::Promise::Resolver>());
  }
}

void APIRequestHandler::AsyncResultHandler::CallCustomCallback(
    v8::Local<v8::Context> context,
    const v8::LocalVector<v8::Value>& response_args,
    const std::string& error) {
  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Value> callback_to_pass = v8::Undefined(isolate);
  if (!extension_callback_.IsEmpty() || !promise_resolver_.IsEmpty()) {
    gin::DataObjectBuilder data_builder(isolate);
    v8::Local<v8::Value> resolver_value;
    if (!promise_resolver_.IsEmpty()) {
      resolver_value = promise_resolver_.Get(isolate);
    } else {
      DCHECK(exception_handler_);
      resolver_value = extension_callback_.Get(isolate);
      data_builder.Set(kExceptionHandlerKey,
                       exception_handler_->GetV8Wrapper(isolate));
    }
    v8::Local<v8::Object> data = data_builder.Set(kResolverKey, resolver_value)
                                     .Set(kErrorKey, error)
                                     .Build();
    // Rather than passing the original callback, we create a function which
    // calls back to an adpator which will invoke the original callback or
    // resolve the promises, depending on the type of request that was made to
    // the API.
    callback_to_pass = v8::Function::New(context, &CustomCallbackAdaptor, data)
                           .ToLocalChecked();
  }

  // Custom callbacks in the JS bindings are called with the arguments of the
  // callback function and the response from the API.
  v8::LocalVector<v8::Value> custom_callback_args(isolate);
  custom_callback_args.reserve(1 + response_args.size());
  custom_callback_args.push_back(callback_to_pass);
  custom_callback_args.insert(custom_callback_args.end(), response_args.begin(),
                              response_args.end());

  JSRunner::Get(context)->RunJSFunction(custom_callback_.Get(isolate), context,
                                        custom_callback_args.size(),
                                        custom_callback_args.data());
}

APIRequestHandler::Request::Request() = default;
APIRequestHandler::Request::~Request() = default;

APIRequestHandler::RequestDetails::RequestDetails(
    int request_id,
    v8::Local<v8::Promise> promise)
    : request_id(request_id), promise(promise) {}
APIRequestHandler::RequestDetails::~RequestDetails() = default;
APIRequestHandler::RequestDetails::RequestDetails(const RequestDetails& other) =
    default;

APIRequestHandler::PendingRequest::PendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const std::string& method_name,
    std::unique_ptr<AsyncResultHandler> async_handler,
    std::unique_ptr<InteractionProvider::Token> gesture_token)
    : isolate(isolate),
      context(isolate, context),
      method_name(method_name),
      async_handler(std::move(async_handler)) {
  // Only curry the user gesture through if there's something to handle the
  // response.
  if (this->async_handler) {
    user_gesture_token = std::move(gesture_token);
  }
}

APIRequestHandler::PendingRequest::~PendingRequest() = default;
APIRequestHandler::PendingRequest::PendingRequest(PendingRequest&&) = default;
APIRequestHandler::PendingRequest& APIRequestHandler::PendingRequest::operator=(
    PendingRequest&&) = default;

APIRequestHandler::APIRequestHandler(
    SendRequestMethod send_request,
    APILastError last_error,
    ExceptionHandler* exception_handler,
    const InteractionProvider* interaction_provider)
    : send_request_(std::move(send_request)),
      last_error_(std::move(last_error)),
      exception_handler_(exception_handler),
      interaction_provider_(interaction_provider) {}

APIRequestHandler::~APIRequestHandler() = default;

v8::Local<v8::Promise> APIRequestHandler::StartRequest(
    v8::Local<v8::Context> context,
    const std::string& method,
    base::Value::List arguments_list,
    binding::AsyncResponseType async_type,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Function> custom_callback,
    binding::ResultModifierFunction result_modifier) {
  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Promise> promise;
  std::unique_ptr<AsyncResultHandler> async_handler =
      GetAsyncResultHandler(context, async_type, callback, custom_callback,
                            std::move(result_modifier), &promise);
  DCHECK_EQ(async_type == binding::AsyncResponseType::kPromise,
            !promise.IsEmpty());

  int request_id = GetNextRequestId();
  auto request = std::make_unique<Request>();
  request->request_id = request_id;

  std::unique_ptr<InteractionProvider::Token> user_gesture_token;
  if (async_handler) {
    user_gesture_token = interaction_provider_->GetCurrentToken(context);
    request->has_async_response_handler = true;
  }

  pending_requests_.emplace(
      request_id,
      PendingRequest(isolate, context, method, std::move(async_handler),
                     std::move(user_gesture_token)));

  request->has_user_gesture =
      interaction_provider_->HasActiveInteraction(context);
  request->arguments_list = std::move(arguments_list);
  request->method_name = method;

  last_sent_request_id_ = request_id;
  send_request_.Run(std::move(request), context);

  return promise;
}

void APIRequestHandler::CompleteRequest(
    int request_id,
    const base::Value::List& response_args,
    const std::string& error,
    mojom::ExtraResponseDataPtr extra_data) {
  CompleteRequestImpl(request_id,
                      ArgumentAdapter(&response_args, std::move(extra_data)),
                      error);
}

void APIRequestHandler::CompleteRequest(
    int request_id,
    const v8::LocalVector<v8::Value>& response_args,
    const std::string& error) {
  CompleteRequestImpl(request_id, ArgumentAdapter(response_args), error);
}

APIRequestHandler::RequestDetails APIRequestHandler::AddPendingRequest(
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type,
    v8::Local<v8::Function> callback,
    binding::ResultModifierFunction result_modifier) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Promise> promise;
  std::unique_ptr<AsyncResultHandler> async_handler = GetAsyncResultHandler(
      context, async_type, callback, v8::Local<v8::Function>(),
      std::move(result_modifier), &promise);
  DCHECK_EQ(async_type == binding::AsyncResponseType::kPromise,
            !promise.IsEmpty());

  int request_id = GetNextRequestId();

  // NOTE(devlin): We ignore the UserGestureToken for synthesized requests like
  // these that aren't sent to the browser. It is the caller's responsibility to
  // handle any user gesture behavior. This prevents an issue where messaging
  // handling would create an extra scoped user gesture, causing issues. See
  // https://crbug.com/921141.
  std::unique_ptr<InteractionProvider::Token> null_user_gesture_token;

  pending_requests_.emplace(
      request_id,
      PendingRequest(isolate, context, std::string(), std::move(async_handler),
                     std::move(null_user_gesture_token)));

  return RequestDetails(request_id, promise);
}

void APIRequestHandler::InvalidateContext(v8::Local<v8::Context> context) {
  for (auto iter = pending_requests_.begin();
       iter != pending_requests_.end();) {
    if (iter->second.context == context) {
      iter = pending_requests_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void APIRequestHandler::SetResponseValidator(
    std::unique_ptr<APIResponseValidator> response_validator) {
  DCHECK(!response_validator_);
  response_validator_ = std::move(response_validator);
}

std::set<int> APIRequestHandler::GetPendingRequestIdsForTesting() const {
  std::set<int> result;
  for (const auto& pair : pending_requests_) {
    result.insert(pair.first);
  }
  return result;
}

int APIRequestHandler::GetNextRequestId() {
  // The request id is primarily used in the renderer to associate an API
  // request with the associated callback, but it's also used in the browser as
  // an identifier for the extension function (e.g. by the pageCapture API).
  // TODO(devlin): We should probably fix this, since the request id is only
  // unique per-isolate, rather than globally.
  // TODO(devlin): We could *probably* get away with just using an integer
  // here, but it's a little less foolproof. How slow is GenerateGUID? Should
  // we use that instead? It means updating the IPC
  // (ExtensionHostMsg_Request).
  // base::UnguessableToken is another good option.
  return next_request_id_++;
}

std::unique_ptr<APIRequestHandler::AsyncResultHandler>
APIRequestHandler::GetAsyncResultHandler(
    v8::Local<v8::Context> context,
    binding::AsyncResponseType async_type,
    v8::Local<v8::Function> extension_callback,
    v8::Local<v8::Function> custom_callback,
    binding::ResultModifierFunction result_modifier,
    v8::Local<v8::Promise>* promise_out) {
  v8::Isolate* isolate = context->GetIsolate();

  std::unique_ptr<AsyncResultHandler> async_handler;
  if (async_type == binding::AsyncResponseType::kPromise) {
    DCHECK(extension_callback.IsEmpty())
        << "Promise based requests should never be "
           "started with a callback being passed in.";
    v8::Local<v8::Promise::Resolver> resolver =
        v8::Promise::Resolver::New(context).ToLocalChecked();
    async_handler = std::make_unique<AsyncResultHandler>(
        isolate, resolver, custom_callback, std::move(result_modifier));
    *promise_out = resolver->GetPromise();
  } else if (!custom_callback.IsEmpty() || !extension_callback.IsEmpty()) {
    async_handler = std::make_unique<AsyncResultHandler>(
        isolate, extension_callback, custom_callback,
        std::move(result_modifier), exception_handler_);
  }
  return async_handler;
}

void APIRequestHandler::CompleteRequestImpl(int request_id,
                                            ArgumentAdapter arguments,
                                            const std::string& error) {
  auto iter = pending_requests_.find(request_id);
  // The request may have been removed if the context was invalidated before a
  // response is ready.
  if (iter == pending_requests_.end()) {
    return;
  }

  PendingRequest pending_request = std::move(iter->second);
  pending_requests_.erase(iter);

  v8::Isolate* isolate = pending_request.isolate;
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = pending_request.context.Get(isolate);
  v8::Context::Scope context_scope(context);

  if (!pending_request.async_handler) {
    // If there's no async handler associated with the request, but there is an
    // error, report the error as if it were unchecked.
    if (!error.empty()) {
      // TODO(devlin): Use pending_requeset.method_name here?
      last_error_.ReportUncheckedError(context, error);
    }
    // No async handler to trigger, so we're done!
    return;
  }

  const v8::LocalVector<v8::Value>& response_args =
      arguments.GetArguments(context);

  std::unique_ptr<InteractionProvider::Scope> user_gesture;
  if (pending_request.user_gesture_token) {
    user_gesture = interaction_provider_->CreateScopedInteraction(
        context, std::move(pending_request.user_gesture_token));
  }

  if (response_validator_) {
    bool has_custom_callback =
        pending_request.async_handler->has_custom_callback();
    response_validator_->ValidateResponse(
        context, pending_request.method_name, response_args, error,
        has_custom_callback
            ? APIResponseValidator::CallbackType::kAPIProvided
            : APIResponseValidator::CallbackType::kCallerProvided);
  }

  v8::TryCatch try_catch(isolate);
  pending_request.async_handler->ResolveRequest(
      context, &last_error_, response_args, error, arguments.TakeExtraData());

  // Since arbitrary JS has ran, the context may have been invalidated. If it
  // was, bail.
  if (!binding::IsContextValid(context)) {
    return;
  }

  if (try_catch.HasCaught()) {
    v8::Local<v8::Message> v8_message = try_catch.Message();
    std::optional<std::string> message;
    if (!v8_message.IsEmpty()) {
      message = gin::V8ToString(isolate, v8_message->Get());
    }
    exception_handler_->HandleException(context, "Error handling response",
                                        &try_catch);
  }
}

}  // namespace extensions
