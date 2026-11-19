// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_

#include <new>

#include "base/check.h"
#include "base/memory/stack_allocated.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink::internal {

// A dummy class used in following macros.
class __thisIsHereToForceASemicolonAfterThisMacro;

struct DisallowNewTag;
struct FastMallocTag;

// Internal helper for compile-time validation of USING_FAST_MALLOC. Do not use
// directly; see USING_FAST_MALLOC_TYPE_CHECKS below for the only intended usage
// pattern:
//
// 1. Each call site declares a fresh class-local `struct FastMallocCheckTag;`
//    and passes it as `Tag`. Without a distinct `Tag` per class, instantiating
//    these templates from a CRTP base like `RefCounted<T>` while `T` is still
//    incomplete causes Clang to memoize a `false` result, which would cause a
//    subsequent check inside `T`'s own class body to pass incorrectly.
// 2. Each call site grants friendship to `FastMallocChecker` so the `requires`
//    expressions can inspect marker aliases inherited through private or
//    protected base classes (without friendship, accessing a private base's
//    alias fails access checking and silently evaluates to `false`).
struct FastMallocChecker {
  template <typename T, typename Tag>
  static consteval bool HasGCMarker() {
    return requires { typename T::IsGarbageCollectedTypeMarker; } ||
           requires { typename T::IsGarbageCollectedMixinTypeMarker; };
  }

  template <typename T, typename Tag>
  static consteval bool HasDisallowNewMarker() {
    return requires { typename T::IsDisallowNewMarker; };
  }

  template <typename T, typename Tag>
  static consteval bool HasStackAllocatedMarker() {
    return requires { typename T::IsStackAllocatedTypeMarker; };
  }
};

}  // namespace blink::internal

// Classes that contain references to garbage-collected objects but aren't
// themselves garbaged allocated, have some extra macros available which
// allows their use to be restricted to cases where the garbage collector
// is able to discover their references. These macros will be useful for
// non-garbage-collected objects to avoid unintended allocations.
//
// STACK_ALLOCATED() classes may contain raw pointers to garbage-collected
// objects.
//
// DISALLOW_NEW(): Cannot be allocated with new operators but can be a
// part of object, a value object in collections or stack allocated. If it has
// Members you need a trace method and the containing object needs to call that
// trace method.
//
#define DISALLOW_NEW()                                           \
 public:                                                         \
  using IsDisallowNewMarker [[maybe_unused]] = int;              \
  void* operator new(size_t, base::NotNullTag, void* location) { \
    return location;                                             \
  }                                                              \
  void* operator new(size_t, void* location) {                   \
    return location;                                             \
  }                                                              \
                                                                 \
 private:                                                        \
  using BlinkAllocatorMacroTag [[maybe_unused]] =                \
      ::blink::internal::DisallowNewTag;                         \
  void* operator new(size_t) = delete;                           \
                                                                 \
 public:                                                         \
  friend class ::blink::internal::__thisIsHereToForceASemicolonAfterThisMacro

#define STATIC_ONLY(Type)                                       \
  Type() = delete;                                              \
  Type(const Type&) = delete;                                   \
  Type& operator=(const Type&) = delete;                        \
  void* operator new(size_t) = delete;                          \
  void* operator new(size_t, base::NotNullTag, void*) = delete; \
  void* operator new(size_t, void*) = delete

// Provides customizable overrides of fastMalloc/fastFree and operator
// new/delete
//
// Provided functionality:
//    Macro: USING_FAST_MALLOC
//
// Example usage:
//    class Widget {
//        USING_FAST_MALLOC(Widget)
//    ...
//    };
//
//    struct Data {
//        USING_FAST_MALLOC(Data)
//    public:
//    ...
//    };
//

// In official builds, do not include type info string literals to avoid
// bloating the binary.
#if defined(OFFICIAL_BUILD)
#define WTF_HEAP_PROFILER_TYPE_NAME(T) nullptr
#else
#define WTF_HEAP_PROFILER_TYPE_NAME(T) ::blink::GetStringWithTypeName<T>()
#endif

// Both of these macros enable fast malloc and provide type info to the heap
// profiler when PartitionAlloc-Everywhere is disabled. When
// PartitionAlloc-Everywhere is enabled, they are no-ops because the global
// operator new/delete already allocate via PartitionAlloc. The regular macro
// does not provide type info in official builds, to avoid bloating the binary
// with type name strings. The |WITH_TYPE_NAME| variant provides type info
// unconditionally, so it should be used sparingly. Furthermore, the
// |WITH_TYPE_NAME| variant does not work if |type| is a template argument;
// |USING_FAST_MALLOC| does.
#define USING_FAST_MALLOC(type) \
  USING_FAST_MALLOC_INTERNAL(type, WTF_HEAP_PROFILER_TYPE_NAME(type))
