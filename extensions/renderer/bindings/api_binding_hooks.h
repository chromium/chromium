// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace extensions {
class APIBindingHooksDelegate;
class APITypeReferenceMap;
class APISignature;
class APIRequestHandler;

// A class to register custom hooks for given API calls that need different
// handling. An instance exists for a single API, but can be used across
// multiple contexts (but only on the same thread).
// TODO(devlin): We have both C++ and JS custom bindings, but this only allows
// for registration of C++ handlers. Add JS support.
class APIBindingHooks {
 public:
  // The result of checking for hooks to handle a request.
  struct RequestResult {
    enum ResultCode {
      HANDLED,              // A custom hook handled the request.
      ARGUMENTS_UPDATED,    // The arguments were updated post-validation.
      THROWN,               // An exception was thrown during parsing or
                            // handling.
      INVALID_INVOCATION,   // The request was called with invalid arguments.
                            // |error| will contain the invocation error.
      CONTEXT_INVALIDATED,  // The context was invalidated during the handling
                            // of the API. Ideally, this wouldn't happen, but
                            // could in certain circumstances.
      NOT_HANDLED,          // The request was not handled.
    };

    explicit RequestResult(ResultCode code);
    RequestResult(ResultCode code, v8::Local<v8::Function> custom_callback);
    RequestResult(ResultCode code,
                  v8::Local<v8::Function> custom_callback,
                  binding::ResultModifierFunction result_modifier);
    explicit RequestResult(std::string invocation_error);
    RequestResult(RequestResult&& other);
    ~RequestResult();

    ResultCode code;
    v8::Local<v8::Function> custom_callback;
    binding::ResultModifierFunction result_modifier;
    v8::Local<v8::Value> return_value;  // Only valid if code == HANDLED.
    std::string error;
  };

  APIBindingHooks(const std::string& api_name,
                  APIRequestHandler* request_handler);

  APIBindingHooks(const APIBindingHooks&) = delete;
  APIBindingHooks& operator=(const APIBindingHooks&) = delete;

  ~APIBindingHooks();

  // Looks for any custom hooks associated with the given request, and, if any
  // are found, runs them. Returns the result of running the hooks, if any.
  RequestResult RunHooks(const std::string& method_name,
                         v8::Local<v8::Context> context,
                         const APISignature* signature,
                         v8::LocalVector<v8::Value>* arguments,
                         const APITypeReferenceMap& type_refs);

  // Handler function to resolve asynchronous requests associated with handle
  // request hooks.
  void CompleteHandleRequest(int request_id,
                             bool did_succeed,
                             gin::Arguments* arguments);

  // Returns a JS interface that can be used to register hooks.
  v8::Local<v8::Object> GetJSHookInterface(v8::Local<v8::Context> context);

  // Creates a new JS event for the given |event_name|, if a custom event is
  // provided. Returns true if an event was created.
  bool CreateCustomEvent(v8::Local<v8::Context> context,
                         const std::string& event_name,
                         v8::Local<v8::Value>* event_out);

  // Performs any extra initialization on the template.
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs);

  // Performs any extra initialization on an instance of the API.
  void InitializeInstance(v8::Local<v8::Context> context,
                          v8::Local<v8::Object> instance);

  void SetDelegate(std::unique_ptr<APIBindingHooksDelegate> delegate);

 private:
  // Updates the |arguments| by running |function| and settings arguments to the
  // returned result.
  bool UpdateArguments(v8::Local<v8::Function> function,
                       v8::Local<v8::Context> context,
                       v8::LocalVector<v8::Value>* arguments);

  // The name of the associated API.
  std::string api_name_;

  // The request handler used to resolve asynchronous responses associated with
  // handle request hooks. Guaranteed to outlive this object.
  const raw_ptr<APIRequestHandler, DanglingUntriaged> request_handler_;

  std::unique_ptr<APIBindingHooksDelegate> delegate_;

  base::WeakPtrFactory<APIBindingHooks> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_
