// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"

namespace gin {

typedef V8Test PerContextDataTest;

// Verifies PerContextData can be looked up by context and that it is not
// available once ContextHolder is destroyed.
TEST_F(PerContextDataTest, LookupAndDestruction) {
  v8::Isolate::Scope isolate_scope(instance_->isolate());
  v8::HandleScope handle_scope(instance_->isolate());
  v8::Local<v8::Context> context = v8::Context::New(
      instance_->isolate(), NULL, v8::Local<v8::ObjectTemplate>());
  {
    ContextHolder context_holder(instance_->isolate());
    context_holder.SetContext(context);
    PerContextData* per_context_data = PerContextData::From(context);
    EXPECT_TRUE(per_context_data != NULL);
    EXPECT_EQ(&context_holder, per_context_data->context_holder());
  }
  PerContextData* per_context_data = PerContextData::From(context);
  EXPECT_TRUE(per_context_data == NULL);
}

}  // namespace gin
