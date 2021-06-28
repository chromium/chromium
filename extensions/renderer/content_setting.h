// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONTENT_SETTING_H_
#define EXTENSIONS_RENDERER_CONTENT_SETTING_H_

#include <string>

#include "base/macros.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
class APIRequestHandler;
class BindingAccessChecker;

// The custom implementation of the contentSettings.ContentSetting type exposed
// to APIs.
class ContentSetting final : public gin::Wrappable<ContentSetting> {
 public:
  ~ContentSetting() override;

  // Creates a ContentSetting object for the given property.
  static v8::Local<v8::Object> Create(
      v8::Isolate* isolate,
      const std::string& property_name,
      const base::ListValue* property_values,
      APIRequestHandler* request_handler,
      APIEventHandler* event_handler,
      APITypeReferenceMap* type_refs,
      const BindingAccessChecker* access_checker);

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

 private:
  ContentSetting(APIRequestHandler* request_handler,
                 const APITypeReferenceMap* type_refs,
                 const BindingAccessChecker* access_checker,
                 const std::string& pref_name,
                 const base::DictionaryValue& argument_spec);

  // JS function handlers:
  void Get(gin::Arguments* arguments);
  void Set(gin::Arguments* arguments);
  void Clear(gin::Arguments* arguments);
  void GetResourceIdentifiers(gin::Arguments* arguments);

  // Common function handling endpoint.
  void HandleFunction(const std::string& function_name,
                      gin::Arguments* arguments);

  APIRequestHandler* request_handler_;

  const APITypeReferenceMap* type_refs_;

  const BindingAccessChecker* const access_checker_;

  // The name of the preference this ContentSetting is managing.
  std::string pref_name_;

  // The type of argument that calling set() on the ContentSetting expects
  // (since different settings can take a different type of argument depending
  // on the preference it manages).
  ArgumentSpec argument_spec_;

  DISALLOW_COPY_AND_ASSIGN(ContentSetting);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_CONTENT_SETTING_H_
