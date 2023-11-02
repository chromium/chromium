// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_ATOMIC_FLAG_H_
#define MOJO_CORE_ATOMIC_FLAG_H_

#include <atomic>

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
  AtomicFlag();
  AtomicFlag(const AtomicFlag&) = delete;
  AtomicFlag& operator=(const AtomicFlag&) = delete;

  ~AtomicFlag() = default;

  void Set(bool value) { flag_.store(value, std::memory_order_release); }

  bool Get() const { return flag_.load(std::memory_order_acquire); }

  operator const bool() const { return Get(); }

 private:
  std::atomic<bool> flag_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_ATOMIC_FLAG_H_
