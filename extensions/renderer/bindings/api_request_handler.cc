// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"

namespace extensions {

// A helper class to adapt base::Value-style response arguments to v8 arguments
// lazily, or simply return v8 arguments directly (depending on which style of
// arguments were used in construction).
class APIRequestHandler::ArgumentAdapter {
 public:
  explicit ArgumentAdapter(const base::ListValue* base_argumements);
  explicit ArgumentAdapter(
      const std::vector<v8::Local<v8::Value>>& v8_arguments);
  ~ArgumentAdapter();

  const std::vector<v8::Local<v8::Value>>& GetArguments(
      v8::Local<v8::Context> context) const;

 private:
  const base::ListValue* base_arguments_ = nullptr;
  mutable std::vector<v8::Local<v8::Value>> v8_arguments_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentAdapter);
};

APIRequestHandler::ArgumentAdapter::ArgumentAdapter(
    const base::ListValue* base_arguments)
    : base_arguments_(base_arguments) {}
APIRequestHandler::ArgumentAdapter::ArgumentAdapter(
    const std::vector<v8::Local<v8::Value>>& v8_arguments)
    : v8_arguments_(v8_arguments) {}
APIRequestHandler::ArgumentAdapter::~ArgumentAdapter() = default;

const std::vector<v8::Local<v8::Value>>&
APIRequestHandler::ArgumentAdapter::GetArguments(
    v8::Local<v8::Context> context) const {
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(isolate->GetCurrentContext() == context);

  if (base_arguments_) {
    DCHECK(v8_arguments_.empty())
        << "GetArguments() should only be called once.";
    std::unique_ptr<content::V8ValueConverter> converter =
        content::V8ValueConverter::Create();
    v8_arguments_.reserve(base_arguments_->GetSize());
    for (const auto& arg : *base_arguments_)
      v8_arguments_.push_back(converter->ToV8Value(&arg, context));
  }

  return v8_arguments_;
}

// A helper class to handler delivering the results of an API call to a handler,
// which can be either a callback or a promise.
class APIRequestHandler::AsyncResultHandler {
 public:
  // A callback-based result handler.
  AsyncResultHandler(
      v8::Isolate* isolate,
      v8::Local<v8::Function> callback,
      base::Optional<std::vector<v8::Global<v8::Value>>> callback_args);
  // A promise-based result handler.
  AsyncResultHandler(v8::Isolate* isolate,
                     v8::Local<v8::Promise::Resolver> promise_resolver);

  ~AsyncResultHandler();

  // Delivers the result to the result handler.
  void DeliverResult(v8::Local<v8::Context> context,
                     APILastError* last_error,
                     const std::vector<v8::Local<v8::Value>>& response_args,
                     const std::string& error);

  // Returns true if the request handler is using a custom callback.
  bool has_custom_callback() const { return !!callback_arguments_; }

 private:
  void DeliverPromiseResult(
      v8::Local<v8::Context> context,
      const std::vector<v8::Local<v8::Value>>& response_args,
      const std::string& error);

  void DeliverCallbackResult(
      v8::Local<v8::Context> context,
      APILastError* last_error,
      const std::vector<v8::Local<v8::Value>>& response_args,
      const std::string& error);

  // Callback-based handlers. Mutually exclusive with promise-based handlers.
  v8::Global<v8::Function> callback_;
  base::Optional<std::vector<v8::Global<v8::Value>>> callback_arguments_;

  // Promise-based handlers. Mutually exclusive with callback-based handlers.
  v8::Global<v8::Promise::Resolver> promise_resolver_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResultHandler);
};

APIRequestHandler::AsyncResultHandler::AsyncResultHandler(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    base::Optional<std::vector<v8::Global<v8::Value>>> callback_args)
    : callback_arguments_(std::move(callback_args)) {
  DCHECK(!callback.IsEmpty());
  callback_.Reset(isolate, callback);
}

APIRequestHandler::AsyncResultHandler::AsyncResultHandler(
    v8::Isolate* isolate,
    v8::Local<v8::Promise::Resolver> promise_resolver) {
  // NOTE(devlin): We'll need to handle an empty promise resolver if
  // v8::Promise::Resolver::New() isn't guaranteed.
  DCHECK(!promise_resolver.IsEmpty());
  promise_resolver_.Reset(isolate, promise_resolver);
}

APIRequestHandler::AsyncResultHandler::~AsyncResultHandler() {}

