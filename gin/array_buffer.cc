// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/array_buffer.h"

#include <stddef.h>
#include <stdlib.h>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/check_op.h"
#include "build/build_config.h"
#include "gin/per_isolate_data.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif  // BUILDFLAG(IS_POSIX)

namespace gin {

static_assert(V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT == 2,
              "array buffers must have two internal fields");

// ArrayBufferAllocator -------------------------------------------------------

void* ArrayBufferAllocator::Allocate(size_t length) {
  // TODO(bbudge) Use partition allocator for malloc/calloc allocations.
  return calloc(1, length);
}

void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return malloc(length);
}

void ArrayBufferAllocator::Free(void* data, size_t length) {
  free(data);
}

ArrayBufferAllocator* ArrayBufferAllocator::SharedInstance() {
  static ArrayBufferAllocator* instance = new ArrayBufferAllocator();
  return instance;
}

// ArrayBuffer ----------------------------------------------------------------
ArrayBuffer::ArrayBuffer() = default;

ArrayBuffer::ArrayBuffer(v8::Isolate* isolate, v8::Local<v8::ArrayBuffer> array)
    : backing_store_(array->GetBackingStore()) {}

ArrayBuffer::~ArrayBuffer() = default;

ArrayBuffer& ArrayBuffer::operator=(const ArrayBuffer& other) = default;

// Converter<ArrayBuffer> -----------------------------------------------------

bool Converter<ArrayBuffer>::FromV8(v8::Isolate* isolate,
                                    v8::Local<v8::Value> val,
                                    ArrayBuffer* out) {
  if (!val->IsArrayBuffer())
    return false;
  *out = ArrayBuffer(isolate, v8::Local<v8::ArrayBuffer>::Cast(val));
  return true;
}

// ArrayBufferView ------------------------------------------------------------

ArrayBufferView::ArrayBufferView()
    : offset_(0),
      num_bytes_(0) {
}

ArrayBufferView::ArrayBufferView(v8::Isolate* isolate,
                                 v8::Local<v8::ArrayBufferView> view)
    : array_buffer_(isolate, view->Buffer()),
      offset_(view->ByteOffset()),
      num_bytes_(view->ByteLength()) {
}

ArrayBufferView::~ArrayBufferView() = default;

ArrayBufferView& ArrayBufferView::operator=(const ArrayBufferView& other) =
    default;

// Converter<ArrayBufferView> -------------------------------------------------

bool Converter<ArrayBufferView>::FromV8(v8::Isolate* isolate,
                                        v8::Local<v8::Value> val,
                                        ArrayBufferView* out) {
  if (!val->IsArrayBufferView())
    return false;
  *out = ArrayBufferView(isolate, v8::Local<v8::ArrayBufferView>::Cast(val));
  return true;
}

}  // namespace gin
