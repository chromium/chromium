// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_REENTRANCY_CHECKER_H_
#define MEDIA_BASE_REENTRANCY_CHECKER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"

// Asserts that a non-reentrant scope (which can span multiple methods) is not
// reentered.
//
// No-op and zero-sized when DCHECKs are disabled at build time.
//
// Failures are reported by LOG(FATAL):
//   [...:FATAL:reentrancy_checker.cc(15)] Non-reentrant scope reentered
//   #0 0x7f12ef2ee8dd base::debug::StackTrace::StackTrace()
//   #1 0x7f12eefdffaa base::debug::StackTrace::StackTrace()
//   #2 0x7f12ef051e0b logging::LogMessage::~LogMessage()
//   #3 0x7f12edc8bfc6 media::NonReentrantScope::NonReentrantScope()
//   #4 0x7f12edd93e41 MyClass::MyMethod()
//
// Usage:
//   class MyClass {
//    public:
//     void MyMethod() {
//       NON_REENTRANT_SCOPE(my_reentrancy_checker_);
//       ...
//      }
//
//    private:
//     REENTRANCY_CHECKER(my_reentrancy_checker_);
//   };

#if DCHECK_IS_ON()
#define REENTRANCY_CHECKER(name) ::base::Lock name
#define NON_REENTRANT_SCOPE(name) ::media::NonReentrantScope name##scope(name)
#else  // DCHECK_IS_ON()
#define REENTRANCY_CHECKER(name) static_assert(true, "")
#define NON_REENTRANT_SCOPE(name)
#endif  // DCHECK_IS_ON()

namespace media {

// Implementation of NON_REENTRANT_SCOPE(). Do not use directly.
class SCOPED_LOCKABLE MEDIA_EXPORT NonReentrantScope {
 public:
  explicit NonReentrantScope(base::Lock& lock) EXCLUSIVE_LOCK_FUNCTION(lock);
  ~NonReentrantScope() UNLOCK_FUNCTION();

 private:
  base::Lock& lock_;
  bool is_lock_holder_ = false;

  DISALLOW_COPY_AND_ASSIGN(NonReentrantScope);
};

}  // namespace media

#endif  // MEDIA_BASE_REENTRANCY_CHECKER_H_
