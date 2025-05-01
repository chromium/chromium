// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef REMOTING_BASE_TYPED_BUFFER_H_
#define REMOTING_BASE_TYPED_BUFFER_H_

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <type_traits>

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
  constexpr explicit TypedBuffer(uint32_t length) : length_(length) {
    if (length_ > 0) {
      buffer_ = std::make_unique<uint8_t[]>(length);
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
  // operator* and operator-> will assert() if there is no current buffer.
  T& operator*() {
    assert(buffer_);
    return *(reinterpret_cast<T*>(buffer_.get()));
  }
  T* operator->() {
    assert(buffer_);
    return reinterpret_cast<T*>(buffer_.get());
  }
  T* get() { return buffer_ ? reinterpret_cast<T*>(&buffer_[0]) : nullptr; }

  // `const` variants of the above.
  const T& operator*() const {
    assert(buffer_);
    return *(reinterpret_cast<const T*>(buffer_.get()));
  }
  const T* operator->() const {
    assert(buffer_);
    return reinterpret_cast<const T*>(buffer_.get());
  }
  const T* get() const {
    return buffer_ ? reinterpret_cast<const T*>(&buffer_[0]) : nullptr;
  }

  uint32_t length() const { return length_; }

  explicit operator bool() const { return buffer_.operator bool(); }

  // Swap two buffers.
  void Swap(TypedBuffer& other) {
    std::swap(buffer_, other.buffer_);
    std::swap(length_, other.length_);
  }

 private:
  // Points to the owned buffer.
  std::unique_ptr<uint8_t[]> buffer_;

  // Length of the owned buffer in bytes.
  uint32_t length_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_TYPED_BUFFER_H_
