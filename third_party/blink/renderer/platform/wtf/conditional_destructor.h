// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace WTF {

// ConditionalDestructor defines the destructor of the derived object.
// This base is used in order to completely avoid creating a destructor
// for an object that does not need to be destructed. By doing so,
// the clang compiler will have correct information about whether or not
// the object has a trivial destructor.
// Note: the derived object MUST release all its resources at the finalize()
// method.
template <typename Derived, bool noDestructor>
class ConditionalDestructor {
  USING_FAST_MALLOC(ConditionalDestructor);

 public:
  ~ConditionalDestructor() { static_cast<Derived*>(this)->Finalize(); }
};

template <typename Derived>
class ConditionalDestructor<Derived, true> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONDITIONAL_DESTRUCTOR_H_
