// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/common/gin_java_bridge.mojom.h"
#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8-util.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

class GinJavaBridgeObject : public gin::Wrappable<GinJavaBridgeObject>,
                            public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  GinJavaBridgeObject(const GinJavaBridgeObject&) = delete;
  GinJavaBridgeObject& operator=(const GinJavaBridgeObject&) = delete;

  GinJavaBridgeDispatcher::ObjectID object_id() const { return object_id_; }

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override;
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override;

  static GinJavaBridgeObject* InjectNamed(
      blink::WebLocalFrame* frame,
      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
      const std::string& object_name,
      GinJavaBridgeDispatcher::ObjectID object_id);
  static GinJavaBridgeObject* InjectAnonymous(
      blink::WebLocalFrame* frame,
      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
      GinJavaBridgeDispatcher::ObjectID object_id);

  // Returns the bound remote object, nullptr if mojo is disabled.
  mojom::GinJavaBridgeRemoteObject* GetRemote();

 private:
  GinJavaBridgeObject(v8::Isolate* isolate,
                      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
                      GinJavaBridgeDispatcher::ObjectID object_id);
  ~GinJavaBridgeObject() override;

  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(v8::Isolate* isolate,
                                                      const std::string& name);

  base::WeakPtr<GinJavaBridgeDispatcher> dispatcher_;
  GinJavaBridgeDispatcher::ObjectID object_id_;
  std::map<std::string, bool> known_methods_;
  v8::StdGlobalValueMap<std::string, v8::FunctionTemplate> template_cache_;
  mojo::Remote<mojom::GinJavaBridgeRemoteObject> remote_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_
