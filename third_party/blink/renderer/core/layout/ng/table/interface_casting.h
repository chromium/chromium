// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_INTERFACE_CASTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_INTERFACE_CASTING_H_

#include "base/notreached.h"

namespace blink {

// These are the helpers for downcasting to mixin classes.
//
// Given the following class hierarchy:
//
// class BaseClass;
// class MixinClass;
// class DerivedOne : public BaseClass, public MixinClass;
// class DerivedTwo : public BaseClass, public MixinClass;
//
// There is no way to convert BaseClass to MixingClass by c++ casting.
//
// Instead, we must declare a casting helper methods on BaseClass:
//
// class BaseClass {
//   virtual const MixinClass* ToMixin() const {
//     DCHECK(false);
//     return nullptr;
//   }
// }
//
// and override them in derived classes:
//
// class DerivedOne : public BaseClass, public MixinClass {
//   const MixinClass* ToMixin() const override {
//     return this;
//   }
// }
//
// These templates try to make mixin casting feel similar to downcasting
// primitives from wtf/casting.h
//
// Mixin casting is implemented by specializing InterfaceDowncastTraits:
//
// struct InterfaceDowncastTraits<MixingClass> {
//   static bool AllowFrom(const BaseClass& b) {
//     return b.IsDerivedClassWithMixin();
//   }
//   static const MixingClass& ConvertFrom(const BaseClass& b) {
//     return b.ToMixin();
//   }
// };

template <typename T>
struct InterfaceDowncastTraits {
  template <typename U>
  static bool AllowFrom(const U&) {
    static_assert(sizeof(U) == 0, "no downcast traits specialization for T");
    NOTREACHED();
    return false;
  }
  template <typename U>
  static const T& ConvertFrom(const U&) {
    static_assert(sizeof(U) == 0, "no downcast traits specialization for T");
  }
};

template <typename Derived, typename Base>
const Derived& ToInterface(const Base& from) {
  SECURITY_DCHECK(InterfaceDowncastTraits<Derived>::AllowFrom(from));
  return InterfaceDowncastTraits<Derived>::ConvertFrom(from);
}

template <typename Derived, typename Base>
const Derived* ToInterface(const Base* from) {
  if (from)
    SECURITY_DCHECK(InterfaceDowncastTraits<Derived>::AllowFrom(*from));
  return from ? &InterfaceDowncastTraits<Derived>::ConvertFrom(*from) : nullptr;
}

template <typename Derived, typename Base>
Derived& ToInterface(Base& from) {
  SECURITY_DCHECK(InterfaceDowncastTraits<Derived>::AllowFrom(from));
  // const_cast is safe because from is not const.
  return const_cast<Derived&>(
      InterfaceDowncastTraits<Derived>::ConvertFrom(*from));
}

template <typename Derived, typename Base>
Derived* ToInterface(Base* from) {
  if (from)
    SECURITY_DCHECK(InterfaceDowncastTraits<Derived>::AllowFrom(*from));
  // const_cast is safe because from is not const.
  return from ? const_cast<Derived*>(
                    &InterfaceDowncastTraits<Derived>::ConvertFrom(*from))
              : nullptr;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_INTERFACE_CASTING_H_
