// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "gin/public/context_holder.h"
#include "gin/public/wrapper_info.h"

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
  context->SetAlignedPointerInEmbedderData(kGinPerContextDataIndex, this,
                                           kGinPerContextDataIndex);
}

PerContextData::~PerContextData() {
  context_holder_->context()->SetAlignedPointerInEmbedderData(
      kGinPerContextDataIndex, nullptr, kGinPerContextDataIndex);
}

// static
PerContextData* PerContextData::From(v8::Local<v8::Context> context) {
  return static_cast<PerContextData*>(
      context->GetAlignedPointerFromEmbedderData(kGinPerContextDataIndex,
                                                 kGinPerContextDataIndex));
}

}  // namespace gin