void APIRequestHandler::AsyncResultHandler::DeliverResult(
    v8::Local<v8::Context> context,
    APILastError* last_error,
    const std::vector<v8::Local<v8::Value>>& response_args,
    const std::string& error) {
  if (!promise_resolver_.IsEmpty()) {
    DCHECK(callback_.IsEmpty());
    DCHECK(!callback_arguments_);
    DeliverPromiseResult(context, response_args, error);
  } else {
    DeliverCallbackResult(context, last_error, response_args, error);
  }
}

void APIRequestHandler::AsyncResultHandler::DeliverPromiseResult(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& response_args,
    const std::string& error) {
  DCHECK_LE(response_args.size(), 1u);

  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Promise::Resolver> resolver = promise_resolver_.Get(isolate);
  if (error.empty()) {
    v8::Local<v8::Value> result;
    if (!response_args.empty())
      result = response_args[0];
    else
      result = v8::Undefined(isolate);

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

void APIRequestHandler::AsyncResultHandler::DeliverCallbackResult(
    v8::Local<v8::Context> context,
    APILastError* last_error,
    const std::vector<v8::Local<v8::Value>>& response_args,
    const std::string& error) {
  v8::Isolate* isolate = context->GetIsolate();
  std::vector<v8::Local<v8::Value>> full_args;
  size_t curried_argument_size =
      callback_arguments_ ? callback_arguments_->size() : 0u;
  full_args.reserve(response_args.size() + curried_argument_size);
  if (callback_arguments_) {
    for (const auto& arg : *callback_arguments_)
      full_args.push_back(arg.Get(isolate));
  }
  full_args.insert(full_args.end(), response_args.begin(), response_args.end());

  if (!error.empty())
    last_error->SetError(context, error);

  JSRunner::Get(context)->RunJSFunction(callback_.Get(isolate), context,
                                        full_args.size(), full_args.data());

  // Arbitrary JS ran; context might have been invalidated.
  if (!binding::IsContextValid(context))
    return;

  if (!error.empty())
    last_error->ClearError(context, true);
}

APIRequestHandler::Request::Request() {}
APIRequestHandler::Request::~Request() = default;

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
  if (this->async_handler)
    user_gesture_token = std::move(gesture_token);
}

APIRequestHandler::PendingRequest::~PendingRequest() {}
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

APIRequestHandler::~APIRequestHandler() {}

int APIRequestHandler::StartRequest(v8::Local<v8::Context> context,
                                    const std::string& method,
                                    std::unique_ptr<base::ListValue> arguments,
                                    v8::Local<v8::Function> callback,
                                    v8::Local<v8::Function> custom_callback) {
  std::unique_ptr<AsyncResultHandler> async_handler;
  int request_id = GetNextRequestId();
  if (!custom_callback.IsEmpty() || !callback.IsEmpty()) {
    v8::Isolate* isolate = context->GetIsolate();
    base::Optional<std::vector<v8::Global<v8::Value>>> callback_args;

    // In the JS bindings, custom callbacks are called with the arguments of
    // name, the full request object (see below), the original callback, and
    // the responses from the API. The responses from the API are handled by the
    // APIRequestHandler, but we need to curry in the other values.
    if (!custom_callback.IsEmpty()) {
      // TODO(devlin): The |request| object in the JS bindings includes
      // properties for callback, callbackSchema, args, stack, id, and
      // customCallback. Of those, it appears that we only use stack, args, and
      // id (since callback is curried in separately). We may be able to update
      // bindings to get away from some of those. For now, just pass in an
      // object with the request id.
      v8::Local<v8::Object> request =
          gin::DataObjectBuilder(isolate).Set("id", request_id).Build();
      v8::Local<v8::Value> callback_to_pass = callback;
      if (callback_to_pass.IsEmpty())
        callback_to_pass = v8::Undefined(isolate);

      v8::Global<v8::Value> args[] = {
          v8::Global<v8::Value>(isolate, gin::StringToSymbol(isolate, method)),
          v8::Global<v8::Value>(isolate, request),
          v8::Global<v8::Value>(isolate, callback_to_pass)};
      callback_args.emplace(std::make_move_iterator(std::begin(args)),
                            std::make_move_iterator(std::end(args)));
      callback = custom_callback;
    }

    async_handler = std::make_unique<AsyncResultHandler>(
        isolate, callback, std::move(callback_args));
  }

  StartRequestImpl(context, request_id, method, std::move(arguments),
                   std::move(async_handler));
  return request_id;
}

std::pair<int, v8::Local<v8::Promise>>
APIRequestHandler::StartPromiseBasedRequest(
    v8::Local<v8::Context> context,
    const std::string& method,
    std::unique_ptr<base::ListValue> arguments) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  auto async_handler = std::make_unique<AsyncResultHandler>(isolate, resolver);
  int request_id = GetNextRequestId();
  StartRequestImpl(context, request_id, method, std::move(arguments),
                   std::move(async_handler));

  return {request_id, resolver->GetPromise()};
}

