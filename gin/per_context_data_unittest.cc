// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_context_data.h"

#include "base/test/gtest_util.h"
#include "gin/public/context_holder.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-template.h"

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

TEST_F(PerContextDataTest, ContextMismatchCrashes) {
  v8::Isolate::Scope isolate_scope(instance_->isolate());
  v8::HandleScope handle_scope(instance_->isolate());
  v8::Local<v8::Context> context1 = v8::Context::New(instance_->isolate());
  v8::Local<v8::Context> context2 = v8::Context::New(instance_->isolate());

  ContextHolder context_holder1(instance_->isolate());
  context_holder1.SetContext(context1);
  ContextHolder context_holder2(instance_->isolate());
  context_holder2.SetContext(context2);

  PerContextData* data1 = PerContextData::From(context1);
  PerContextData* data2 = PerContextData::From(context2);
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data2, nullptr);
  EXPECT_EQ(&context_holder1, data1->context_holder());
  EXPECT_EQ(&context_holder2, data2->context_holder());

  constexpr int kGinPerContextDataIndex =
      int{kPerContextDataStartIndex} + kEmbedderNativeGin;
  context1->SetAlignedPointerInEmbedderDataV2(
      kGinPerContextDataIndex, data2,
      static_cast<v8::CppHeapPointerTag>(gin::kGinPerContextData));

  EXPECT_CHECK_DEATH(PerContextData::From(context1));

  // Restore context1's PerContextData so context_holder1 detaches cleanly.
  context1->SetAlignedPointerInEmbedderDataV2(
      kGinPerContextDataIndex, data1,
      static_cast<v8::CppHeapPointerTag>(gin::kGinPerContextData));
}

}  // namespace gin
