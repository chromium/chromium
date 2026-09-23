// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_

#include <type_traits>

#include "gin/public/wrappable_pointer_tags.h"
#include "v8/include/v8-sandbox.h"

namespace blink {

// Pointer tags for Blink CppHeap objects that do NOT inherit from
// v8::Object::Wrappable (nor blink::ScriptWrappable).
enum class CppHeapExternalTag : std::underlying_type_t<v8::CppHeapPointerTag> {
  kFirst = static_cast<std::underlying_type_t<v8::CppHeapPointerTag>>(
      gin::kBlinkNonWrappableTagRange.first),
  kTaskAttributionTaskStateTag = kFirst,
  kEventLoopMicrotaskWrapperTag,
  kScriptStateTag,

  kLastTag = kScriptStateTag
};

static_assert(
    gin::kBlinkNonWrappableTagRange.Contains(
        static_cast<v8::CppHeapPointerTag>(CppHeapExternalTag::kLastTag)),
    "CppHeapExternalTag must be within kBlinkNonWrappableTagRange");

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CPP_HEAP_EXTERNAL_TAG_H_
