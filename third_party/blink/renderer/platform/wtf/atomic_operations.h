// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ATOMIC_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ATOMIC_OPERATIONS_H_

#include <atomic>
#include <cstddef>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

// TOOD(omerkatz): Replace these casts with std::atomic_ref (C++20) once it
// becomes available.
template <typename T>
ALWAYS_INLINE std::atomic<T>* AsAtomicPtr(T* t) {
  return reinterpret_cast<std::atomic<T>*>(t);
}

template <typename T>
ALWAYS_INLINE const std::atomic<T>* AsAtomicPtr(const T* t) {
  return reinterpret_cast<const std::atomic<T>*>(t);
}

// Copies |bytes| bytes from |from| to |to| using atomic reads. Assumes |to|
// and |from| are size_t-aligned and point to buffers of size |bytes|. Note
// that atomicity is guaranteed only per word, not for the entire |bytes|
// bytes as a whole. When copying arrays of elements, If |to| and |from|
// are overlapping, should move the elements one by one.
WTF_EXPORT void AtomicReadMemcpy(void* to, const void* from, size_t bytes);

template <size_t bytes>
ALWAYS_INLINE void AtomicReadMemcpy(void* to, const void* from) {
  static_assert(bytes > 0, "Number of copied bytes should be greater than 0");
  AtomicReadMemcpy(to, from, bytes);
}

// AtomicReadMemcpy specializations:

#if defined(ARCH_CPU_X86_64)
template <>
ALWAYS_INLINE void AtomicReadMemcpy<sizeof(uint32_t)>(void* to,
                                                      const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(uint32_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(uint32_t) - 1));
  *reinterpret_cast<uint32_t*>(to) =
      AsAtomicPtr(reinterpret_cast<const uint32_t*>(from))
          ->load(std::memory_order_relaxed);
}
#endif  // ARCH_CPU_X86_64

template <>
ALWAYS_INLINE void AtomicReadMemcpy<sizeof(size_t)>(void* to,
                                                    const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  *reinterpret_cast<size_t*>(to) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from))
          ->load(std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicReadMemcpy<2 * sizeof(size_t)>(void* to,
                                                        const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  *reinterpret_cast<size_t*>(to) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from))
          ->load(std::memory_order_relaxed);
  *(reinterpret_cast<size_t*>(to) + 1) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from) + 1)
          ->load(std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicReadMemcpy<3 * sizeof(size_t)>(void* to,
                                                        const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  *reinterpret_cast<size_t*>(to) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from))
          ->load(std::memory_order_relaxed);
  *(reinterpret_cast<size_t*>(to) + 1) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from) + 1)
          ->load(std::memory_order_relaxed);
  *(reinterpret_cast<size_t*>(to) + 2) =
      AsAtomicPtr(reinterpret_cast<const size_t*>(from) + 2)
          ->load(std::memory_order_relaxed);
}

// Copies |bytes| bytes from |from| to |to| using atomic writes. Assumes |to|
// and |from| are size_t-aligned and point to buffers of size |bytes|. Note
// that atomicity is guaranteed only per word, not for the entire |bytes|
// bytes as a whole. When copying arrays of elements, If |to| and |from| are
// overlapping, should move the elements one by one.
WTF_EXPORT void AtomicWriteMemcpy(void* to, const void* from, size_t bytes);
template <size_t bytes>
ALWAYS_INLINE void AtomicWriteMemcpy(void* to, const void* from) {
  static_assert(bytes > 0, "Number of copied bytes should be greater than 0");
  AtomicWriteMemcpy(to, from, bytes);
}

// AtomicReadMemcpy specializations:

#if defined(ARCH_CPU_X86_64)
template <>
ALWAYS_INLINE void AtomicWriteMemcpy<sizeof(uint32_t)>(void* to,
                                                       const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(uint32_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(uint32_t) - 1));
  AsAtomicPtr(reinterpret_cast<uint32_t*>(to))
      ->store(*reinterpret_cast<const uint32_t*>(from),
              std::memory_order_relaxed);
}
#endif  // ARCH_CPU_X86_64

template <>
ALWAYS_INLINE void AtomicWriteMemcpy<sizeof(size_t)>(void* to,
                                                     const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(to))
      ->store(*reinterpret_cast<const size_t*>(from),
              std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicWriteMemcpy<2 * sizeof(size_t)>(void* to,
                                                         const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(to))
      ->store(*reinterpret_cast<const size_t*>(from),
              std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(to) + 1)
      ->store(*(reinterpret_cast<const size_t*>(from) + 1),
              std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicWriteMemcpy<3 * sizeof(size_t)>(void* to,
                                                         const void* from) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(to))
      ->store(*reinterpret_cast<const size_t*>(from),
              std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(to) + 1)
      ->store(*(reinterpret_cast<const size_t*>(from) + 1),
              std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(to) + 2)
      ->store(*(reinterpret_cast<const size_t*>(from) + 2),
              std::memory_order_relaxed);
}

// Set the first |bytes| bytes of |buf| to 0 using atomic writes. Assumes |buf|
// is size_t-aligned and points to a buffer of size at least |bytes|. Note
// that atomicity is guaranteed only per word, not for the entire |bytes| bytes
// as a whole.
WTF_EXPORT void AtomicMemzero(void* buf, size_t bytes);

template <size_t bytes>
ALWAYS_INLINE void AtomicMemzero(void* buf) {
  static_assert(bytes > 0, "Number of copied bytes should be greater than 0");
  AtomicMemzero(buf, bytes);
}

// AtomicReadMemcpy specializations:

#if defined(ARCH_CPU_X86_64)
template <>
ALWAYS_INLINE void AtomicMemzero<sizeof(uint32_t)>(void* buf) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(buf) & (sizeof(uint32_t) - 1));
  AsAtomicPtr(reinterpret_cast<uint32_t*>(buf))
      ->store(0, std::memory_order_relaxed);
}
#endif  // ARCH_CPU_X86_64

template <>
ALWAYS_INLINE void AtomicMemzero<sizeof(size_t)>(void* buf) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(buf) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(buf))
      ->store(0, std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicMemzero<2 * sizeof(size_t)>(void* buf) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(buf) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(buf))
      ->store(0, std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(buf) + 1)
      ->store(0, std::memory_order_relaxed);
}

template <>
ALWAYS_INLINE void AtomicMemzero<3 * sizeof(size_t)>(void* buf) {
  DCHECK_EQ(0u, reinterpret_cast<size_t>(buf) & (sizeof(size_t) - 1));
  AsAtomicPtr(reinterpret_cast<size_t*>(buf))
      ->store(0, std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(buf) + 1)
      ->store(0, std::memory_order_relaxed);
  AsAtomicPtr(reinterpret_cast<size_t*>(buf) + 2)
      ->store(0, std::memory_order_relaxed);
}

// Swaps values using atomic writes.
template <typename T>
ALWAYS_INLINE void AtomicWriteSwap(T& lhs, T& rhs) {
  T tmp_val = rhs;
  AsAtomicPtr(&rhs)->store(lhs, std::memory_order_relaxed);
  AsAtomicPtr(&lhs)->store(tmp_val, std::memory_order_relaxed);
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ATOMIC_OPERATIONS_H_
