// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "gin/public/context_holder.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"

namespace gin {

namespace {
// The context object allocates internal fields for each embedder type. This is
// the index into the context object's internal field array for Gin.
constexpr int kGinPerContextDataIndex =
    int{kPerContextDataStartIndex} + kEmbedderNativeGin;
}  // namespace

PerContextData::PerContextData(ContextHolder* context_holder,
                               v8::Local<v8::Context> context)
    : context_holder_(context_holder) {
  context->SetAlignedPointerInEmbedderData(
      kGinPerContextDataIndex, this,
      static_cast<v8::CppHeapPointerTag>(gin::kGinPerContextData));
}

PerContextData::~PerContextData() = default;

void PerContextData::Detach() {
  ClearAllUserData();
  CHECK(context_holder_ != nullptr);
  context_holder_->context()->SetAlignedPointerInEmbedderData(
      kGinPerContextDataIndex, static_cast<PerContextData*>(nullptr),
      static_cast<v8::CppHeapPointerTag>(gin::kGinPerContextData));
  context_holder_ = nullptr;
}

void PerContextData::Trace(cppgc::Visitor* visitor) const {}

// static
PerContextData* PerContextData::From(v8::Local<v8::Context> context) {
  if (context->GetNumberOfEmbedderDataFields() <= kGinPerContextDataIndex) {
    return nullptr;
  }
  return context->GetAlignedPointerFromEmbedderData<PerContextData>(
      v8::Isolate::GetCurrent(), kGinPerContextDataIndex,
      static_cast<v8::CppHeapPointerTag>(gin::kGinPerContextData));
}

void PerContextData::SetObjectTemplate(
    const WrapperInfo* info,
    v8::Local<v8::ObjectTemplate> object_template) {
  object_templates_[info].Reset(context_holder_->isolate(), object_template);
}

v8::Local<v8::ObjectTemplate> PerContextData::GetObjectTemplate(
    const WrapperInfo* info) {
  auto iter = object_templates_.find(info);
  if (iter == object_templates_.end()) {
    return v8::Local<v8::ObjectTemplate>();
  }
  return v8::Local<v8::ObjectTemplate>::New(context_holder_->isolate(),
                                            iter->second);
}

}  // namespace gin