#define USING_FAST_MALLOC_WITH_TYPE_NAME(type) \
  USING_FAST_MALLOC_INTERNAL(type, #type)

// Compile-time checks emitted by USING_FAST_MALLOC_INTERNAL in both PA-E and
// non-PA-E builds:
// - `BlinkAllocatorMacroTag` conflicts with `DISALLOW_NEW()`'s alias of the
//   same name in the same class, catching `DISALLOW_NEW()` even when placed
//   after `USING_FAST_MALLOC`.
// - `FastMallocCheckTag` is a unique per-class tag type passed to
//   `FastMallocChecker` to prevent incomplete-type template memoization across
//   CRTP bases (e.g. `RefCounted<T>`). Redeclaring it in the same class also
//   rejects duplicate `USING_FAST_MALLOC` invocations.
// - `friend struct ::blink::internal::FastMallocChecker` grants access to
//   marker aliases inherited through private or protected base classes.
#define USING_FAST_MALLOC_TYPE_CHECKS(type)                                  \
 private:                                                                    \
  using BlinkAllocatorMacroTag [[maybe_unused]] =                            \
      ::blink::internal::FastMallocTag;                                      \
  struct FastMallocCheckTag;                                                 \
  friend struct ::blink::internal::FastMallocChecker;                        \
  static_assert(                                                             \
      !::blink::internal::FastMallocChecker::HasGCMarker<                    \
          type, FastMallocCheckTag>(),                                       \
      "USING_FAST_MALLOC cannot be used on a GarbageCollected type");        \
  static_assert(!::blink::internal::FastMallocChecker::HasDisallowNewMarker< \
                    type, FastMallocCheckTag>(),                             \
                "USING_FAST_MALLOC cannot be used on a DISALLOW_NEW type");  \
  static_assert(                                                             \
      !::blink::internal::FastMallocChecker::HasStackAllocatedMarker<        \
          type, FastMallocCheckTag>(),                                       \
      "USING_FAST_MALLOC cannot be used on a STACK_ALLOCATED type")

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// FastMalloc doesn't provide isolation, only a (hopefully fast) malloc(). When
// PartitionAlloc is already the malloc() implementation, there is nothing to
// do; avoiding class-specific operator new/delete overrides also preserves
// C++17 sized deallocation and over-aligned allocation.
//
// Still validate at compile time that the class is not also GarbageCollected,
// GarbageCollectedMixin, DISALLOW_NEW, or STACK_ALLOCATED, since without
// overriding `operator new`, the Blink GC plugin's `ClassOverridesNew` check
// would otherwise not fire on PartitionAlloc-Everywhere builds.
#define USING_FAST_MALLOC_INTERNAL(type, typeName) \
  USING_FAST_MALLOC_TYPE_CHECKS(type);             \
  friend class ::blink::internal::__thisIsHereToForceASemicolonAfterThisMacro
#else
#define USING_FAST_MALLOC_INTERNAL(type, typeName)               \
 public:                                                         \
  void* operator new(size_t, void* p) {                          \
    return p;                                                    \
  }                                                              \
  void* operator new[](size_t, void* p) {                        \
    return p;                                                    \
  }                                                              \
                                                                 \
  void* operator new(size_t size) {                              \
    return ::blink::Partitions::FastMalloc(size, typeName);      \
  }                                                              \
                                                                 \
  void operator delete(void* p) {                                \
    ::blink::Partitions::FastFree(p);                            \
  }                                                              \
                                                                 \
  void* operator new[](size_t size) {                            \
    return ::blink::Partitions::FastMalloc(size, typeName);      \
  }                                                              \
                                                                 \
  void operator delete[](void* p) {                              \
    ::blink::Partitions::FastFree(p);                            \
  }                                                              \
  void* operator new(size_t, base::NotNullTag, void* location) { \
    DCHECK(location);                                            \
    return location;                                             \
  }                                                              \
  USING_FAST_MALLOC_TYPE_CHECKS(type);                           \
  friend class ::blink::internal::__thisIsHereToForceASemicolonAfterThisMacro
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// This version of placement new omits a 0 check.
inline void* operator new(size_t, base::NotNullTag, void* location) {
  DCHECK(location);
  return location;
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_
