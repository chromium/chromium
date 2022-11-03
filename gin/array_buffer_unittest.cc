// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/array_buffer.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"

namespace gin {

using ArrayBufferTest = V8Test;

namespace {
const size_t kBufferLength = 65536;
}

// Make sure we can allocate, access and free memory.
TEST_F(ArrayBufferTest, AllocateAndFreeBuffer) {
  v8::Isolate* const isolate = instance_->isolate();
  v8::ArrayBuffer::Allocator* const allocator =
      PerIsolateData::From(isolate)->allocator();

  void* buffer = allocator->Allocate(kBufferLength);
  char* buffer0 = reinterpret_cast<char*>(buffer);
  *buffer0 = '0';  // ASCII zero
  CHECK_EQ('0', *buffer0);
  allocator->Free(buffer, kBufferLength);
}

}  // namespace gin
