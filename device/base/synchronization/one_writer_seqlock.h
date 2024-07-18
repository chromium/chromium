// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_
#define DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "base/atomicops.h"
#include "base/check_op.h"

namespace device {

// This SeqLock handles only *one* writer and multiple readers. It may be
// suitable for low-contention with relatively infrequent writes, and many
// readers. See:
//   http://en.wikipedia.org/wiki/Seqlock
//   http://www.concurrencykit.org/doc/ck_sequence.html
// This implementation is based on ck_sequence.h from http://concurrencykit.org.
//
// Currently this type of lock is used in at least two implementations (gamepad
// and device motion, in particular see e.g. shared_memory_seqlock_buffer.h).
// It may make sense to generalize this lock to multiple writers.
//
// You must be very careful not to operate on potentially inconsistent read
// buffers. If the read must be retry'd, the data in the read buffer could
// contain any random garbage. e.g., contained pointers might be
// garbage, or indices could be out of range. Probably the only suitable thing
// to do during the read loop is to make a copy of the data, and operate on it
// only after the read was found to be consistent.
//
// Accesses to data protected by this SeqLock is recommended to be atomic.
// See:
//   https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf
//   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1478r1.html
class OneWriterSeqLock {
 public:
  OneWriterSeqLock();

  OneWriterSeqLock(const OneWriterSeqLock&) = delete;
  OneWriterSeqLock& operator=(const OneWriterSeqLock&) = delete;

  // Copies data from src into dest using atomic stores. This should be used by
  // writer of SeqLock. Data must be 4-byte aligned.
  template <typename T>
  static void AtomicWriterMemcpy(T* dest, const T* src, size_t size);
  // Copies data from src into dest using atomic loads. This should be used by
  // readers of SeqLock. Data must be 4-byte aligned.
  template <typename T>
  static void AtomicReaderMemcpy(T* dest, const T* src, size_t size);
  // ReadBegin returns |sequence_| when it is even, or when it has retried
  // |max_retries| times. Omitting |max_retries| results in ReadBegin not
  // returning until |sequence_| is even.
  int32_t ReadBegin(uint32_t max_retries = UINT32_MAX) const;
  bool ReadRetry(int32_t version) const;
  void WriteBegin();
  void WriteEnd();

 private:
  std::atomic<int32_t> sequence_;
};

// static
template <typename T>
void OneWriterSeqLock::AtomicReaderMemcpy(T* dest, const T* src, size_t size) {
  static_assert(std::is_trivially_copyable<T>::value,
                "AtomicReaderMemcpy requires a trivially copyable type");

  DCHECK_EQ(reinterpret_cast<std::uintptr_t>(dest) % 4, 0U);
  DCHECK_EQ(reinterpret_cast<std::uintptr_t>(src) % 4, 0U);
  DCHECK_EQ(size % 4, 0U);
  for (size_t i = 0; i < size / 4; ++i) {
    reinterpret_cast<int32_t*>(dest)[i] =
        reinterpret_cast<const std::atomic<int32_t>*>(src)[i].load(
            std::memory_order_relaxed);
  }
}

// static
template <typename T>
void OneWriterSeqLock::AtomicWriterMemcpy(T* dest, const T* src, size_t size) {
  static_assert(std::is_trivially_copyable<T>::value,
                "AtomicWriterMemcpy requires a trivially copyable type");

  DCHECK_EQ(reinterpret_cast<std::uintptr_t>(dest) % 4, 0U);
  DCHECK_EQ(reinterpret_cast<std::uintptr_t>(src) % 4, 0U);
  DCHECK_EQ(size % 4, 0U);
  for (size_t i = 0; i < size / 4; ++i) {
    reinterpret_cast<std::atomic<int32_t>*>(dest)[i].store(
        reinterpret_cast<const int32_t*>(src)[i], std::memory_order_relaxed);
  }
}

}  // namespace device

#endif  // DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_
