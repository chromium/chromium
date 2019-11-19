// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "v8/include/v8.h"

namespace extensions {
class APIBindingHooksDelegate;
class APITypeReferenceMap;
class APISignature;

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
    RequestResult(std::string invocation_error);
    RequestResult(const RequestResult& other);
    ~RequestResult();

    ResultCode code;
    v8::Local<v8::Function> custom_callback;
    v8::Local<v8::Value> return_value;  // Only valid if code == HANDLED.
    std::string error;
  };

  explicit APIBindingHooks(const std::string& api_name);
  ~APIBindingHooks();

  // Looks for any custom hooks associated with the given request, and, if any
  // are found, runs them. Returns the result of running the hooks, if any.
  RequestResult RunHooks(const std::string& method_name,
                         v8::Local<v8::Context> context,
                         const APISignature* signature,
                         std::vector<v8::Local<v8::Value>>* arguments,
                         const APITypeReferenceMap& type_refs);

  // Returns a JS interface that can be used to register hooks.
  v8::Local<v8::Object> GetJSHookInterface(v8::Local<v8::Context> context);

  // Gets the custom-set JS callback for the given method, if one exists.
  v8::Local<v8::Function> GetCustomJSCallback(const std::string& method_name,
                                              v8::Local<v8::Context> context);

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
                       std::vector<v8::Local<v8::Value>>* arguments);

  // The name of the associated API.
  std::string api_name_;

  std::unique_ptr<APIBindingHooksDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingHooks);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_H_
