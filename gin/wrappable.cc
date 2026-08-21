// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/check_op.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"
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

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return {};
  }

  const WrapperInfo* info = wrapper_info();

  PerContextData* data = PerContextData::From(context);
  v8::Local<v8::ObjectTemplate> templ;
  if (data) {
    templ = data->GetObjectTemplate(info);
  }
  if (templ.IsEmpty()) {
    templ = GetObjectTemplateBuilder(isolate).Build();
    CHECK(!templ.IsEmpty());
    if (data) {
      data->SetObjectTemplate(info, templ);
    }
  }
  v8::Local<v8::Object> wrapper;
  // |wrapper| may be empty in some extreme cases, e.g., when
  // Object.prototype.constructor is overwritten.
  if (!templ->NewInstance(context).ToLocal(&wrapper)) {
    return {};
  }

  AssociateWithWrapper(isolate, wrapper);
  return wrapper;
}

void WrappableBase::SetWrapper(v8::Isolate* isolate,
                               v8::Local<v8::Object> wrapper) {
  CHECK(wrapper_.IsEmpty());
  AssociateWithWrapper(isolate, wrapper);
}

}  // namespace gin
