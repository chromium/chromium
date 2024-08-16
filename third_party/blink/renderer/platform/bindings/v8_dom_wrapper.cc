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

v8::Local<v8::Object> V8DOMWrapper::CreateWrapper(ScriptState* script_state,
                                                  const WrapperTypeInfo* type) {
  auto* isolate = script_state->GetIsolate();
  RUNTIME_CALL_TIMER_SCOPE(isolate,
                           RuntimeCallStats::CounterId::kCreateWrapper);

  const V8WrapperInstantiationScope scope(script_state);

  v8::Local<v8::Object> wrapper;
  auto* per_context_data = script_state->PerContextData();
  if (per_context_data) [[likely]] {
    wrapper = per_context_data->CreateWrapperFromCache(isolate, type);
    CHECK(!wrapper.IsEmpty());
  } else {
    // The context is detached, but still accessible.
    // TODO(yukishiino): This code does not create a wrapper with
    // the correct settings.  Should follow the same way as
    // V8PerContextData::createWrapperFromCache, though there is no need to
    // cache resulting objects or their constructors.
    const DOMWrapperWorld& world = script_state->World();
    wrapper = type->GetV8ClassTemplate(isolate, world)
                  .As<v8::FunctionTemplate>()
                  ->InstanceTemplate()
                  ->NewInstance(scope.GetContext())
                  .ToLocalChecked();
  }
  return wrapper;
}

bool V8DOMWrapper::IsWrapper(v8::Isolate* isolate,
                             v8::Local<v8::Object> object) {
  CHECK(!object.IsEmpty());

  if (!object->IsApiWrapper()) {
    return false;
  }

  // TODO(b/328117814): this works as long as other embedders within the
  // renderer process are not using new wrappers. We will need to come up
  // with a friend-or-foe identification when we switch gin to new wrappers.
  if (WrapperTypeInfo::HasLegacyInternalFieldsSet(object)) {
    return false;
  }

  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  if (!(untrusted_wrapper_type_info && per_isolate_data))
    return false;
  return per_isolate_data->HasInstanceOfUntrustedType(
      untrusted_wrapper_type_info, object);
}

bool V8DOMWrapper::HasInternalFieldsSet(v8::Isolate* isolate,
                                        v8::Local<v8::Object> object) {
  CHECK(!object.IsEmpty());

  if (!object->IsApiWrapper())
    return false;
  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  return untrusted_wrapper_type_info &&
         untrusted_wrapper_type_info->gin_embedder == gin::kEmbedderBlink;
}

}  // namespace blink
