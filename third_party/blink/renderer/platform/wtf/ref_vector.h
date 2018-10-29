// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_REF_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_REF_VECTOR_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename T>
class RefVector : public RefCounted<RefVector<T>> {
 public:
  static scoped_refptr<RefVector> Create() {
    return base::AdoptRef(new RefVector<T>);
  }
  static scoped_refptr<RefVector> Create(const Vector<T>& vector) {
    return base::AdoptRef(new RefVector<T>(vector));
  }
  static scoped_refptr<RefVector> Create(Vector<T>&& vector) {
    return base::AdoptRef(new RefVector<T>(std::move(vector)));
  }
  scoped_refptr<RefVector> Copy() { return Create(GetVector()); }

  const T& operator[](wtf_size_t i) const { return vector_[i]; }
  T& operator[](wtf_size_t i) { return vector_[i]; }
  const T& at(wtf_size_t i) const { return vector_.at(i); }
  T& at(wtf_size_t i) { return vector_.at(i); }

  T* begin() { return vector_.begin(); }
  T* end() { return vector_.end(); }
  const T* begin() const { return vector_.begin(); }
  const T* end() const { return vector_.end(); }

  bool operator==(const RefVector& o) const { return vector_ == o.vector_; }
  bool operator!=(const RefVector& o) const { return vector_ != o.vector_; }

  wtf_size_t size() const { return vector_.size(); }
  bool IsEmpty() const { return !size(); }
  void push_back(const T& decoration) { vector_.push_back(decoration); }
  template <typename... Args>
  T& emplace_back(Args&&... args) {
    return vector_.emplace_back(std::forward<Args>(args)...);
  }
  const Vector<T>& GetVector() const { return vector_; }
  Vector<T>* GetMutableVector() { return &vector_; }

 private:
  Vector<T> vector_;
  RefVector() = default;
  RefVector(const Vector<T>& vector) : vector_(vector) {}
  RefVector(Vector<T>&& vector) : vector_(vector) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_REF_VECTOR_H_
