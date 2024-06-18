// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_object.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/java/gin_java_function_invocation_helper.h"
#include "gin/function_template.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-local-handle.h"

namespace content {

// static
GinJavaBridgeObject* GinJavaBridgeObject::InjectNamed(
    blink::WebLocalFrame* frame,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    const std::string& object_name,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
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
    blink::WebLocalFrame* frame,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  return new GinJavaBridgeObject(frame->GetAgentGroupScheduler()->Isolate(),
                                 dispatcher, object_id);
}

GinJavaBridgeObject::GinJavaBridgeObject(
    v8::Isolate* isolate,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id)
    : gin::NamedPropertyInterceptor(isolate, this),
      dispatcher_(dispatcher),
      object_id_(object_id),
      template_cache_(isolate) {
  dispatcher_->GetRemoteObjectHost()->GetObject(
      object_id, remote_.BindNewPipeAndPassReceiver());
}

GinJavaBridgeObject::~GinJavaBridgeObject() {
  if (dispatcher_) {
    dispatcher_->OnGinJavaBridgeObjectDeleted(this);
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
  if (!base::Contains(known_methods_, property)) {
    if (!dispatcher_) {
      return v8::Local<v8::Value>();
    }
    if (remote_) {
      bool result = false;
      remote_->HasMethod(property, &result);
      known_methods_[property] = result;
    }
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
  std::vector<std::string> method_names;
  if (remote_) {
    remote_->GetMethods(&method_names);
  }
  return method_names;
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

mojom::GinJavaBridgeRemoteObject* GinJavaBridgeObject::GetRemote() {
  if (!remote_) {
    // non-mojo case.
    return nullptr;
  }
  return remote_.get();
}

gin::WrapperInfo GinJavaBridgeObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace content
