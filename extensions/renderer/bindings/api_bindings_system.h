// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/renderer/bindings/api_binding.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_last_error.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "extensions/renderer/bindings/exception_handler.h"

namespace extensions {
class APIBindingHooks;
class APIBindingHooksDelegate;
class InteractionProvider;

// A class encompassing the necessary pieces to construct the JS entry points
// for Extension APIs. Designed to be used on a single thread, but safe between
// multiple v8::Contexts.
class APIBindingsSystem {
 public:
  using GetAPISchemaMethod =
      base::RepeatingCallback<const base::Value::Dict&(const std::string&)>;
  using CustomTypeHandler = base::RepeatingCallback<v8::Local<v8::Object>(
      v8::Isolate* isolate,
      const std::string& property_name,
      const base::Value::List* property_values,
      APIRequestHandler* request_handler,
      APIEventHandler* event_handler,
      APITypeReferenceMap* type_refs,
      const BindingAccessChecker* access_checker)>;

  APIBindingsSystem(
      GetAPISchemaMethod get_api_schema,
      BindingAccessChecker::APIAvailabilityCallback api_available,
      BindingAccessChecker::PromiseAvailabilityCallback promises_available,
      APIRequestHandler::SendRequestMethod send_request,
      std::unique_ptr<InteractionProvider> interaction_provider,
      APIEventListeners::ListenersUpdated event_listeners_changed,
      APIEventHandler::ContextOwnerIdGetter context_owner_getter,
      APIBinding::OnSilentRequest on_silent_request,
      binding::AddConsoleError add_console_error,
      APILastError last_error);

  APIBindingsSystem(const APIBindingsSystem&) = delete;
  APIBindingsSystem& operator=(const APIBindingsSystem&) = delete;

  ~APIBindingsSystem();

  // Returns a new v8::Object representing the api specified by |api_name|.
  v8::Local<v8::Object> CreateAPIInstance(
      const std::string& api_name,
      v8::Local<v8::Context> context,
      APIBindingHooks** hooks_out);

  // Responds to the request with the given |request_id|, calling the callback
  // with |response|. If |error| is non-empty, sets the last error.
  void CompleteRequest(int request_id,
                       const base::Value::List& response,
                       const std::string& error,
                       mojom::ExtraResponseDataPtr extra_data = nullptr);

  // Notifies the APIEventHandler to fire the corresponding event, notifying
  // listeners.
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          const base::Value::List& response,
                          mojom::EventFilteringInfoPtr filter);

  // Registers the custom hook on the APIBindingHooks object for the given API.
  // These must be registered *before* the binding is instantiated.
  void RegisterHooksDelegate(const std::string& api_name,
                             std::unique_ptr<APIBindingHooksDelegate> delegate);

  // Registers the handler for creating a custom type with the given
  // |type_name|, where |type_name| is the fully-qualified type (e.g.
  // storage.StorageArea).
  void RegisterCustomType(const std::string& type_name,
                          CustomTypeHandler function);

  // Handles any cleanup necessary before releasing the given |context|.
  void WillReleaseContext(v8::Local<v8::Context> context);

  InteractionProvider* interaction_provider() {
    return interaction_provider_.get();
  }
  APIRequestHandler* request_handler() { return &request_handler_; }
  APIEventHandler* event_handler() { return &event_handler_; }
  APITypeReferenceMap* type_reference_map() { return &type_reference_map_; }
  ExceptionHandler* exception_handler() { return &exception_handler_; }

 private:
  // Creates a new APIBinding for the given |api_name|.
  std::unique_ptr<APIBinding> CreateNewAPIBinding(const std::string& api_name);

  // Callback for the APITypeReferenceMap in order to initialize an unknown
  // type.
  void InitializeType(const std::string& name);

  // Handles creating the type for the specified property.
  v8::Local<v8::Object> CreateCustomType(
      v8::Isolate* isolate,
      const std::string& type_name,
      const std::string& property_name,
      const base::Value::List* property_values);

  // The map of cached API reference types.
  APITypeReferenceMap type_reference_map_;

  // The exception handler for the system.
  ExceptionHandler exception_handler_;

  // The interaction provider for the system.
  std::unique_ptr<InteractionProvider> interaction_provider_;

  // The request handler associated with the system.
  APIRequestHandler request_handler_;

  // The event handler associated with the system.
  APIEventHandler event_handler_;

  // The access checker associated with the system.
  BindingAccessChecker access_checker_;

  // A map from api_name -> APIBinding for constructed APIs. APIBindings are
  // created lazily.
  std::map<std::string, std::unique_ptr<APIBinding>> api_bindings_;

  // A map from api_name -> APIBindingHooks for registering custom hooks.
  // TODO(devlin): This map is pretty pointer-y. Is that going to be a
  // performance concern?
  std::map<std::string, std::unique_ptr<APIBindingHooks>> binding_hooks_;

  std::map<std::string, CustomTypeHandler> custom_types_;

  // The method to retrieve the dictionary describing a given extension
  // API. Curried in for testing purposes so we can use fake APIs.
  GetAPISchemaMethod get_api_schema_;

  // The method to call when the system silently handles an API request without
  // notifying the browser.
  APIBinding::OnSilentRequest on_silent_request_;
};

}  // namespace

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_H_
