// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_

namespace WTF {

// `ConditionalDestructor` defines the destructor of the derived object.
// This base is used in order to completely avoid creating a destructor
// for an object that does not need to be destructed. By doing so,
// the clang compiler will have correct information about whether or not
// the object has a trivial destructor.
template <typename Derived, bool needsDestructor>
class ConditionalDestructor;

template <typename Derived>
class ConditionalDestructor<Derived, true> {
 public:
  ~ConditionalDestructor() { static_cast<Derived*>(this)->Finalize(); }
};

template <typename Derived>
class ConditionalDestructor<Derived, false> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_
