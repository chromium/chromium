// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_

#include "third_party/blink/renderer/core/layout/fragmentation_context.h"

namespace blink {

class LayoutView;

class ViewFragmentationContext final : public FragmentationContext {
 public:
  explicit ViewFragmentationContext(LayoutView& view) : view_(&view) {}
  bool IsFragmentainerLogicalHeightKnown() final;
  LayoutUnit FragmentainerLogicalHeightAt(LayoutUnit block_offset) final;
  LayoutUnit RemainingLogicalHeightAt(LayoutUnit block_offset) final;

 private:
  LayoutView* const view_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_VIEW_FRAGMENTATION_CONTEXT_H_
