// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_JS_UTIL_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_JS_UTIL_H_

#include <string>

#include "base/macros.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
class APIRequestHandler;
class APITypeReferenceMap;
class ExceptionHandler;

// An object that exposes utility methods to the existing JS bindings, such as
// sendRequest and registering event argument massagers. If/when we get rid of
// some of our JS bindings, we can reduce or remove this class.
class APIBindingJSUtil final : public gin::Wrappable<APIBindingJSUtil> {
 public:
  APIBindingJSUtil(APITypeReferenceMap* type_refs,
                   APIRequestHandler* request_handler,
                   APIEventHandler* event_handler,
                   ExceptionHandler* exception_handler);
  ~APIBindingJSUtil() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

 private:
  // A handler to initiate an API request through the APIRequestHandler. A
  // replacement for custom bindings that utilize require('sendRequest').
  void SendRequest(gin::Arguments* arguments,
                   const std::string& name,
                   const std::vector<v8::Local<v8::Value>>& request_args,
                   v8::Local<v8::Value> options);

  // A handler to register an argument massager for a specific event.
  // Replacement for event_bindings.registerArgumentMassager.
  void RegisterEventArgumentMassager(gin::Arguments* arguments,
                                     const std::string& event_name,
                                     v8::Local<v8::Function> massager);

  // A handler to allow custom bindings to create custom extension API event
  // objects (e.g. foo.onBar).
  void CreateCustomEvent(gin::Arguments* arguments,
                         v8::Local<v8::Value> v8_event_name,
                         bool supports_filters,
                         bool supports_lazy_listeners);

  // Creates a new declarative event.
  void CreateCustomDeclarativeEvent(
      gin::Arguments* arguments,
      const std::string& event_name,
      const std::vector<std::string>& actions_list,
      const std::vector<std::string>& conditions_list,
      int webview_instance_id);

  // Invalidates an event, removing its listeners and preventing any more from
  // being added.
  void InvalidateEvent(gin::Arguments* arguments, v8::Local<v8::Object> event);

  // Sets the last error in the context.
  void SetLastError(gin::Arguments* arguments, const std::string& error);

  // Clears the last error in the context.
  void ClearLastError(gin::Arguments* arguments);

  // Returns true if there is a set lastError in the given context.
  void HasLastError(gin::Arguments* arguments);

  // Sets the lastError in the given context, runs the provided callback, and
  // then clears the last error.
  void RunCallbackWithLastError(gin::Arguments* arguments,
                                const std::string& error,
                                v8::Local<v8::Function> callback);

  // Handles an exception with the given |message| and |exception| value.
  void HandleException(gin::Arguments* arguments,
                       const std::string& message,
                       v8::Local<v8::Value> exception);

  // Sets a custom exception handler to be used when an uncaught exception is
  // found.
  void SetExceptionHandler(gin::Arguments* arguments,
                           v8::Local<v8::Function> handler);

  // Validates a given |value| against the specification for the type with
  // |type_name|. Throws an error if the validation fails; otherwise returns
  // undefined.
  void ValidateType(gin::Arguments* arguments,
                    const std::string& type_name,
                    v8::Local<v8::Value> value);

  // Allows custom bindings to add a signature with the given
  // |custom_signature_name| to use later in argument validation. The signature
  // is expected to be an array of expected types, that can be passed to
  // construct an APISignature.
  void AddCustomSignature(gin::Arguments* arguments,
                          const std::string& custom_signature_name,
                          v8::Local<v8::Value> signature);

  // Looks up the signature with the given |custom_signature_name| and validates
  // |arguments_to_validate| against it, throwing an error if the arguments
  // don't match.
  void ValidateCustomSignature(gin::Arguments* arguments,
                               const std::string& custom_signature_name,
                               v8::Local<v8::Value> arguments_to_validate);

  // Type references. Guaranteed to outlive this object.
  APITypeReferenceMap* const type_refs_;

  // The request handler. Guaranteed to outlive this object.
  APIRequestHandler* const request_handler_;

  // The event handler. Guaranteed to outlive this object.
  APIEventHandler* const event_handler_;

  // The exception handler. Guaranteed to outlive this object.
  ExceptionHandler* const exception_handler_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingJSUtil);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_JS_UTIL_H_
