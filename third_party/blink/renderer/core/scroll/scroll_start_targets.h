// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_START_TARGETS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_START_TARGETS_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class LayoutBox;

struct ScrollStartTargetCandidates
    : public GarbageCollected<ScrollStartTargetCandidates> {
  void Trace(Visitor* visitor) const;
  Member<const LayoutBox> x;
  Member<const LayoutBox> y;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_START_TARGETS_H_
