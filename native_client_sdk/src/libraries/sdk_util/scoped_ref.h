// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_SDK_UTIL_SCOPED_REF_H_
#define LIBRARIES_SDK_UTIL_SCOPED_REF_H_

#include <stdlib.h>

#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"

namespace sdk_util {

class ScopedRefBase {
 protected:
  ScopedRefBase() : ptr_(NULL) {}
  ~ScopedRefBase() { reset(NULL); }

  void reset(RefObject* obj) {
    if (obj) {
      obj->Acquire();
    }
    if (ptr_) {
      ptr_->Release();
    }
    ptr_ = obj;
  }

 protected:
  RefObject* ptr_;
};

template <class T>
class ScopedRef : public ScopedRefBase {
 public:
  ScopedRef() {}
  ScopedRef(const ScopedRef& ptr) { reset(ptr.get()); }
  explicit ScopedRef(T* ptr) { reset(ptr); }

  ScopedRef& operator=(const ScopedRef& ptr) {
    reset(ptr.get());
    return *this;
  }

  template <typename U>
  ScopedRef(const ScopedRef<U>& ptr) {
    reset(ptr.get());
  }

  template <typename U>
  ScopedRef& operator=(const ScopedRef<U>& ptr) {
    reset(ptr.get());
    return *this;
  }

  void reset(T* obj = NULL) { ScopedRefBase::reset(obj); }
  T* get() const { return static_cast<T*>(ptr_); }

  template <typename U>
  bool operator==(const ScopedRef<U>& p) const {
    return get() == p.get();
  }

  template <typename U>
  bool operator!=(const ScopedRef<U>& p) const {
    return get() != p.get();
  }

 public:
  T& operator*() const { return *get(); }
  T* operator->() const { return get(); }

 private:
  typedef void (ScopedRef::*bool_as_func_ptr)() const;
  void bool_as_func_impl() const {}

 public:
  operator bool_as_func_ptr() const {
    return (ptr_ != NULL) ? &ScopedRef::bool_as_func_impl : 0;
  }
};

template <typename U, typename T>
ScopedRef<U> static_scoped_ref_cast(const ScopedRef<T>& ptr) {
  return ScopedRef<U>(static_cast<U*>(ptr.get()));
}

}  // namespace sdk_util

#endif  // LIBRARIES_SDK_UTIL_SCOPED_REF_H_
