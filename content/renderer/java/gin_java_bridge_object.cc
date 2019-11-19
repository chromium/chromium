// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_object.h"

#include "base/bind.h"
#include "content/common/gin_java_bridge_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/java/gin_java_function_invocation_helper.h"
#include "gin/function_template.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

// static
GinJavaBridgeObject* GinJavaBridgeObject::InjectNamed(
    blink::WebLocalFrame* frame,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    const std::string& object_name,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return NULL;

  GinJavaBridgeObject* object =
      new GinJavaBridgeObject(isolate, dispatcher, object_id);

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  gin::Handle<GinJavaBridgeObject> controller =
      gin::CreateHandle(isolate, object);
  // WrappableBase instance deletes itself in case of a wrapper
  // creation failure, thus there is no need to delete |object|.
  if (controller.IsEmpty())
    return NULL;

  global->Set(context, gin::StringToV8(isolate, object_name), controller.ToV8())
      .Check();

  return object;
}

// static
GinJavaBridgeObject* GinJavaBridgeObject::InjectAnonymous(
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  return new GinJavaBridgeObject(blink::MainThreadIsolate(), dispatcher,
                                 object_id);
}

GinJavaBridgeObject::GinJavaBridgeObject(
    v8::Isolate* isolate,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id)
    : gin::NamedPropertyInterceptor(isolate, this),
      dispatcher_(dispatcher),
      object_id_(object_id),
      frame_routing_id_(dispatcher_->routing_id()),
      template_cache_(isolate) {
}

GinJavaBridgeObject::~GinJavaBridgeObject() {
  if (dispatcher_) {
    dispatcher_->OnGinJavaBridgeObjectDeleted(this);
  } else {
    // A wrapper can outlive a render frame, and thus the dispatcher.
    // Note that we intercept GinJavaBridgeHostMsg messages in a browser filter
    // thus it's OK to send the message with a routing id of a ceased frame.
    RenderThread::Get()->Send(new GinJavaBridgeHostMsg_ObjectWrapperDeleted(
        frame_routing_id_, object_id_));
  }
}

gin::ObjectTemplateBuilder GinJavaBridgeObject::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GinJavaBridgeObject>::GetObjectTemplateBuilder(isolate)
      .AddNamedPropertyInterceptor();
}

v8::Local<v8::Value> GinJavaBridgeObject::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  std::map<std::string, bool>::iterator method_pos =
      known_methods_.find(property);
  if (method_pos == known_methods_.end()) {
    if (!dispatcher_) {
      return v8::Local<v8::Value>();
    }
    known_methods_[property] = dispatcher_->HasJavaMethod(object_id_, property);
  }
  if (known_methods_[property]) {
    return GetFunctionTemplate(isolate, property)
        ->GetFunction(isolate->GetCurrentContext())
        .FromMaybe(v8::Local<v8::Value>());
  } else {
    return v8::Local<v8::Value>();
  }
}

std::vector<std::string> GinJavaBridgeObject::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  std::set<std::string> method_names;
  if (dispatcher_)
    dispatcher_->GetJavaMethods(object_id_, &method_names);
  return std::vector<std::string> (method_names.begin(), method_names.end());
}

v8::Local<v8::FunctionTemplate> GinJavaBridgeObject::GetFunctionTemplate(
    v8::Isolate* isolate,
    const std::string& name) {
  v8::Local<v8::FunctionTemplate> function_template = template_cache_.Get(name);
  if (!function_template.IsEmpty())
    return function_template;
  function_template = gin::CreateFunctionTemplate(
      isolate,
      base::BindRepeating(
          &GinJavaFunctionInvocationHelper::Invoke,
          base::Owned(new GinJavaFunctionInvocationHelper(name, dispatcher_))));
  template_cache_.Set(name, function_template);
  return function_template;
}

gin::WrapperInfo GinJavaBridgeObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace content
