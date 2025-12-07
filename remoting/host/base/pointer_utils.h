// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_POINTER_UTILS_H_
#define REMOTING_HOST_BASE_POINTER_UTILS_H_

#include <concepts>
#include <cstddef>
#include <functional>

#include "base/memory/raw_ptr.h"

namespace remoting {

// Allows specifying a delete function at the type level so it doesn't have
// to be specified at each construction of the std::unique_ptr or stored
// inline. So instead of saying
//
//     std::unique_ptr<foo, void(*)(foo*)> foo_ptr{make_foo_ptr(), &foo_free};
//
// and having foo_ptr have to hold two pointers, one can say
//
//     std::unique_ptr<foo, DeleteFunc<foo_free>> foo_ptr{make_foo_ptr()};
//
// and have foo_ptr only hold the object pointer.
template <auto kDeleteFunction>
struct DeleteFunc {
  void operator()(auto* ptr) { kDeleteFunction(ptr); }
};

// A generic smart pointer for working with reference counted objects exposed by
// C APIs. Holds an owned reference to a object of type T, whose reference be
// managed with kRefFunc, kUnrefFunc, and optionally kTakeFunc and kRefSinkFunc.
//
// Typically used via a type alias, e.g.,
//
//     using FooPtr = CRefCounted<foo_ref, foo_unref>;
template <typename T,
          // Takes a pointer to T, increases the reference count, and returns
          // the pointer.
          auto kRefFunc,
          // Takes a pointer to T, decreases the reference count, and frees the
          // object if the reference count is now zero.
          auto kUnrefFunc,
          // Takes a pointer to T, performs any action needed to take ownership
          // of an existing reference (such as sinking a floating reference),
          // and returns the pointer.
          auto kTakeFunc = std::identity(),
          // Takes a pointer to T, and sinks the existing floating reference, if
          // any, or otherwise increases the reference count. (Only specify for
          // types supporting floating references.)
          auto kRefSinkFunc = nullptr>
class CRefCounted {
 public:
  // Constructs a null reference.
  CRefCounted() = default;

  // Creates a new owned reference, increasing the reference count.
  CRefCounted(const CRefCounted& other)
      : ptr_(other.ptr_ ? kRefFunc(other.ptr_) : nullptr) {}

  // Takes ownership of an existing reference, leaving |other| null.
  CRefCounted(CRefCounted&& other) : ptr_(other.ptr_) { other.ptr_ = nullptr; }

  // Unrefs the current object, if any, and assigns to reference the same object
  // as |other|, increasing the reference count.
  CRefCounted& operator=(const CRefCounted& other) {
    if (this == &other) {
      return *this;
    }
    reset();
    if (other.ptr_) {
      ptr_ = kRefFunc(other.ptr_);
    }
    return *this;
  }

  // Unrefs the current object, if any, and takes ownship of the reference in
  // |other|, leaving |other| null.
  CRefCounted& operator=(CRefCounted&& other) {
    if (this == &other) {
      return *this;
    }
    reset();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  // Unrefs the current object.
  ~CRefCounted() { reset(); }

  // Unrefs the current object and sets reference to null.
  void reset() {
    if (ptr_) {
      kUnrefFunc(ptr_.ExtractAsDangling());
    }
  }

  // Gets the pointer to the referenced object.
  T* get() const { return ptr_; }

  // Returns the pointer to the referenced object and releases the ownership,
  // setting this reference to null without decreasing the reference count.
  [[nodiscard]] T* release() {
    T* ptr = ptr_;
    ptr_ = nullptr;
    return ptr;
  }

  // Smart pointer operators
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  // Checks if |this| and |other| reference the same object.
  bool operator==(const CRefCounted& other) const { return ptr_ == other.ptr_; }

  // Takes ownership of an existing raw object pointer without increasing the
  // reference count.
  static CRefCounted Take(T* ptr) { return CRefCounted(kTakeFunc(ptr)); }

  // Creates a new owned reference to |ptr| by increasing the reference count.
  static CRefCounted Ref(T* ptr) { return CRefCounted(kRefFunc(ptr)); }

  // If |ptr| is floating, takes ownership and sinks the floating reference.
  // Otherwise, creates a new owned reference by increasing the reference count.
  static CRefCounted RefSink(T* ptr)
    requires(!std::same_as<decltype(kRefSinkFunc), std::nullptr_t>)
  {
    return CRefCounted(kRefSinkFunc(ptr));
  }

 private:
  explicit CRefCounted(T* ptr) : ptr_(ptr) {}
  raw_ptr<T> ptr_ = nullptr;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_POINTER_UTILS_H_
