// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_TEST_DELEGATE_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_TEST_DELEGATE_H_

#include <map>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {

// A test class to simply adding custom events or handlers for API hooks.
class APIBindingHooksTestDelegate : public APIBindingHooksDelegate {
 public:
  APIBindingHooksTestDelegate();

  APIBindingHooksTestDelegate(const APIBindingHooksTestDelegate&) = delete;
  APIBindingHooksTestDelegate& operator=(const APIBindingHooksTestDelegate&) =
      delete;

  ~APIBindingHooksTestDelegate() override;

  using CustomEventFactory = base::RepeatingCallback<v8::Local<v8::Value>(
      v8::Local<v8::Context>,
      const std::string& event_name)>;

  using RequestHandler = base::RepeatingCallback<APIBindingHooks::RequestResult(
      const APISignature*,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>*,
      const APITypeReferenceMap&)>;

  using TemplateInitializer =
      base::RepeatingCallback<void(v8::Isolate*,
                                   v8::Local<v8::ObjectTemplate>,
                                   const APITypeReferenceMap&)>;

  using InstanceInitializer =
      base::RepeatingCallback<void(v8::Local<v8::Context>,
                                   v8::Local<v8::Object>)>;

  // Adds a custom |handler| for the method with the given |name|.
  void AddHandler(std::string_view name, RequestHandler handler);

  // Creates events with the given factory.
  void SetCustomEvent(CustomEventFactory custom_event);

  void SetTemplateInitializer(TemplateInitializer initializer);

  void SetInstanceInitializer(InstanceInitializer initializer);

  // APIBindingHooksDelegate:
  bool CreateCustomEvent(v8::Local<v8::Context> context,
                         const std::string& event_name,
                         v8::Local<v8::Value>* event_out) override;
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;
  void InitializeInstance(v8::Local<v8::Context> context,
                          v8::Local<v8::Object> instance) override;

 private:
  std::map<std::string, RequestHandler> request_handlers_;
  CustomEventFactory custom_event_;
  TemplateInitializer template_initializer_;
  InstanceInitializer instance_initializer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_HOOKS_TEST_DELEGATE_H_
