// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_REQUEST_HANDLER_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_REQUEST_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/mojom/extra_response_data.mojom.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_last_error.h"
#include "extensions/renderer/bindings/interaction_provider.h"
#include "v8/include/v8.h"

namespace extensions {
class APIResponseValidator;
class ExceptionHandler;

// A wrapper around a map for extension API calls. Contains all pending requests
// and the associated context and callback. Designed to be used on a single
// thread, but amongst multiple contexts.
class APIRequestHandler {
 public:
  // TODO(devlin): We may want to coalesce this with the
  // ExtensionHostMsg_Request_Params IPC struct.
  struct Request {
    Request();

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    ~Request();

    int request_id = -1;
    std::string method_name;
    bool has_async_response_handler = false;
    bool has_user_gesture = false;
    base::Value::List arguments_list;
  };

  // Details about a newly-added request to provide as a return to callers.
  // Contains the id of the request and if this is a promise based request, the
  // associated promise.
  struct RequestDetails {
    RequestDetails(int request_id, v8::Local<v8::Promise> promise);
    ~RequestDetails();
    RequestDetails(const RequestDetails& other);

    const int request_id;
    v8::Local<v8::Promise> promise;
  };

  using SendRequestMethod =
      base::RepeatingCallback<void(std::unique_ptr<Request>,
                                   v8::Local<v8::Context>)>;

  APIRequestHandler(SendRequestMethod send_request,
                    APILastError last_error,
                    ExceptionHandler* exception_handler,
                    const InteractionProvider* interaction_provider);

  APIRequestHandler(const APIRequestHandler&) = delete;
  APIRequestHandler& operator=(const APIRequestHandler&) = delete;

  ~APIRequestHandler();

  // Begins the process of processing the request. If this is a promise based
  // request returns the associated promise, otherwise returns an empty promise.
  v8::Local<v8::Promise> StartRequest(
      v8::Local<v8::Context> context,
      const std::string& method,
      base::Value::List arguments_list,
      binding::AsyncResponseType async_type,
      v8::Local<v8::Function> callback,
      v8::Local<v8::Function> custom_callback,
      binding::ResultModifierFunction result_modifier);

  // Adds a pending request for the request handler to manage (and complete via
  // CompleteRequest). This is used by renderer-side implementations that
  // shouldn't be dispatched to the browser in the normal flow, but means other
  // classes don't have to worry about context invalidation. Returns the details
  // of the newly-added request.
  // Note: Unlike StartRequest(), this will not track user gesture state.
  RequestDetails AddPendingRequest(
      v8::Local<v8::Context> context,
      binding::AsyncResponseType async_type,
      v8::Local<v8::Function> callback,
      binding::ResultModifierFunction result_modifier);

  // Responds to the request with the given |request_id|, calling the callback
  // with the given |response| arguments.
  // Invalid ids are ignored.
  // Warning: This can run arbitrary JS code, so the |context| may be
  // invalidated after this!
  void CompleteRequest(int request_id,
                       const base::Value::List& response_list,
                       const std::string& error,
                       mojom::ExtraResponseDataPtr extra_data = nullptr);
  void CompleteRequest(int request_id,
                       const v8::LocalVector<v8::Value>& response,
                       const std::string& error);

  // Invalidates any requests that are associated with |context|.
  void InvalidateContext(v8::Local<v8::Context> context);

  void SetResponseValidator(std::unique_ptr<APIResponseValidator> validator);

  APILastError* last_error() { return &last_error_; }
  int last_sent_request_id() const { return last_sent_request_id_; }
  bool has_response_validator_for_testing() const {
    return response_validator_.get() != nullptr;
  }

  std::set<int> GetPendingRequestIdsForTesting() const;

 private:
  class ArgumentAdapter;
  class AsyncResultHandler;

  struct PendingRequest {
    PendingRequest(
        v8::Isolate* isolate,
        v8::Local<v8::Context> context,
        const std::string& method_name,
        std::unique_ptr<AsyncResultHandler> async_handler,
        std::unique_ptr<InteractionProvider::Token> user_gesture_token);

    ~PendingRequest();
    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    raw_ptr<v8::Isolate> isolate;
    v8::Global<v8::Context> context;
    std::string method_name;

    std::unique_ptr<AsyncResultHandler> async_handler;

    // Note: We can't use std::optional here for derived Token instances.
    std::unique_ptr<InteractionProvider::Token> user_gesture_token;
  };

  // Returns the next request ID to be used.
  int GetNextRequestId();

  // Creates and returns an AsyncResultHandler for a request if the request
  // requires an asynchronous response, otherwise returns null. Also populates
  // |promise_out| with the associated promise if this is a promise based
  // request.
  std::unique_ptr<AsyncResultHandler> GetAsyncResultHandler(
      v8::Local<v8::Context> context,
      binding::AsyncResponseType async_type,
      v8::Local<v8::Function> callback,
      v8::Local<v8::Function> custom_callback,
      binding::ResultModifierFunction result_modifier,
      v8::Local<v8::Promise>* promise_out);

  // Common implementation for completing a request.
  void CompleteRequestImpl(int request_id,
                           ArgumentAdapter arguments,
                           const std::string& error);

  // The next available request identifier.
  int next_request_id_ = 0;

  // The id of the last request we sent to the browser. This can be used as a
  // flag for whether or not a request was sent (if the last_sent_request_id_
  // changes).
  int last_sent_request_id_ = -1;

  // A map of all pending requests.
  std::map<int, PendingRequest> pending_requests_;

  SendRequestMethod send_request_;

  APILastError last_error_;

  // The exception handler for the bindings system; guaranteed to be valid
  // during this object's lifetime.
  const raw_ptr<ExceptionHandler> exception_handler_;

  // The response validator used to check the responses for resolved requests.
  // Null if response validation is disabled.
  std::unique_ptr<APIResponseValidator> response_validator_;

  // Outlives |this|.
  const raw_ptr<const InteractionProvider, DanglingUntriaged>
      interaction_provider_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_REQUEST_HANDLER_H_
