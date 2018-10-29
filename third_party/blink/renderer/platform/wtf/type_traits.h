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
#include "base/template_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template <typename T>
inline const char* GetStringWithTypeName() {
  return WTF_PRETTY_FUNCTION;
}

template <typename T>
struct IsWeak {
  static const bool value = false;
};

enum WeakHandlingFlag {
  kNoWeakHandling,
  kWeakHandling,
};

template <typename T>
struct IsTriviallyDestructible {
  // TODO(slangley): crbug.com/783060 - std::is_trivially_destructible behaves
  // differently on across platforms.
  static constexpr bool value =
      __has_trivial_destructor(T) && std::is_destructible<T>::value;
};

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

template <typename T>
class IsTraceable {
  typedef char YesType;
  typedef struct NoType { char padding[8]; } NoType;

  // Note that this also checks if a superclass of V has a trace method.
  template <typename V>
  static YesType CheckHasTraceMethod(
      V* v,
      blink::Visitor* p = nullptr,
      typename std::enable_if<
          std::is_same<decltype(v->Trace(p)), void>::value>::type* g = nullptr);
  template <typename V>
  static NoType CheckHasTraceMethod(...);

 public:
  // We add sizeof(T) to both sides here, because we want it to fail for
  // incomplete types. Otherwise it just assumes that incomplete types do not
  // have a trace method, which may not be true.
  static const bool value = sizeof(YesType) + sizeof(T) ==
                            sizeof(CheckHasTraceMethod<T>(nullptr)) + sizeof(T);
};

// Convenience template wrapping the IsTraceableInCollection template in
// Collection Traits. It helps make the code more readable.
template <typename Traits>
class IsTraceableInCollectionTrait {
 public:
  static const bool value = Traits::template IsTraceableInCollection<>::value;
};

template <typename T, typename U>
struct IsTraceable<std::pair<T, U>> {
  static const bool value = IsTraceable<T>::value || IsTraceable<U>::value;
};

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
class IsGarbageCollectedType {
  typedef char YesType;
  typedef struct NoType { char padding[8]; } NoType;

  static_assert(sizeof(T), "T must be fully defined");

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
  //        USING_GARBAGE_COLLECTED_MIXIN(A);
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

template <>
class IsGarbageCollectedType<void> {
 public:
  static const bool value = false;
};

template <typename T>
class IsPersistentReferenceType {
  typedef char YesType;
  typedef struct NoType { char padding[8]; } NoType;

  template <typename U>
  static YesType CheckPersistentReferenceType(
      typename U::IsPersistentReferenceTypeMarker*);
  template <typename U>
  static NoType CheckPersistentReferenceType(...);

 public:
  static const bool value =
      (sizeof(YesType) == sizeof(CheckPersistentReferenceType<T>(nullptr)));
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
