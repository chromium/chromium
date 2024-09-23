/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

#include <stdlib.h>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

namespace {

constexpr char kContextLabel[] = "V8PerContextData::context_";

}  // namespace

V8PerContextData::V8PerContextData(v8::Local<v8::Context> context)
    : isolate_(context->GetIsolate()),
      context_holder_(std::make_unique<gin::ContextHolder>(isolate_)),
      context_(isolate_, context),
      activity_logger_(nullptr) {
  context_holder_->SetContext(context);
  context_.Get().AnnotateStrongRetainer(kContextLabel);

  if (IsMainThread()) {
    InstanceCounters::IncrementCounter(
        InstanceCounters::kV8PerContextDataCounter);
  }
}

V8PerContextData::~V8PerContextData() {
  if (IsMainThread()) {
    InstanceCounters::DecrementCounter(
        InstanceCounters::kV8PerContextDataCounter);
  }
}

void V8PerContextData::Dispose() {
  // These fields are not traced by the garbage collector and could contain
  // strong GC roots that prevent `this` from otherwise being collected, so
  // explicitly break any potential cycles in the ownership graph now.
  context_holder_ = nullptr;
  if (!context_.IsEmpty())
    context_.SetPhantom();
}

void V8PerContextData::Trace(Visitor* visitor) const {
  visitor->Trace(wrapper_boilerplates_);
  visitor->Trace(constructor_map_);
  visitor->Trace(data_map_);
}

v8::Local<v8::Object> V8PerContextData::CreateWrapperFromCacheSlowCase(
    v8::Isolate* isolate,
    const WrapperTypeInfo* type) {
  DCHECK(!wrapper_boilerplates_.Contains(type));
  v8::Context::Scope scope(GetContext());
  v8::Local<v8::Function> interface_object = ConstructorForType(type);
  if (interface_object.IsEmpty()) [[unlikely]] {
    // For investigation of crbug.com/1199223
    static crash_reporter::CrashKeyString<64> crash_key(
        "blink__create_interface_object");
    crash_key.Set(type->interface_name);
    CHECK(!interface_object.IsEmpty());
  }
  v8::Local<v8::Object> instance_template =
      V8ObjectConstructor::NewInstance(isolate_, interface_object)
          .ToLocalChecked();

  wrapper_boilerplates_.insert(
      type, TraceWrapperV8Reference<v8::Object>(isolate_, instance_template));

  return instance_template->Clone(isolate);
}

v8::Local<v8::Function> V8PerContextData::ConstructorForTypeSlowCase(
    const WrapperTypeInfo* type) {
  DCHECK(!constructor_map_.Contains(type));
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope scope(context);

  v8::Local<v8::Function> parent_interface_object;
  if (auto* parent = type->parent_class) {
    if (parent->is_skipped_in_interface_object_prototype_chain) {
      // This is a special case for WindowProperties.
      // We need to set up the inheritance of Window as the following:
      //   Window.__proto__ === EventTarget
      // although the prototype chain is the following:
      //   Window.prototype.__proto__           === the named properties object
      //   Window.prototype.__proto__.__proto__ === EventTarget.prototype
      // where the named properties object is WindowProperties.prototype in
      // our implementation (although WindowProperties is not JS observable).
      // Let WindowProperties be skipped and make
      // Window.__proto__ == EventTarget.
      DCHECK(parent->parent_class);
      DCHECK(!parent->parent_class
                  ->is_skipped_in_interface_object_prototype_chain);
      parent = parent->parent_class;
    }
    parent_interface_object = ConstructorForType(parent);
  }

  const DOMWrapperWorld& world = DOMWrapperWorld::World(isolate_, context);
  v8::Local<v8::Function> interface_object =
      V8ObjectConstructor::CreateInterfaceObject(
          type, context, world, isolate_, parent_interface_object,
          V8ObjectConstructor::CreationMode::kInstallConditionalFeatures);

  constructor_map_.insert(
      type, TraceWrapperV8Reference<v8::Function>(isolate_, interface_object));

  return interface_object;
}

v8::Local<v8::Object> V8PerContextData::PrototypeForType(
    const WrapperTypeInfo* type) {
  v8::Local<v8::Object> constructor = ConstructorForType(type);
  if (constructor.IsEmpty())
    return v8::Local<v8::Object>();
  v8::Local<v8::Value> prototype_value;
  if (!constructor->Get(GetContext(), V8AtomicString(isolate_, "prototype"))
           .ToLocal(&prototype_value) ||
      !prototype_value->IsObject())
    return v8::Local<v8::Object>();
  return prototype_value.As<v8::Object>();
}

bool V8PerContextData::GetExistingConstructorAndPrototypeForType(
    const WrapperTypeInfo* type,
    v8::Local<v8::Object>* prototype_object,
    v8::Local<v8::Function>* interface_object) {
  auto it = constructor_map_.find(type);
  if (it == constructor_map_.end()) {
    interface_object->Clear();
    prototype_object->Clear();
    return false;
  }
  *interface_object = it->value.Get(isolate_);
  *prototype_object = PrototypeForType(type);
  DCHECK(!prototype_object->IsEmpty());
  return true;
}

void V8PerContextData::AddData(const char* key, Data* data) {
  data_map_.Set(key, data);
}

void V8PerContextData::ClearData(const char* key) {
  data_map_.erase(key);
}

V8PerContextData::Data* V8PerContextData::GetData(const char* key) {
  auto it = data_map_.find(key);
  return it != data_map_.end() ? it->value : nullptr;
}

}  // namespace blink
