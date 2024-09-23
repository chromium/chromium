// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIBindingHooks;
class APIEventHandler;
class APIRequestHandler;
class APISignature;
class APITypeReferenceMap;
class BindingAccessChecker;

namespace binding {
enum class RequestThread;
}

// A class that vends v8::Objects for extension APIs. These APIs have function
// interceptors for all exposed methods, which call back into the APIBinding.
// The APIBinding then matches the calling arguments against an expected method
// signature, throwing an error if they don't match.
// There should only need to be a single APIBinding object for each API, and
// each can vend multiple v8::Objects for different contexts.
// This object is designed to be one-per-isolate, but used across separate
// contexts.
class APIBinding {
 public:
  using CreateCustomType = base::RepeatingCallback<v8::Local<v8::Object>(
      v8::Isolate* isolate,
      const std::string& type_name,
      const std::string& property_name,
      const base::Value::List* property_values)>;

  // Called when a request is handled without notifying the browser.
  using OnSilentRequest = base::RepeatingCallback<void(
      v8::Local<v8::Context>,
      const std::string& name,
      const v8::LocalVector<v8::Value>& arguments)>;

  // The callback type for handling an API call.
  using HandlerCallback = base::RepeatingCallback<void(gin::Arguments*)>;

  // The APITypeReferenceMap is required to outlive this object.
  // |function_definitions|, |type_definitions| and |event_definitions|
  // may be null if the API does not specify any of that category.
  APIBinding(const std::string& name,
             const base::Value::List* function_definitions,
             const base::Value::List* type_definitions,
             const base::Value::List* event_definitions,
             const base::Value::Dict* property_definitions,
             CreateCustomType create_custom_type,
             OnSilentRequest on_silent_request,
             std::unique_ptr<APIBindingHooks> binding_hooks,
             APITypeReferenceMap* type_refs,
             APIRequestHandler* request_handler,
             APIEventHandler* event_handler,
             BindingAccessChecker* access_checker);

  APIBinding(const APIBinding&) = delete;
  APIBinding& operator=(const APIBinding&) = delete;

  ~APIBinding();

  // Returns a new v8::Object for the API this APIBinding represents.
  v8::Local<v8::Object> CreateInstance(v8::Local<v8::Context> context);

  APIBindingHooks* hooks() { return binding_hooks_.get(); }

 private:
  // Initializes the object_template_ for this API. Called lazily when the
  // first instance is created.
  void InitializeTemplate(v8::Isolate* isolate);

  // Decorates |object_template| with the properties specified by |properties|.
  // |is_root| is used to determine whether to add the properties to
  // |root_properties_|.
  void DecorateTemplateWithProperties(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate> object_template,
      const base::Value::Dict& properties,
      bool is_root);

  // Handler for getting the v8::Object associated with an event on the API.
  static void GetEventObject(v8::Local<v8::Name>,
                             const v8::PropertyCallbackInfo<v8::Value>& info);

  // Handler for getting the v8::Object associated with a custom property on the
  // API.
  static void GetCustomPropertyObject(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  // Handles calling of an API method with the given |name| on the given
  // |thread| and matches the arguments against |signature|.
  void HandleCall(const std::string& name,
                  const APISignature* signature,
                  gin::Arguments* args);

  // The root name of the API, e.g. "tabs" for chrome.tabs.
  std::string api_name_;

  // A map from method name to method data.
  struct MethodData;
  std::map<std::string, std::unique_ptr<MethodData>> methods_;

  // The events associated with this API.
  struct EventData;
  std::vector<std::unique_ptr<EventData>> events_;

  // The custom properties on the API; these are rare.
  struct CustomPropertyData;
  std::vector<std::unique_ptr<CustomPropertyData>> custom_properties_;

  // The pair for enum entry is <original, js-ified>. JS enum entries use
  // SCREAMING_STYLE (whereas our API enums are just inconsistent).
  using EnumEntry = std::pair<std::string, std::string>;
  // A map of <name, values> for the enums on this API.
  std::map<std::string, std::vector<EnumEntry>> enums_;

  // The associated properties of the API, if any.
  raw_ptr<const base::Value::Dict> property_definitions_;
  // The names of all the "root properties" added to the API; i.e., properties
  // exposed on the API object itself.
  base::flat_set<std::string> root_properties_;

  // The callback for constructing a custom type.
  CreateCustomType create_custom_type_;

  OnSilentRequest on_silent_request_;

  // The registered hooks for this API.
  std::unique_ptr<APIBindingHooks> binding_hooks_;

  // The reference map for all known types; required to outlive this object.
  raw_ptr<APITypeReferenceMap> type_refs_;

  // The associated request handler, shared between this and other bindings.
  // Required to outlive this object.
  raw_ptr<APIRequestHandler, DanglingUntriaged> request_handler_;

  // The associated event handler, shared between this and other bindings.
  // Required to outlive this object.
  raw_ptr<APIEventHandler, DanglingUntriaged> event_handler_;

  // The associated access checker; required to outlive this object.
  const raw_ptr<const BindingAccessChecker, DanglingUntriaged> access_checker_;

  // The template for this API. Note: some methods may only be available in
  // certain contexts, but this template contains all methods. Those that are
  // unavailable are removed after object instantiation.
  v8::Eternal<v8::ObjectTemplate> object_template_;

  base::WeakPtrFactory<APIBinding> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_H_
