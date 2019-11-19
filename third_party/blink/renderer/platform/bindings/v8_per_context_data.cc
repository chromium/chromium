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
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v0_custom_element_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

namespace {

constexpr char kWrapperBoilerplatesLabel[] =
    "V8PerContextData::wrapper_boilerplates_";
constexpr char kConstructorMapLabel[] = "V8PerContextData::constructor_map_";
constexpr char kContextLabel[] = "V8PerContextData::context_";

}  // namespace

V8PerContextData::V8PerContextData(v8::Local<v8::Context> context)
    : isolate_(context->GetIsolate()),
      wrapper_boilerplates_(isolate_, kWrapperBoilerplatesLabel),
      constructor_map_(isolate_, kConstructorMapLabel),
      context_holder_(std::make_unique<gin::ContextHolder>(isolate_)),
      context_(isolate_, context),
      activity_logger_(nullptr),
      data_map_(MakeGarbageCollected<DataMap>()) {
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

V8PerContextData* V8PerContextData::From(v8::Local<v8::Context> context) {
  return ScriptState::From(context)->PerContextData();
}

v8::Local<v8::Object> V8PerContextData::CreateWrapperFromCacheSlowCase(
    const WrapperTypeInfo* type) {
  v8::Context::Scope scope(GetContext());
  v8::Local<v8::Function> interface_object = ConstructorForType(type);
  CHECK(!interface_object.IsEmpty());
  v8::Local<v8::Object> instance_template =
      V8ObjectConstructor::NewInstance(isolate_, interface_object)
          .ToLocalChecked();
  wrapper_boilerplates_.Set(type, instance_template);
  return instance_template->Clone();
}

v8::Local<v8::Function> V8PerContextData::ConstructorForTypeSlowCase(
    const WrapperTypeInfo* type) {
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope scope(context);

  v8::Local<v8::Function> parent_interface_object;
  if (type->parent_class) {
    parent_interface_object = ConstructorForType(type->parent_class);
  }

  const DOMWrapperWorld& world = DOMWrapperWorld::World(context);
  v8::Local<v8::Function> interface_object =
      V8ObjectConstructor::CreateInterfaceObject(
          type, context, world, isolate_, parent_interface_object,
          V8ObjectConstructor::CreationMode::kInstallConditionalFeatures);

  constructor_map_.Set(type, interface_object);
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
  *interface_object = constructor_map_.Get(type);
  if (interface_object->IsEmpty()) {
    *prototype_object = v8::Local<v8::Object>();
    return false;
  }
  *prototype_object = PrototypeForType(type);
  DCHECK(!prototype_object->IsEmpty());
  return true;
}

void V8PerContextData::AddCustomElementBinding(
    std::unique_ptr<V0CustomElementBinding> binding) {
  custom_element_bindings_.push_back(std::move(binding));
}

void V8PerContextData::AddData(const char* key, Data* data) {
  data_map_->Set(key, data);
}

void V8PerContextData::ClearData(const char* key) {
  data_map_->erase(key);
}

V8PerContextData::Data* V8PerContextData::GetData(const char* key) {
  return data_map_->at(key);
}

}  // namespace blink
