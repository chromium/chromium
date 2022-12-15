// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include "base/task/single_thread_task_runner.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "v8/include/v8.h"

namespace blink {

class DOMArrayBufferTest : public testing::Test {
 public:
  void SetUp() override {
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                   gin::ArrayBufferAllocator::SharedInstance());
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        gin::IsolateHolder::IsolateType::kBlinkWorkerThread);
  }

  void TearDown() override { isolate_holder_.reset(); }

  v8::Isolate* isolate() const { return isolate_holder_->isolate(); }

 private:
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
};

TEST_F(DOMArrayBufferTest, TransferredArrayBufferIsDetached) {
  V8TestingScope v8_scope;
  ArrayBufferContents src(10, 4, ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kZeroInitialize);
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(isolate(), dst, v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
}

TEST_F(DOMArrayBufferTest, TransferredEmptyArrayBufferIsDetached) {
  V8TestingScope v8_scope;
  ArrayBufferContents src;
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(isolate(), dst, v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
}

}  // namespace blink
