// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef REMOTING_BASE_TYPED_BUFFER_H_
#define REMOTING_BASE_TYPED_BUFFER_H_

#include <stdint.h>

#include <algorithm>
#include <type_traits>

#include "base/check.h"
#include "base/containers/heap_array.h"

namespace remoting {

// A scoper for a variable-length structure such as SID, SECURITY_DESCRIPTOR and
// similar. These structures typically consist of a header followed by variable-
// length data, so the size may not match sizeof(T). The class supports
// move-only semantics and typed buffer getters.
template <typename T>
  requires std::is_trivially_destructible_v<T>
class TypedBuffer {
 public:
  TypedBuffer() = default;

  // Creates an instance of the object allocating a buffer of the given size.
  constexpr explicit TypedBuffer(uint32_t length) {
    if (length > 0) {
      buffer_ = base::HeapArray<uint8_t>::Uninit(length);
    }
  }

  TypedBuffer(TypedBuffer&& rvalue) : TypedBuffer() { Swap(rvalue); }

  TypedBuffer(const TypedBuffer&) = delete;
  TypedBuffer& operator=(const TypedBuffer&) = delete;

  ~TypedBuffer() = default;

  TypedBuffer& operator=(TypedBuffer&& rvalue) {
    Swap(rvalue);
    return *this;
  }

  // Accessors to get the owned buffer.
  // operator* and operator-> are fatal if there is no current buffer.
  T& operator*() {
    CHECK(!buffer_.empty());
    return *(reinterpret_cast<T*>(buffer_.data()));
  }
  T* operator->() {
    CHECK(!buffer_.empty());
    return reinterpret_cast<T*>(buffer_.data());
  }
  T* get() {
    return buffer_.empty() ? nullptr : reinterpret_cast<T*>(buffer_.data());
  }

  // `const` variants of the above.
  const T& operator*() const {
    CHECK(!buffer_.empty());
    return *(reinterpret_cast<const T*>(buffer_.data()));
  }
  const T* operator->() const {
    CHECK(!buffer_.empty());
    return reinterpret_cast<const T*>(buffer_.data());
  }
  const T* get() const {
    return buffer_.empty() ? nullptr
                           : reinterpret_cast<const T*>(buffer_.data());
  }

  size_t length() const { return buffer_.size(); }

  explicit operator bool() const { return !buffer_.empty(); }

  // Swap two buffers.
  void Swap(TypedBuffer& other) {
    std::swap(buffer_, other.buffer_);
  }

 private:
  base::HeapArray<uint8_t> buffer_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_TYPED_BUFFER_H_
