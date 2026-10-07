// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SAFE_NON_RETAINING_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SAFE_NON_RETAINING_PERSISTENT_H_

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// `SafeNonRetainingPersistent<T>` holds a weak reference (`WeakPersistent<T>`)
// from a non-garbage-collected object to a garbage-collected object of type
// `T`, and `CHECK()`s that the reference is not null whenever it is accessed.
//
// When to use:
// Use `SafeNonRetainingPersistent<T>` for back-references from a
// non-garbage-collected object to a garbage-collected object that owns it (e.g.
// via `std::unique_ptr` or `scoped_refptr`), where the garbage-collected object
// is guaranteed to outlive the non-garbage-collected object:
// - A strong `Persistent<T>` cannot be used in this case because it would
//   create an uncollectible reference cycle across the Oilpan/non-Oilpan
//   boundary and leak both objects.
// - A raw pointer (`T*` or `raw_ptr<T>`) must not be used to reference
//   garbage-collected objects from off-heap fields.
// - Unlike `WeakPersistent<T>`, `SafeNonRetainingPersistent<T>` expresses the
//   invariant that the referenced object is always alive when accessed, and
//   enforces it via `CHECK()` on every access so that any lifetime bug results
//   in an immediate, deterministic crash rather than a use-after-free or silent
//   null dereference.
//
// If the non-garbage-collected object can legitimately outlive the
// garbage-collected object and needs to dynamically check whether the
// referenced object is still alive, use `WeakPersistent<T>` instead.
//
// Note: Where feasible, prefer migrating the non-garbage-collected object to
// Oilpan (`GarbageCollected`) and using `Member<T>`, which allows Oilpan to
// manage cycles automatically.
template <typename T>
class SafeNonRetainingPersistent final {
 public:
  using PointeeType = T;

  SafeNonRetainingPersistent(  // NOLINT
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(loc) {}

  SafeNonRetainingPersistent(  // NOLINT
      std::nullptr_t,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(nullptr, loc) {}

  SafeNonRetainingPersistent(  // NOLINT
      T* raw,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(raw, loc) {}

  SafeNonRetainingPersistent(  // NOLINT
      T& raw,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(raw, loc) {}

  SafeNonRetainingPersistent(
      const SafeNonRetainingPersistent& other,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(other.ref_, loc) {}

  template <typename U>
    requires(std::is_base_of_v<T, U>)
  // NOLINTNEXTLINE
  SafeNonRetainingPersistent(
      const SafeNonRetainingPersistent<U>& other,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(other.ref_, loc) {}

  SafeNonRetainingPersistent(
      SafeNonRetainingPersistent&& other,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) noexcept
      : ref_(std::move(other.ref_), loc) {}

  template <typename U,
            typename OtherWeaknessPolicy,
            typename OtherLocationPolicy,
            typename OtherCheckingPolicy>
    requires(std::is_base_of_v<T, U>)
  // NOLINTNEXTLINE
  SafeNonRetainingPersistent(
      const cppgc::internal::BasicPersistent<U,
                                             OtherWeaknessPolicy,
                                             OtherLocationPolicy,
                                             OtherCheckingPolicy>& other,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(other, loc) {}

  template <typename U,
            typename MemberBarrierPolicy,
            typename MemberWeaknessTag,
            typename MemberCheckingPolicy,
            typename MemberStorageType>
    requires(std::is_base_of_v<T, U>)
  // NOLINTNEXTLINE
  SafeNonRetainingPersistent(
      const cppgc::internal::BasicMember<U,
                                         MemberBarrierPolicy,
                                         MemberWeaknessTag,
                                         MemberCheckingPolicy,
                                         MemberStorageType>& member,
      const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE)
      : ref_(member, loc) {}

  ~SafeNonRetainingPersistent() = default;

  SafeNonRetainingPersistent& operator=(
      const SafeNonRetainingPersistent& other) {
    ref_ = other.ref_;
    return *this;
  }

  template <typename U>
    requires(std::is_base_of_v<T, U>)
  SafeNonRetainingPersistent& operator=(
      const SafeNonRetainingPersistent<U>& other) {
    ref_ = other.ref_;
    return *this;
  }

  SafeNonRetainingPersistent& operator=(
      SafeNonRetainingPersistent&& other) noexcept {
    ref_ = std::move(other.ref_);
    return *this;
  }

  template <typename U,
            typename OtherWeaknessPolicy,
            typename OtherLocationPolicy,
            typename OtherCheckingPolicy>
    requires(std::is_base_of_v<T, U>)
  SafeNonRetainingPersistent& operator=(
      const cppgc::internal::BasicPersistent<U,
                                             OtherWeaknessPolicy,
                                             OtherLocationPolicy,
                                             OtherCheckingPolicy>& other) {
    ref_ = other;
    return *this;
  }

  template <typename U,
            typename MemberBarrierPolicy,
            typename MemberWeaknessTag,
            typename MemberCheckingPolicy,
            typename MemberStorageType>
    requires(std::is_base_of_v<T, U>)
  SafeNonRetainingPersistent& operator=(
      const cppgc::internal::BasicMember<U,
                                         MemberBarrierPolicy,
                                         MemberWeaknessTag,
                                         MemberCheckingPolicy,
                                         MemberStorageType>& member) {
    ref_ = member;
    return *this;
  }

  SafeNonRetainingPersistent& operator=(T* other) {
    ref_ = other;
    return *this;
  }

  SafeNonRetainingPersistent& operator=(std::nullptr_t) {
    Clear();
    return *this;
  }

  // NOLINTNEXTLINE
  operator T*() const { return Get(); }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  T* Get() const {
    T* ptr = ref_.Get();
    CHECK(ptr);
    return ptr;
  }

  void Clear() { ref_.Clear(); }

  T* Release() {
    T* result = Get();
    Clear();
    return result;
  }

 private:
  template <typename U>
  friend class SafeNonRetainingPersistent;

  WeakPersistent<T> ref_;
};

template <typename T>
SafeNonRetainingPersistent<T> WrapUnretainedPersistent(
    T* value,
    const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
  return SafeNonRetainingPersistent<T>(value, loc);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SAFE_NON_RETAINING_PERSISTENT_H_
