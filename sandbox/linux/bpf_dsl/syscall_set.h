// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_SYSCALL_SET_H__
#define SANDBOX_LINUX_BPF_DSL_SYSCALL_SET_H__

#include <stdint.h>

#include <iterator>

#include "sandbox/sandbox_export.h"

namespace sandbox {

// Iterates over the entire system call range from 0..0xFFFFFFFFu. This
// iterator is aware of how system calls look like and will skip quickly
// over ranges that can't contain system calls. It iterates more slowly
// whenever it reaches a range that is potentially problematic, returning
// the last invalid value before a valid range of system calls, and the
// first invalid value after a valid range of syscalls. It iterates over
// individual values whenever it is in the normal range for system calls
// (typically MIN_SYSCALL..MAX_SYSCALL).
//
// Example usage:
//   for (uint32_t sysnum : SyscallSet::All()) {
//     // Do something with sysnum.
//   }
class SANDBOX_EXPORT SyscallSet {
 public:
  class Iterator;

  SyscallSet(const SyscallSet& ss) : set_(ss.set_) {}

  SyscallSet& operator=(const SyscallSet&) = delete;

  ~SyscallSet() {}

  Iterator begin() const;
  Iterator end() const;

  // All returns a SyscallSet that contains both valid and invalid
  // system call numbers.
  static SyscallSet All() { return SyscallSet(Set::ALL); }

  // ValidOnly returns a SyscallSet that contains only valid system
  // call numbers.
  static SyscallSet ValidOnly() { return SyscallSet(Set::VALID_ONLY); }

  // InvalidOnly returns a SyscallSet that contains only invalid
  // system call numbers, but still omits numbers in the middle of a
  // range of invalid system call numbers.
  static SyscallSet InvalidOnly() { return SyscallSet(Set::INVALID_ONLY); }

  // IsValid returns whether |num| specifies a valid system call
  // number.
  static bool IsValid(uint32_t num);

 private:
  enum class Set { ALL, VALID_ONLY, INVALID_ONLY };

  explicit SyscallSet(Set set) : set_(set) {}

  Set set_;

  friend bool operator==(const SyscallSet&, const SyscallSet&);
};

SANDBOX_EXPORT bool operator==(const SyscallSet& lhs, const SyscallSet& rhs);

// Iterator provides C++ input iterator semantics for traversing a
// SyscallSet.
class SyscallSet::Iterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = uint32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = uint32_t*;
  using reference = uint32_t&;

  Iterator(const Iterator& it)
      : set_(it.set_), done_(it.done_), num_(it.num_) {}

  Iterator& operator=(const Iterator&) = delete;

  ~Iterator() {}

  uint32_t operator*() const;
  Iterator& operator++();

 private:
  Iterator(Set set, bool done);

  uint32_t NextSyscall() const;

  Set set_;
  bool done_;
  uint32_t num_;

  friend SyscallSet;
  friend bool operator==(const Iterator&, const Iterator&);
};

SANDBOX_EXPORT bool operator==(const SyscallSet::Iterator& lhs,
                               const SyscallSet::Iterator& rhs);
SANDBOX_EXPORT bool operator!=(const SyscallSet::Iterator& lhs,
                               const SyscallSet::Iterator& rhs);

}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_SYSCALL_SET_H__