void APIRequestHandler::CompleteRequest(int request_id,
                                        const base::ListValue& response_args,
                                        const std::string& error) {
  CompleteRequestImpl(request_id, ArgumentAdapter(&response_args), error);
}

void APIRequestHandler::CompleteRequest(
    int request_id,
    const std::vector<v8::Local<v8::Value>>& response_args,
    const std::string& error) {
  CompleteRequestImpl(request_id, ArgumentAdapter(response_args), error);
}

int APIRequestHandler::AddPendingRequest(v8::Local<v8::Context> context,
                                         v8::Local<v8::Function> callback) {
  int request_id = GetNextRequestId();

  // NOTE(devlin): We ignore the UserGestureToken for synthesized requests like
  // these that aren't sent to the browser. It is the caller's responsibility to
  // handle any user gesture behavior. This prevents an issue where messaging
  // handling would create an extra scoped user gesture, causing issues. See
  // https://crbug.com/921141.
  std::unique_ptr<InteractionProvider::Token> null_user_gesture_token;

  auto async_handler = std::make_unique<AsyncResultHandler>(
      context->GetIsolate(), callback, base::nullopt);
  pending_requests_.emplace(
      request_id, PendingRequest(context->GetIsolate(), context, std::string(),
                                 std::move(async_handler),
                                 std::move(null_user_gesture_token)));
  return request_id;
}

void APIRequestHandler::InvalidateContext(v8::Local<v8::Context> context) {
  for (auto iter = pending_requests_.begin();
       iter != pending_requests_.end();) {
    if (iter->second.context == context)
      iter = pending_requests_.erase(iter);
    else
      ++iter;
  }
}

void APIRequestHandler::SetResponseValidator(
    std::unique_ptr<APIResponseValidator> response_validator) {
  DCHECK(!response_validator_);
  response_validator_ = std::move(response_validator);
}

std::set<int> APIRequestHandler::GetPendingRequestIdsForTesting() const {
  std::set<int> result;
  for (const auto& pair : pending_requests_)
    result.insert(pair.first);
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

void APIRequestHandler::StartRequestImpl(
    v8::Local<v8::Context> context,
    int request_id,
    const std::string& method,
    std::unique_ptr<base::ListValue> arguments,
    std::unique_ptr<AsyncResultHandler> async_handler) {
  auto request = std::make_unique<Request>();
  request->request_id = request_id;

  std::unique_ptr<InteractionProvider::Token> user_gesture_token;
  if (async_handler) {
    user_gesture_token = interaction_provider_->GetCurrentToken(context);
    request->has_callback = true;
  }

  v8::Isolate* isolate = context->GetIsolate();
  pending_requests_.emplace(
      request_id,
      PendingRequest(isolate, context, method, std::move(async_handler),
                     std::move(user_gesture_token)));

  request->has_user_gesture =
      interaction_provider_->HasActiveInteraction(context);
  request->arguments = std::move(arguments);
  request->method_name = method;

  last_sent_request_id_ = request_id;
  send_request_.Run(std::move(request), context);
}

void APIRequestHandler::CompleteRequestImpl(int request_id,
                                            const ArgumentAdapter& arguments,
                                            const std::string& error) {
  auto iter = pending_requests_.find(request_id);
  // The request may have been removed if the context was invalidated before a
  // response is ready.
  if (iter == pending_requests_.end())
    return;

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

  const std::vector<v8::Local<v8::Value>>& response_args =
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

  pending_request.async_handler->DeliverResult(context, &last_error_,
                                               response_args, error);

  // Since arbitrary JS has ran, the context may have been invalidated. If it
  // was, bail.
  if (!binding::IsContextValid(context))
    return;

  if (try_catch.HasCaught()) {
    v8::Local<v8::Message> v8_message = try_catch.Message();
    base::Optional<std::string> message;
    if (!v8_message.IsEmpty())
      message = gin::V8ToString(isolate, v8_message->Get());
    exception_handler_->HandleException(context, "Error handling response",
                                        &try_catch);
  }
}

}  // namespace extensions
