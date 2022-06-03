// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_ATOMIC_FLAG_H_
#define MOJO_CORE_ATOMIC_FLAG_H_

#include "base/atomicops.h"
#include "base/macros.h"

namespace mojo {
namespace core {

// AtomicFlag is a boolean flag that can be set and tested atomically. It is
// intended to be used to fast-path checks where the common case would normally
// release the governing mutex immediately after checking.
//
// Example usage:
// void DoFoo(Bar* bar) {
//   AutoLock l(lock_);
//   queue_.push_back(bar);
//   flag_.Set(true);
// }
//
// void Baz() {
//   if (!flag_)  // Assume this is the common case.
//     return;
//
//   AutoLock l(lock_);
//   ... drain queue_ ...
//   flag_.Set(false);
// }
class AtomicFlag {
 public:
  AtomicFlag() : flag_(0) {}

  AtomicFlag(const AtomicFlag&) = delete;
  AtomicFlag& operator=(const AtomicFlag&) = delete;

  ~AtomicFlag() = default;

  void Set(bool value) { base::subtle::Release_Store(&flag_, value ? 1 : 0); }

  bool Get() const { return base::subtle::Acquire_Load(&flag_) ? true : false; }

  operator const bool() const { return Get(); }

 private:
  base::subtle::Atomic32 flag_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_ATOMIC_FLAG_H_
