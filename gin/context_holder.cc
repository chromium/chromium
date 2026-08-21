// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/context_holder.h"

#include "base/check.h"
#include "gin/per_context_data.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-cppgc.h"

namespace gin {

ContextHolder::ContextHolder(v8::Isolate* isolate) : isolate_(isolate) {}

ContextHolder::~ContextHolder() {
  if (data_) {
    data_->Detach();
    data_ = nullptr;
  }
}

void ContextHolder::SetContext(v8::Local<v8::Context> context) {
  DCHECK(context_.IsEmpty());
  context_.Reset(isolate_, context);
  context_.AnnotateStrongRetainer("gin::ContextHolder::context_");
  data_ = cppgc::MakeGarbageCollected<PerContextData>(
      isolate_->GetCppHeap()->GetAllocationHandle(), this, context);
}

}  // namespace gin
