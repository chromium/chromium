// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/check_op.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "v8/include/cppgc/visitor.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-sandbox.h"

namespace gin {

ObjectTemplateBuilder WrappableBase::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return ObjectTemplateBuilder(isolate, GetHumanReadableName());
}

void WrappableBase::AssociateWithWrapper(v8::Isolate* isolate,
                                         v8::Local<v8::Object> wrapper) {
  const WrapperInfo* info = wrapper_info();
  v8::Object::Wrap(isolate, wrapper, this,
                   static_cast<v8::CppHeapPointerTag>(info->pointer_tag));
  wrapper_.Reset(isolate, wrapper);
}

NamedPropertyInterceptor* WrappableBase::GetNamedPropertyInterceptor() {
  return nullptr;
}

void WrappableBase::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(wrapper_);
}

const v8::Object::WrapperTypeInfo* WrappableBase::GetWrapperTypeInfo() const {
  return wrapper_info();
}

v8::MaybeLocal<v8::Object> WrappableBase::GetWrapper(v8::Isolate* isolate) {
  if (!wrapper_.IsEmpty()) {
    return wrapper_.Get(isolate);
  }

  const WrapperInfo* info = wrapper_info();

  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(info);
  if (templ.IsEmpty()) {
    templ = GetObjectTemplateBuilder(isolate).Build();
    CHECK(!templ.IsEmpty());
    data->SetObjectTemplate(info, templ);
  }
  CHECK_EQ(kNumberOfInternalFields, templ->InternalFieldCount());
  v8::Local<v8::Object> wrapper;
  // |wrapper| may be empty in some extreme cases, e.g., when
  // Object.prototype.constructor is overwritten.
  if (!templ->NewInstance(isolate->GetCurrentContext()).ToLocal(&wrapper)) {
    return {};
  }

  // TODO(345640553): Delete the internal fields once DeprecatedWrappable does
  // not exist anymore.
  wrapper->SetAlignedPointerInInternalField(kWrapperInfoIndex, nullptr,
                                            kDeprecatedData);
  wrapper->SetAlignedPointerInInternalField(kEncodedValueIndex, nullptr,
                                            kDeprecatedData);

  AssociateWithWrapper(isolate, wrapper);
  return wrapper;
}

void WrappableBase::SetWrapper(v8::Isolate* isolate,
                               v8::Local<v8::Object> wrapper) {
  CHECK(wrapper_.IsEmpty());
  AssociateWithWrapper(isolate, wrapper);
}

DeprecatedWrappableBase::DeprecatedWrappableBase() = default;

DeprecatedWrappableBase::~DeprecatedWrappableBase() {
  wrapper_.Reset();
}

ObjectTemplateBuilder DeprecatedWrappableBase::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return ObjectTemplateBuilder(isolate, GetTypeName());
}

const char* DeprecatedWrappableBase::GetTypeName() {
  return nullptr;
}

void DeprecatedWrappableBase::FirstWeakCallback(
    const v8::WeakCallbackInfo<DeprecatedWrappableBase>& data) {
  DeprecatedWrappableBase* wrappable = data.GetParameter();
  wrappable->dead_ = true;
  wrappable->wrapper_.Reset();
  data.SetSecondPassCallback(SecondWeakCallback);
}

void DeprecatedWrappableBase::SecondWeakCallback(
    const v8::WeakCallbackInfo<DeprecatedWrappableBase>& data) {
  DeprecatedWrappableBase* wrappable = data.GetParameter();
  delete wrappable;
}

v8::MaybeLocal<v8::Object> DeprecatedWrappableBase::GetWrapperImpl(
    v8::Isolate* isolate,
    DeprecatedWrapperInfo* info) {
  if (!wrapper_.IsEmpty()) {
    return v8::MaybeLocal<v8::Object>(
        v8::Local<v8::Object>::New(isolate, wrapper_));
  }

  if (dead_) {
    return v8::MaybeLocal<v8::Object>();
  }

  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->DeprecatedGetObjectTemplate(info);
  if (templ.IsEmpty()) {
    templ = GetObjectTemplateBuilder(isolate).Build();
    CHECK(!templ.IsEmpty());
    data->DeprecatedSetObjectTemplate(info, templ);
  }
  CHECK_EQ(kNumberOfInternalFields, templ->InternalFieldCount());
  v8::Local<v8::Object> wrapper;
  // |wrapper| may be empty in some extreme cases, e.g., when
  // Object.prototype.constructor is overwritten.
  if (!templ->NewInstance(isolate->GetCurrentContext()).ToLocal(&wrapper)) {
    // The current wrappable object will be no longer managed by V8. Delete this
    // now.
    delete this;
    return v8::MaybeLocal<v8::Object>(wrapper);
  }

  wrapper->SetAlignedPointerInInternalField(kWrapperInfoIndex, info,
                                            kDeprecatedData);
  wrapper->SetAlignedPointerInInternalField(kEncodedValueIndex, this,
                                            kDeprecatedData);
  wrapper_.Reset(isolate, wrapper);
  wrapper_.SetWeak(this, FirstWeakCallback, v8::WeakCallbackType::kParameter);
  return v8::MaybeLocal<v8::Object>(wrapper);
}

namespace internal {

void* FromV8Impl(v8::Isolate* isolate,
                 v8::Local<v8::Value> val,
                 DeprecatedWrapperInfo* wrapper_info) {
  if (!val->IsObject()) {
    return nullptr;
  }
  v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(val);
  DeprecatedWrapperInfo* info = DeprecatedWrapperInfo::From(obj);

  // If this fails, the object is not managed by Gin. It is either a normal JS
  // object that's not wrapping any external C++ object, or it is wrapping some
  // C++ object, but that object isn't managed by Gin (maybe Blink).
  if (!info) {
    return nullptr;
  }

  // If this fails, the object is managed by Gin, but it's not wrapping an
  // instance of the C++ class associated with wrapper_info.
  if (info != wrapper_info) {
    return nullptr;
  }

  return obj->GetAlignedPointerFromInternalField(kEncodedValueIndex,
                                                 kDeprecatedData);
}

}  // namespace internal

}  // namespace gin
