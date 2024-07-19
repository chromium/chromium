// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_REF_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_REF_H_

#include "base/memory/weak_ptr.h"

namespace content {

// A templated type used to track CacheStorage and CacheStorageCache references.
// The TargetType must provide AddHandleRef() and DropHandleRef() methods, which
// are called when the CacheStorageRef<> object is created and destroyed.
//
// Each CacheStorageRef corresponds to a high level reference from DOM objects
// exposed to JavaScript, and does not correspond to typical smart pointer
// reference counts. Internally this class uses a WeakPtr<> to the target and
// Cache Storage must manage the target's lifecycle through other mechanisms.
template <typename TargetType>
class CacheStorageRef {
 public:
  CacheStorageRef() = default;

  explicit CacheStorageRef(base::WeakPtr<TargetType> target)
      : target_(std::move(target)) {
    target_->AddHandleRef();
  }

  CacheStorageRef(CacheStorageRef&& rhs) noexcept
      : target_(std::move(rhs.target_)) {}

  CacheStorageRef& operator=(CacheStorageRef&& rhs) {
    if (target_)
      target_->DropHandleRef();
    target_ = std::move(rhs.target_);
    return *this;
  }

  CacheStorageRef(const CacheStorageRef&) = delete;
  CacheStorageRef& operator=(const CacheStorageRef&) = delete;

  ~CacheStorageRef() {
    if (target_)
      target_->DropHandleRef();
  }

  TargetType* value() const { return target_.get(); }

  CacheStorageRef Clone() const { return CacheStorageRef(target_); }

 private:
  base::WeakPtr<TargetType> target_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_REF_H_
