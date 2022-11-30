// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/context_holder.h"

#include <memory>

#include "base/check.h"
#include "gin/per_context_data.h"

namespace gin {

ContextHolder::ContextHolder(v8::Isolate* isolate)
    : isolate_(isolate) {
}

ContextHolder::~ContextHolder() {
  // PerContextData needs to be destroyed before the context.
  data_.reset();
}

void ContextHolder::SetContext(v8::Local<v8::Context> context) {
  DCHECK(context_.IsEmpty());
  context_.Reset(isolate_, context);
  context_.AnnotateStrongRetainer("gin::ContextHolder::context_");
  data_ = std::make_unique<PerContextData>(this, context);
}

}  // namespace gin
