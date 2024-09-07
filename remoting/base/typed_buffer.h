// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef REMOTING_BASE_TYPED_BUFFER_H_
#define REMOTING_BASE_TYPED_BUFFER_H_

#include <assert.h>
#include <stdint.h>

#include <algorithm>

#include "base/memory/raw_ptr.h"

namespace remoting {

// A scoper for a variable-length structure such as SID, SECURITY_DESCRIPTOR and
// similar. These structures typically consist of a header followed by variable-
// length data, so the size may not match sizeof(T). The class supports
// move-only semantics and typed buffer getters.
template <typename T>
class TypedBuffer {
 public:
  TypedBuffer() = default;

  // Creates an instance of the object allocating a buffer of the given size.
  explicit TypedBuffer(uint32_t length) : length_(length) {
    if (length_ > 0) {
      buffer_ = reinterpret_cast<T*>(new uint8_t[length_]);
    }
  }

  TypedBuffer(TypedBuffer&& rvalue) : TypedBuffer() { Swap(rvalue); }

  TypedBuffer(const TypedBuffer&) = delete;
  TypedBuffer& operator=(const TypedBuffer&) = delete;

  ~TypedBuffer() {
    if (buffer_) {
      delete[] reinterpret_cast<uint8_t*>(buffer_.ExtractAsDangling().get());
    }
  }

  TypedBuffer& operator=(TypedBuffer&& rvalue) {
    Swap(rvalue);
    return *this;
  }

  // Accessors to get the owned buffer.
  // operator* and operator-> will assert() if there is no current buffer.
  T& operator*() const {
    assert(buffer_);
    return *buffer_;
  }
  T* operator->() const {
    assert(buffer_);
    return buffer_;
  }
  T* get() const { return buffer_; }

  uint32_t length() const { return length_; }

  // Helper returning a pointer to the structure starting at a specified byte
  // offset.
  T* GetAtOffset(uint32_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(buffer_.get()) +
                                offset);
  }

  // Allow TypedBuffer<T> to be used in boolean expressions.
  explicit operator bool() const { return buffer_ != nullptr; }

  // Swap two buffers.
  void Swap(TypedBuffer& other) {
    std::swap(buffer_, other.buffer_);
    std::swap(length_, other.length_);
  }

 private:
  // Points to the owned buffer.
  raw_ptr<T> buffer_ = nullptr;

  // Length of the owned buffer in bytes.
  uint32_t length_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_TYPED_BUFFER_H_
