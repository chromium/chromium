// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_

#include "v8/include/cppgc/member.h"

namespace blink {

template <typename T>
using Member = cppgc::Member<T>;

template <typename T>
using WeakMember = cppgc::WeakMember<T>;

template <typename T>
using UntracedMember = cppgc::UntracedMember<T>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_
