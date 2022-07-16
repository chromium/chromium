// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_ARRAY_BUFFER_H_
#define GIN_ARRAY_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "gin/converter.h"
#include "gin/gin_export.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-forward.h"

namespace gin {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override;
  void* AllocateUninitialized(size_t length) override;
  void Free(void* data, size_t length) override;

  GIN_EXPORT static ArrayBufferAllocator* SharedInstance();
};

class GIN_EXPORT ArrayBuffer {
 public:
  ArrayBuffer();
  ArrayBuffer(v8::Isolate* isolate, v8::Local<v8::ArrayBuffer> buffer);
  ArrayBuffer(const ArrayBuffer&) = delete;
  ~ArrayBuffer();
  ArrayBuffer& operator=(const ArrayBuffer& other);

  void* bytes() const {
    return backing_store_ ? backing_store_->Data() : nullptr;
  }
  size_t num_bytes() const {
    return backing_store_ ? backing_store_->ByteLength() : 0;
  }

 private:
  std::shared_ptr<v8::BackingStore> backing_store_;
};

template<>
struct GIN_EXPORT Converter<ArrayBuffer> {
  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     ArrayBuffer* out);
};

class GIN_EXPORT ArrayBufferView {
 public:
  ArrayBufferView();
  ArrayBufferView(v8::Isolate* isolate, v8::Local<v8::ArrayBufferView> view);
  ArrayBufferView(const ArrayBufferView&) = delete;
  ~ArrayBufferView();
  ArrayBufferView& operator=(const ArrayBufferView& other);

  void* bytes() const {
    return static_cast<uint8_t*>(array_buffer_.bytes()) + offset_;
  }
  size_t num_bytes() const { return num_bytes_; }

 private:
  ArrayBuffer array_buffer_;
  size_t offset_;
  size_t num_bytes_;
};

template<>
struct GIN_EXPORT Converter<ArrayBufferView> {
  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     ArrayBufferView* out);
};

}  // namespace gin

#endif  // GIN_ARRAY_BUFFER_H_
