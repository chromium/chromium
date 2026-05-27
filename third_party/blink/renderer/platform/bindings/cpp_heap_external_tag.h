// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_

#include <type_traits>

#include "v8/include/v8-sandbox.h"

namespace blink {

enum class CppHeapExternalTag : std::underlying_type_t<v8::CppHeapPointerTag> {
  kFirst = 1,
  kTaskAttributionTaskStateTag = kFirst,
  kEventLoopMicrotaskWrapperTag,

  kLastTag = kEventLoopMicrotaskWrapperTag
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_
