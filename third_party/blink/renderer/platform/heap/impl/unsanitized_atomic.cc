// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/unsanitized_atomic.h"

#include "cstdint"

#include "base/compiler_specific.h"

#if HAS_FEATURE(address_sanitizer)
#error "Must be built without asan."
#endif

namespace blink {
namespace internal {

namespace {
constexpr int ToGCCMemoryOrder(std::memory_order order) {
  switch (order) {
    case std::memory_order_seq_cst:
      return __ATOMIC_SEQ_CST;
    case std::memory_order_relaxed:
      return __ATOMIC_RELAXED;
    case std::memory_order_acquire:
      return __ATOMIC_ACQUIRE;
    case std::memory_order_release:
      return __ATOMIC_RELEASE;
    case std::memory_order_acq_rel:
      return __ATOMIC_ACQ_REL;
    case std::memory_order_consume:
      return __ATOMIC_CONSUME;
  }
}
}  // namespace

template <typename T>
void UnsanitizedAtomic<T>::store(T desired, std::memory_order order) {
  __atomic_store(&value_, &desired, ToGCCMemoryOrder(order));
}

template <typename T>
T UnsanitizedAtomic<T>::load(std::memory_order order) const {
  T result;
  __atomic_load(&value_, &result, ToGCCMemoryOrder(order));
  return result;
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_strong(T& expected,
                                                   T desired,
                                                   std::memory_order order) {
  return compare_exchange_strong(expected, desired, order, order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_strong(
    T& expected,
    T desired,
    std::memory_order succ_order,
    std::memory_order fail_order) {
  return __atomic_compare_exchange(&value_, &expected, &desired, false,
                                   ToGCCMemoryOrder(succ_order),
                                   ToGCCMemoryOrder(fail_order));
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_weak(T& expected,
                                                 T desired,
                                                 std::memory_order order) {
  return compare_exchange_weak(expected, desired, order, order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_weak(T& expected,
                                                 T desired,
                                                 std::memory_order succ_order,
                                                 std::memory_order fail_order) {
  return __atomic_compare_exchange(&value_, &expected, &desired, true,
                                   ToGCCMemoryOrder(succ_order),
                                   ToGCCMemoryOrder(fail_order));
}

template class PLATFORM_EXPORT UnsanitizedAtomic<uint16_t>;

}  // namespace internal
}  // namespace blink
