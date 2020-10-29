// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNSANITIZED_ATOMIC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNSANITIZED_ATOMIC_H_

#include <atomic>

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace internal {

// Simple wrapper for std::atomic<> that makes sure that accesses to underlying
// data are not sanitized. This is needed because the no_sanitize_address
// attribute doesn't propagate down to callees. Must be used with care.
// Currently is only used to access poisoned HeapObjectHeader. For derived or
// user types an explicit instantiation must be added to unsanitized_atomic.cc.
template <typename T>
class PLATFORM_EXPORT UnsanitizedAtomic final {
 public:
  UnsanitizedAtomic() = default;
  explicit UnsanitizedAtomic(T value) : value_(value) {}

  void store(T, std::memory_order = std::memory_order_seq_cst);
  T load(std::memory_order = std::memory_order_seq_cst) const;

  bool compare_exchange_strong(T&,
                               T,
                               std::memory_order = std::memory_order_seq_cst);
  bool compare_exchange_strong(T&, T, std::memory_order, std::memory_order);

  bool compare_exchange_weak(T&,
                             T,
                             std::memory_order = std::memory_order_seq_cst);
  bool compare_exchange_weak(T&, T, std::memory_order, std::memory_order);

 private:
  T value_;
};

template <typename T>
auto* AsUnsanitizedAtomic(T* ptr) {
#if defined(ADDRESS_SANITIZER)
  return reinterpret_cast<UnsanitizedAtomic<T>*>(ptr);
#else
  return WTF::AsAtomicPtr(ptr);
#endif
}

template <typename T>
const auto* AsUnsanitizedAtomic(const T* ptr) {
#if defined(ADDRESS_SANITIZER)
  return reinterpret_cast<const UnsanitizedAtomic<T>*>(ptr);
#else
  return WTF::AsAtomicPtr(ptr);
#endif
}

}  // namespace internal
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNSANITIZED_ATOMIC_H_
