/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VECTOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VECTOR_H_

#include "base/check_op.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_common.h"

#include <vector>

namespace blink {

// A simple vector class that wraps a std::vector.
//
// Sample usage:
//
//   void Foo(WebVector<int>& result) {
//     WebVector<int> data(10);
//     for (size_t i = 0; i < data.size(); ++i)
//         data[i] = ...
//     result.Swap(data);
//   }
//
// In-place element construction:
//
//   WebVector<WebString> Foo() {
//     WebVector<WebString> data;
//     data.reserve(10);
//     WebUChar* buffer = ....;
//     data.emplace_back(buffer, buffer_size);
//     return data;
//   }
//
// It is also possible to assign from any container that implements begin()
// and end().
//
//   void Foo(const std::vector<WTF::String>& input) {
//     WebVector<WebString> strings = input;
//     ...
//   }
template <typename T>
class WebVector {
 public:
  using value_type = typename std::vector<T>::value_type;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  ~WebVector() = default;

  // Create an empty vector.
  //
  // The vector can be populated using reserve() and emplace_back().
  WebVector() = default;

#if defined(ARCH_CPU_64_BITS)
  // Create a vector with |size| default-constructed elements. We define
  // a constructor with size_t otherwise we'd have a duplicate define.
  explicit WebVector(size_t size) : data_(size) {}
#endif

  // Create a vector with |size| default-constructed elements.
  explicit WebVector(uint32_t size) : data_(size) {}

  template <typename U>
  WebVector(const U* values, size_t size) : data_(values, values + size) {}

  WebVector(const WebVector<T>& other) : data_(other.data_) {}

  template <typename C,
            typename = decltype(std::declval<C>().begin()),
            typename = decltype(std::declval<C>().end())>
  WebVector(const C& other) : data_(other.begin(), other.end()) {}

  WebVector(WebVector<T>&& other) noexcept { swap(other); }

  WebVector(std::vector<T>&& other) noexcept : data_(std::move(other)) {}

  std::vector<T> ReleaseVector() noexcept { return std::move(data_); }

  WebVector& operator=(const WebVector& other) {
    if (this != &other)
      Assign(other);
    return *this;
  }

  WebVector& operator=(WebVector&& other) noexcept {
    if (this != &other)
      swap(other);
    return *this;
  }

  template <typename C>
  WebVector<T>& operator=(const C& other) {
    if (this != reinterpret_cast<const WebVector<T>*>(&other))
      Assign(other);
    return *this;
  }

  WebVector<T>& operator=(std::vector<T>&& other) noexcept {
    data_ = std::move(other);
    return *this;
  }

  template <typename C>
  void Assign(const C& other) {
    data_.assign(other.begin(), other.end());
  }

  size_t size() const { return data_.size(); }
  void resize(size_t new_size) {
    DCHECK_LE(new_size, data_.capacity());
    data_.resize(new_size);
  }
  bool empty() const { return data_.empty(); }

  size_t capacity() const { return data_.capacity(); }
  void reserve(size_t new_capacity) { data_.reserve(new_capacity); }

  T& operator[](size_t i) {
    DCHECK_LT(i, data_.size());
    return data_[i];
  }

  const T& operator[](size_t i) const {
    DCHECK_LT(i, data_.size());
    return data_[i];
  }

  T* data() { return data_.data(); }
  const T* data() const { return data_.data(); }

  iterator begin() { return data_.begin(); }
  iterator end() { return data_.end(); }
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    data_.emplace_back(std::forward<Args>(args)...);
  }

  void push_back(const T& value) { data_.push_back(value); }
  void push_back(T&& value) { data_.push_back(std::move(value)); }

  void swap(WebVector<T>& other) { data_.swap(other.data_); }
  void clear() { data_.clear(); }

  T& front() { return data_.front(); }
  const T& front() const { return data_.front(); }
  T& back() { return data_.back(); }
  const T& back() const { return data_.back(); }

  void EraseAt(size_t index) { data_.erase(begin() + index); }
  void Insert(size_t index, const T& value) {
    data_.insert(begin() + index, value);
  }

  bool Equals(const WebVector<T>& other) const { return data_ == other.data_; }

 private:
  std::vector<T> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_VECTOR_H_
