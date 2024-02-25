// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gin/interceptor.h"
#include "gin/wrappable.h"
#include "ppapi/c/pp_var.h"
#include "v8/include/v8-util.h"

struct PPP_Class_Deprecated;

namespace gin {
  class Arguments;
}  // namespace gin

namespace content {

class PepperPluginInstanceImpl;

// A PluginObject is a JS-accessible object implemented by the plugin.
//
// In contrast, a var of type PP_VARTYPE_OBJECT is a reference to a JS object,
// which might be implemented by the plugin (here) or by the JS engine.
class PluginObject : public gin::Wrappable<PluginObject>,
                     public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  PluginObject(const PluginObject&) = delete;
  PluginObject& operator=(const PluginObject&) = delete;

  ~PluginObject() override;

  // Returns the PluginObject which is contained in the given v8 object, or NULL
  // if the object isn't backed by a PluginObject.
  static PluginObject* FromV8Object(v8::Isolate* isolate,
                                    v8::Local<v8::Object> v8_object);

  // Allocates a new PluginObject and returns it as a PP_Var with a
  // refcount of 1.
  static PP_Var Create(PepperPluginInstanceImpl* instance,
                       const PPP_Class_Deprecated* ppp_class,
                       void* ppp_class_data);

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override;
  bool SetNamedProperty(v8::Isolate* isolate,
                        const std::string& property,
                        v8::Local<v8::Value> value) override;
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override;

  const PPP_Class_Deprecated* ppp_class() { return ppp_class_; }
  void* ppp_class_data() { return ppp_class_data_; }

  // Called when the instance is destroyed.
  void InstanceDeleted();

 private:
  PluginObject(PepperPluginInstanceImpl* instance,
               const PPP_Class_Deprecated* ppp_class,
               void* ppp_class_data);

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Helper method to get named properties.
  v8::Local<v8::Value> GetPropertyOrMethod(v8::Isolate* isolate,
                                           PP_Var identifier_var);

  void Call(const std::string& identifier, gin::Arguments* args);

  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(v8::Isolate* isolate,
                                                      const std::string& name);

  raw_ptr<PepperPluginInstanceImpl> instance_;

  raw_ptr<const PPP_Class_Deprecated> ppp_class_;
  raw_ptr<void, DanglingUntriaged> ppp_class_data_;

  v8::StdGlobalValueMap<std::string, v8::FunctionTemplate> template_cache_;

  base::WeakPtrFactory<PluginObject> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_
