// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_

#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/type-traits.h"

namespace blink {

using GarbageCollectedMixin = cppgc::GarbageCollectedMixin;

template <typename T>
struct IsGarbageCollectedMixin {
 public:
  static const bool value = cppgc::IsGarbageCollectedMixinTypeV<T>;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GARBAGE_COLLECTED_H_
