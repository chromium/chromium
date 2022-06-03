// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class DOMArrayBufferTest : public testing::Test {
 public:
  void SetUp() override {
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                   gin::ArrayBufferAllocator::SharedInstance());
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        base::ThreadTaskRunnerHandle::Get(),
        gin::IsolateHolder::IsolateType::kBlinkWorkerThread);
  }

  void TearDown() override { isolate_holder_.reset(); }

  v8::Isolate* isolate() const { return isolate_holder_->isolate(); }

 private:
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
};

TEST_F(DOMArrayBufferTest, TransferredArrayBufferIsDetached) {
  ArrayBufferContents src(10, 4, ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kZeroInitialize);
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst(nullptr, 0, nullptr);
  buffer->Transfer(isolate(), dst);
  ASSERT_EQ(true, buffer->IsDetached());
}

TEST_F(DOMArrayBufferTest, TransferredEmptyArrayBufferIsDetached) {
  ArrayBufferContents src(nullptr, 0, nullptr);
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst(nullptr, 0, nullptr);
  buffer->Transfer(isolate(), dst);
  ASSERT_EQ(true, buffer->IsDetached());
}

}  // namespace blink
