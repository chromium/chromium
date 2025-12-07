// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ForwardDeclaredMember<T> allows a class to contain a GC-ed object of type T,
// even though T would not be visible to it during linking (in particular,
// if an object in core/ wants to contain an object in modules/). The cost
// is that the object gains a vtable, which is used for sending Trace()
// through.
//
// T needs to inherit from GarbageCollectedMixin, either directly or through
// another base. (This holds even if it already inherits from
// GarbageCollected<T>, even though the combination is unusual.) The containng
// class can then forward-declare T, contain a ForwardDeclaredMember<T> member
// t_, and use visitor->Trace(t_) as usual. (Trying to have a Member<T> directly
// would lead to a compile error, as Trace() needs to know the size of T and
// including the right header file would be a layering violation.) It can then
// have getters and setters that take and return ForwardDeclaredMember<T>:
//
//  public:
//   ForwardDeclaredMember<Foo> GetFoo() const {
//     return foo_;
//   }
//   void SetFoo(ForwardDeclaredMember<Foo> foo) {
//     foo_ = foo;
//   }
//   void Trace(Visitor* visitor) const override {
//     visitor->Trace(foo_);
//   }
//
//  private:
//   ForwardDeclaredMember<Foo> foo_;
//
// The caller can use GetFoo() as if it were returning a Foo* (and similar for
// SetFoo()), since there are implicit conversions on both sides. However,
// using these implicit conversions from within the class itself would not work,
// as they need access to the definition of the class. Foo thus remains entirely
// opaque to its container.
//
// Diamond inheritance causes some problems for our casting; if you inherit
// from two or more classes that in turn inherit from GarbageCollectedMixin,
// the compiler will not known which GarbageCollectedMixin you meant. In this
// case, you'll need to choose one of the bases arbitrarily and give that
// as the second template argument (“Via”).
//
// If you do not have layering violation problems, you do not need
// ForwardDeclaredMember<>. Just forward-declare as usual and #include its
// header file in your .cc file (where you also implement Trace()).

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FORWARD_DECLARED_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FORWARD_DECLARED_MEMBER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

template <class T, class Via = T>
class ForwardDeclaredMember {
  DISALLOW_NEW();

 public:
  // These can be used freely from any caller, without seeing the entire
  // definition of T.

  ForwardDeclaredMember() = default;
  explicit ForwardDeclaredMember(nullptr_t) : obj_(nullptr) {}
  ForwardDeclaredMember(const ForwardDeclaredMember&) = default;
  ForwardDeclaredMember(ForwardDeclaredMember&&) = default;
  ForwardDeclaredMember& operator=(const ForwardDeclaredMember&) = default;
  ForwardDeclaredMember& operator=(ForwardDeclaredMember&&) = default;
  ForwardDeclaredMember& operator=(nullptr_t) {
    obj_ = nullptr;
    return *this;
  }
  void Trace(Visitor* visitor) const { visitor->Trace(obj_); }

  // These are only usable by those who have the full definition of T,
  // since static_cast<> needs to know the inheritance tree to adjust
  // the pointer correctly.

  // NOLINTNEXTLINE
  ForwardDeclaredMember(T* obj) : obj_(static_cast<Via*>(obj)) {}
  // NOLINTNEXTLINE
  operator T*() const { return static_cast<T*>(static_cast<Via*>(obj_.Get())); }
  ForwardDeclaredMember& operator=(T* obj) {
    obj_ = static_cast<GarbageCollectedMixin*>(static_cast<Via*>(obj));
    return *this;
  }

 private:
  Member<GarbageCollectedMixin> obj_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FORWARD_DECLARED_MEMBER_H_
