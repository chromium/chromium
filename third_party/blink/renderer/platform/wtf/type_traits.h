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
#include "build/build_config.h"
#include "v8/include/cppgc/type-traits.h"  // nogncheck

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template <typename T>
inline const char* GetStringWithTypeName() {
  return PRETTY_FUNCTION;
}

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

template <typename T>
struct IsTraceable : cppgc::internal::IsTraceable<T> {};

template <typename T>
struct IsGarbageCollectedType
    : cppgc::internal::IsGarbageCollectedOrMixinType<T> {};

template <typename T>
struct IsWeak : cppgc::internal::IsWeak<T> {};

template <typename T>
struct IsMemberType : std::bool_constant<cppgc::IsMemberTypeV<T>> {};

template <typename T>
struct IsWeakMemberType : std::bool_constant<cppgc::IsWeakMemberTypeV<T>> {};

template <typename T>
struct IsMemberOrWeakMemberType
    : std::bool_constant<cppgc::IsMemberTypeV<T> ||
                         cppgc::IsWeakMemberTypeV<T>> {};

template <typename T>
struct IsAnyMemberType
    : std::bool_constant<IsMemberOrWeakMemberType<T>::value ||
                         cppgc::IsUntracedMemberTypeV<T>> {};

template <typename T, typename U>
struct IsTraceable<std::pair<T, U>>
    : std::bool_constant<IsTraceable<T>::value || IsTraceable<U>::value> {};

enum WeakHandlingFlag {
  kNoWeakHandling,
  kWeakHandling,
};

// This is for tracing inside collections that have special support for weak
// pointers.
//
// Structure:
// - `Trace()`: Traces the contents.
// - `IsAlive()`: Returns true if the contents are still considered alive, and
// false otherwise.
//
// Default implementation for non-weak types is to use the regular non-weak
// TraceTrait. Default implementation for types with weakness is to
// delegate to sub types until reaching WeakMember or KeyValuePair which
// have defined weakness semantics.
template <WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceInCollectionTrait;

template <typename T>
inline constexpr WeakHandlingFlag kWeakHandlingTrait =
    IsWeak<T>::value ? kWeakHandling : kNoWeakHandling;

// This is used to check that DISALLOW_NEW objects are not
// stored in off-heap Vectors, HashTables etc.
template <typename T>
concept IsDisallowNew = requires { typename T::IsDisallowNewMarker; };

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

template <typename T>
concept IsStackAllocatedType =
    requires { typename T::IsStackAllocatedTypeMarker; };

template <typename T>
struct IsPointerToGced {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename X,
            typename = std::enable_if_t<WTF::IsGarbageCollectedType<X>::value>>
  static YesType SubclassCheck(X**);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

}  // namespace WTF

using WTF::IsGarbageCollectedType;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_
