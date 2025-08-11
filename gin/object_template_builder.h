// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_OBJECT_TEMPLATE_BUILDER_H_
#define GIN_OBJECT_TEMPLATE_BUILDER_H_

#include <string_view>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/gin_export.h"
#include "v8/include/v8-forward.h"

namespace gin {

namespace internal {

template <typename T>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(v8::Isolate* isolate,
                                                       T callback,
                                                       const char* type_name) {
  // We need to handle member function pointers case specially because the first
  // parameter for callbacks to MFP should typically come from the the
  // JavaScript "this" object the function was called on, not from the first
  // normal parameter.
  InvokerOptions options;
  if (std::is_member_function_pointer<T>::value) {
    options.holder_is_first_argument = true;
    options.holder_type = type_name;
  }
  return ::gin::CreateFunctionTemplate(
      isolate, base::BindRepeating(std::move(callback)), std::move(options));
}

}  // namespace internal

// ObjectTemplateBuilder provides a handy interface to creating
// v8::ObjectTemplate instances with various sorts of properties.
class GIN_EXPORT ObjectTemplateBuilder {
 public:
  explicit ObjectTemplateBuilder(v8::Isolate* isolate);
  ObjectTemplateBuilder(v8::Isolate* isolate, const char* type_name);
  ObjectTemplateBuilder(const ObjectTemplateBuilder& other);
  ~ObjectTemplateBuilder();

  // It's against Google C++ style to return a non-const ref, but we take some
  // poetic license here in order that all calls to Set() can be via the '.'
  // operator and line up nicely.
  template <typename T>
  ObjectTemplateBuilder& SetValue(const std::string_view& name, T val) {
    return SetImpl(name, ConvertToV8(isolate_, val));
  }

  // In the following methods, T and U can be function pointer, member function
  // pointer, base::RepeatingCallback, or v8::FunctionTemplate. Most clients
  // will want to use one of the first two options. Also see
  // gin::CreateFunctionTemplate() for creating raw function templates.
  template <typename T>
  ObjectTemplateBuilder& SetMethod(const std::string_view& name,
                                   const T& callback) {
    return SetImpl(
        name, internal::CreateFunctionTemplate(isolate_, callback, type_name_));
  }

  template <typename T>
  ObjectTemplateBuilder& SetMethod(v8::Local<v8::Name> name,
                                   const T& callback) {
    return SetImpl(
        name, internal::CreateFunctionTemplate(isolate_, callback, type_name_));
  }

  template <typename T>
  ObjectTemplateBuilder& SetProperty(const std::string_view& name,
                                     const T& getter) {
    return SetPropertyImpl(
        name, internal::CreateFunctionTemplate(isolate_, getter, type_name_),
        v8::Local<v8::FunctionTemplate>());
  }
  template <typename T, typename U>
  ObjectTemplateBuilder& SetProperty(const std::string_view& name,
                                     const T& getter,
                                     const U& setter) {
    return SetPropertyImpl(
        name, internal::CreateFunctionTemplate(isolate_, getter, type_name_),
        internal::CreateFunctionTemplate(isolate_, setter, type_name_));
  }

  // Whereas SetProperty creates an accessor property, this creates what appears
  // to be a data property but whose value is lazily computed the first time the
  // [[Get]] operation occurs.
  template <typename T>
  ObjectTemplateBuilder& SetLazyDataProperty(const std::string_view& name,
                                             const T& getter) {
    InvokerOptions options;
    if (std::is_member_function_pointer<T>::value) {
      options.holder_is_first_argument = true;
      options.holder_type = type_name_;
    }
    auto [callback, data] = CreateDataPropertyCallback(
        isolate_, base::BindRepeating(getter), std::move(options));
    return SetLazyDataPropertyImpl(name, callback, data);
  }

  ObjectTemplateBuilder& AddNamedPropertyInterceptor();
  ObjectTemplateBuilder& AddIndexedPropertyInterceptor();

  v8::Local<v8::ObjectTemplate> Build();

 private:
  ObjectTemplateBuilder& SetImpl(const std::string_view& name,
                                 v8::Local<v8::Data> val);
  ObjectTemplateBuilder& SetImpl(v8::Local<v8::Name> name,
                                 v8::Local<v8::Data> val);
  ObjectTemplateBuilder& SetPropertyImpl(
      const std::string_view& name,
      v8::Local<v8::FunctionTemplate> getter,
      v8::Local<v8::FunctionTemplate> setter);
  ObjectTemplateBuilder& SetLazyDataPropertyImpl(
      const std::string_view& name,
      v8::AccessorNameGetterCallback callback,
      v8::Local<v8::Value> data);

  raw_ptr<v8::Isolate> isolate_;

  // If provided, |type_name_| will be used to give a user-friendly error
  // message if a member function is invoked on the wrong type of object.
  const char* type_name_ = nullptr;

  // ObjectTemplateBuilder should only be used on the stack.
  v8::Local<v8::FunctionTemplate> constructor_template_;
  v8::Local<v8::ObjectTemplate> template_;
};

}  // namespace gin

#endif  // GIN_OBJECT_TEMPLATE_BUILDER_H_
