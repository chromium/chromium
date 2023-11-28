// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include "gin/array_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "v8/include/v8.h"

namespace blink {

TEST(DOMArrayBufferTest, TransferredArrayBufferIsDetached) {
  V8TestingScope v8_scope;
  ArrayBufferContents src(10, 4, ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kZeroInitialize);
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
}

TEST(DOMArrayBufferTest, TransferredEmptyArrayBufferIsDetached) {
  V8TestingScope v8_scope;
  ArrayBufferContents src;
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
}

}  // namespace blink
