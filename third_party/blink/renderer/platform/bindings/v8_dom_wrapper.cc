/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

v8::Local<v8::Object> V8DOMWrapper::CreateWrapper(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* type) {
  RUNTIME_CALL_TIMER_SCOPE(isolate,
                           RuntimeCallStats::CounterId::kCreateWrapper);

  // TODO(adithyas): We should abort wrapper creation if the context access
  // check fails and throws an exception.
  V8WrapperInstantiationScope scope(creation_context, isolate, type);
  CHECK(!scope.AccessCheckFailed());

  V8PerContextData* per_context_data =
      V8PerContextData::From(scope.GetContext());
  v8::Local<v8::Object> wrapper;
  if (per_context_data) {
    wrapper = per_context_data->CreateWrapperFromCache(type);
    CHECK(!wrapper.IsEmpty());
  } else {
    // The context is detached, but still accessible.
    // TODO(yukishiino): This code does not create a wrapper with
    // the correct settings.  Should follow the same way as
    // V8PerContextData::createWrapperFromCache, though there is no need to
    // cache resulting objects or their constructors.
    const DOMWrapperWorld& world = DOMWrapperWorld::World(scope.GetContext());
    wrapper = type->DomTemplate(isolate, world)
                  ->InstanceTemplate()
                  ->NewInstance(scope.GetContext())
                  .ToLocalChecked();
  }
  return wrapper;
}

bool V8DOMWrapper::IsWrapper(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsObject())
    return false;

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  if (!object->IsApiWrapper())
    return false;

  if (object->InternalFieldCount() < kV8DefaultWrapperInternalFieldCount)
    return false;

  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  if (!(untrusted_wrapper_type_info && per_isolate_data))
    return false;
  return per_isolate_data->HasInstance(untrusted_wrapper_type_info, object);
}

bool V8DOMWrapper::HasInternalFieldsSet(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsObject())
    return false;

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  if (!object->IsApiWrapper())
    return false;

  if (object->InternalFieldCount() < kV8DefaultWrapperInternalFieldCount)
    return false;

  // The untyped wrappable can either be ScriptWrappable or CustomWrappable.
  const void* untrused_wrappable = ToUntypedWrappable(object);
  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  return untrused_wrappable && untrusted_wrapper_type_info &&
         untrusted_wrapper_type_info->gin_embedder == gin::kEmbedderBlink;
}

}  // namespace blink
