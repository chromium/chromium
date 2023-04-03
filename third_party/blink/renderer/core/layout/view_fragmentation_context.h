// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_

#include "third_party/blink/renderer/core/layout/fragmentation_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LayoutView;

class ViewFragmentationContext final
    : public GarbageCollected<ViewFragmentationContext>,
      public FragmentationContext {
 public:
  explicit ViewFragmentationContext(LayoutView& view) : view_(&view) {}
  void Trace(Visitor*) const override;

 private:
  Member<LayoutView> view_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_
