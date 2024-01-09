// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include "base/task/single_thread_task_runner.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-template.h"

namespace gin {

// Verifies IsolateHolder can be constructed and destructed in kUseLocker access
// mode. These tests doesn't specifically/deliberately exercise anything
// multi-threaded.
class UseLockerIsolateHolderTest : public V8Test {
  gin::IsolateHolder::AccessMode AccessMode() const override {
    return gin::IsolateHolder::AccessMode::kUseLocker;
  }
};

// This test exercises teardown of function templates with isolate holders.
TEST_F(UseLockerIsolateHolderTest, FunctionTemplateLifetime) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);
  object_template->Set(
      isolate, "someFunction",
      gin::CreateFunctionTemplate(isolate, base::BindRepeating([]() {})));
  context->Global()
      ->Set(context, StringToV8(isolate, "someObjectTemplate"),
            object_template->NewInstance(context).ToLocalChecked())
      .Check();
  // There's nothing specific to assert - we just want to make sure that it
  // doesn't crash on teardown.
}

}  // namespace gin
