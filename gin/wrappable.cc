// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/check_op.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"

namespace gin {

WrappableBase::WrappableBase() = default;

WrappableBase::~WrappableBase() {
  wrapper_.Reset();
}

ObjectTemplateBuilder WrappableBase::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return ObjectTemplateBuilder(isolate, GetTypeName());
}

const char* WrappableBase::GetTypeName() {
  return nullptr;
}

void WrappableBase::FirstWeakCallback(
    const v8::WeakCallbackInfo<WrappableBase>& data) {
  WrappableBase* wrappable = data.GetParameter();
  wrappable->dead_ = true;
  wrappable->wrapper_.Reset();
  data.SetSecondPassCallback(SecondWeakCallback);
}

void WrappableBase::SecondWeakCallback(
    const v8::WeakCallbackInfo<WrappableBase>& data) {
  WrappableBase* wrappable = data.GetParameter();
  delete wrappable;
}

v8::MaybeLocal<v8::Object> WrappableBase::GetWrapperImpl(v8::Isolate* isolate,
                                                         WrapperInfo* info) {
  if (!wrapper_.IsEmpty()) {
    return v8::MaybeLocal<v8::Object>(
        v8::Local<v8::Object>::New(isolate, wrapper_));
  }

  if (dead_)
    return v8::MaybeLocal<v8::Object>();

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
    // The current wrappable object will be no longer managed by V8. Delete this
    // now.
    delete this;
    return v8::MaybeLocal<v8::Object>(wrapper);
  }

  int indices[] = {kWrapperInfoIndex, kEncodedValueIndex};
  void* values[] = {info, this};
  wrapper->SetAlignedPointerInInternalFields(2, indices, values);
  wrapper_.Reset(isolate, wrapper);
  wrapper_.SetWeak(this, FirstWeakCallback, v8::WeakCallbackType::kParameter);
  return v8::MaybeLocal<v8::Object>(wrapper);
}

namespace internal {

void* FromV8Impl(v8::Isolate* isolate, v8::Local<v8::Value> val,
                 WrapperInfo* wrapper_info) {
  if (!val->IsObject())
    return NULL;
  v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(val);
  WrapperInfo* info = WrapperInfo::From(obj);

  // If this fails, the object is not managed by Gin. It is either a normal JS
  // object that's not wrapping any external C++ object, or it is wrapping some
  // C++ object, but that object isn't managed by Gin (maybe Blink).
  if (!info)
    return NULL;

  // If this fails, the object is managed by Gin, but it's not wrapping an
  // instance of the C++ class associated with wrapper_info.
  if (info != wrapper_info)
    return NULL;

  return obj->GetAlignedPointerFromInternalField(kEncodedValueIndex);
}

}  // namespace internal

}  // namespace gin
