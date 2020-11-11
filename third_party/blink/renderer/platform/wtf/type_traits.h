/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_

#include <cstddef>
#include <type_traits>
#include <utility>
#include "base/compiler_specific.h"
#include "base/template_util.h"
#include "build/build_config.h"

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template <typename T>
inline const char* GetStringWithTypeName() {
  return PRETTY_FUNCTION;
}

// Specifies whether a type should be treated weakly by the memory management
// system. Only supported by the garbage collector and not by PartitionAlloc.
// Requires garbage collection support, so it is only safe to  override in sync
// with changing garbage collection semantics.
template <typename T>
struct IsWeak : std::false_type {};

enum WeakHandlingFlag {
  kNoWeakHandling,
  kWeakHandling,
};

template <typename T>
struct WeakHandlingTrait
    : std::integral_constant<WeakHandlingFlag,
                             IsWeak<T>::value ? kWeakHandling
                                              : kNoWeakHandling> {};

template <typename T, typename U>
struct IsSubclass {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  static YesType SubclassCheck(U*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename... V> class U>
struct IsSubclassOfTemplate {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename... W>
  static YesType SubclassCheck(U<W...>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W> class U>
struct IsSubclassOfTemplateTypenameSize {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename X, size_t Y>
  static YesType SubclassCheck(U<X, Y>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W, typename X> class U>
struct IsSubclassOfTemplateTypenameSizeTypename {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename Y, size_t Z, typename A>
  static YesType SubclassCheck(U<Y, Z, A>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

}  // namespace WTF

namespace blink {

class Visitor;

}  // namespace blink

namespace WTF {

namespace internal {
// IsTraceMethodConst is used to verify that all Trace methods are marked as
// const. It is equivalent to IsTraceable but for a non-const object.
template <typename T, typename = void>
struct IsTraceMethodConst : std::false_type {};

template <typename T>
struct IsTraceMethodConst<T,
                          base::void_t<decltype(std::declval<const T>().Trace(
                              std::declval<blink::Visitor*>()))>>
    : std::true_type {};
}  // namespace internal

template <typename T, typename = void>
struct IsTraceable : std::false_type {
  // Fail on incomplete types.
  static_assert(sizeof(T), "incomplete type T");
};

// Note: This also checks if a superclass of T has a trace method.
template <typename T>
struct IsTraceable<T,
                   base::void_t<decltype(std::declval<T>().Trace(
                       std::declval<blink::Visitor*>()))>> : std::true_type {
  // All Trace methods should be marked as const. If an object of type
  // 'T' is traceable then any object of type 'const T' should also
  // be traceable.
  static_assert(internal::IsTraceMethodConst<T>(),
                "Trace methods should be marked as const.");
};

template <typename T, typename U>
struct IsTraceable<std::pair<T, U>>
    : std::integral_constant<bool,
                             IsTraceable<T>::value || IsTraceable<U>::value> {};

// Convenience template wrapping the IsTraceableInCollection template in
// Collection Traits. It helps make the code more readable.
template <typename Traits>
struct IsTraceableInCollectionTrait
    : std::integral_constant<
          bool,
          Traits::template IsTraceableInCollection<>::value> {};

// This is used to check that DISALLOW_NEW objects are not
// stored in off-heap Vectors, HashTables etc.
template <typename T>
struct IsDisallowNew {
 private:
  using YesType = char;
  struct NoType {
    char padding[8];
  };

  template <typename U>
  static YesType CheckMarker(typename U::IsDisallowNewMarker*);
  template <typename U>
  static NoType CheckMarker(...);

 public:
  static const bool value = sizeof(CheckMarker<T>(nullptr)) == sizeof(YesType);
};

template <typename T>
class IsGarbageCollectedTypeInternal {
  typedef char YesType;
  typedef struct NoType { char padding[8]; } NoType;

  using NonConstType = typename std::remove_const<T>::type;
  template <typename U>
  static YesType CheckGarbageCollectedType(
      typename U::IsGarbageCollectedTypeMarker*);
  template <typename U>
  static NoType CheckGarbageCollectedType(...);

  // Separately check for GarbageCollectedMixin, which declares a different
  // marker typedef, to avoid resolution ambiguity for cases like
  // IsGarbageCollectedType<B> over:
  //
  //    class A : public GarbageCollected<A>, public GarbageCollectedMixin {
  //        ...
  //    };
  //    class B : public A, public GarbageCollectedMixin { ... };
  //
  template <typename U>
  static YesType CheckGarbageCollectedMixinType(
      typename U::IsGarbageCollectedMixinMarker*);
  template <typename U>
  static NoType CheckGarbageCollectedMixinType(...);

 public:
  static const bool value =
      (sizeof(YesType) ==
       sizeof(CheckGarbageCollectedType<NonConstType>(nullptr))) ||
      (sizeof(YesType) ==
       sizeof(CheckGarbageCollectedMixinType<NonConstType>(nullptr)));
};

template <typename T>
class IsGarbageCollectedType : public IsGarbageCollectedTypeInternal<T> {
  static_assert(sizeof(T), "T must be fully defined");
};

template <>
class IsGarbageCollectedType<void> {
 public:
  static const bool value = false;
};

template <typename T,
          bool = std::is_function<typename std::remove_const<
                     typename std::remove_pointer<T>::type>::type>::value ||
                 std::is_void<typename std::remove_const<
                     typename std::remove_pointer<T>::type>::type>::value>
class IsPointerToGarbageCollectedType {
 public:
  static const bool value = false;
};

template <typename T>
class IsPointerToGarbageCollectedType<T*, false> {
 public:
  static const bool value = IsGarbageCollectedType<T>::value;
};

template <typename T, typename = void>
struct IsStackAllocatedType : std::false_type {};

template <typename T>
struct IsStackAllocatedType<
    T,
    base::void_t<typename T::IsStackAllocatedTypeMarker>> : std::true_type {};

}  // namespace WTF

using WTF::IsGarbageCollectedType;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_
