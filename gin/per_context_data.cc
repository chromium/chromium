// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "gin/public/context_holder.h"
#include "gin/public/wrapper_info.h"

namespace gin {

PerContextData::PerContextData(ContextHolder* context_holder,
                               v8::Local<v8::Context> context)
    : context_holder_(context_holder), runner_(nullptr) {
  context->SetAlignedPointerInEmbedderData(
      int{kPerContextDataStartIndex} + kEmbedderNativeGin, this);
}

PerContextData::~PerContextData() {
  v8::HandleScope handle_scope(context_holder_->isolate());
  context_holder_->context()->SetAlignedPointerInEmbedderData(
      int{kPerContextDataStartIndex} + kEmbedderNativeGin, NULL);
}

// static
PerContextData* PerContextData::From(v8::Local<v8::Context> context) {
  return static_cast<PerContextData*>(
      context->GetAlignedPointerFromEmbedderData(kEncodedValueIndex));
}

}  // namespace gin
