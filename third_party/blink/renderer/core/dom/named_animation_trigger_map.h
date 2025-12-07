// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NAMED_ANIMATION_TRIGGER_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NAMED_ANIMATION_TRIGGER_MAP_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class AnimationTrigger;
class ScopedCSSName;

// This maps a named animation trigger to the corresponding trigger object
// within.
using NamedAnimationTriggerMap =
    HeapHashMap<Member<const ScopedCSSName>, Member<AnimationTrigger>>;

using GCedNamedAnimationTriggerMap =
    GCedHeapHashMap<Member<const ScopedCSSName>, Member<AnimationTrigger>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NAMED_ANIMATION_TRIGGER_MAP_H_
