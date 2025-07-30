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
  CHECK_EQ(0, templ->InternalFieldCount());
  v8::Local<v8::Object> wrapper;
  // |wrapper| may be empty in some extreme cases, e.g., when
  // Object.prototype.constructor is overwritten.
  if (!templ->NewInstance(isolate->GetCurrentContext()).ToLocal(&wrapper)) {
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
