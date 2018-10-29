// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

scoped_refptr<const NGPhysicalLineBoxFragment>
NGPhysicalLineBoxFragment::Create(NGLineBoxFragmentBuilder* builder) {
  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      sizeof(NGPhysicalLineBoxFragment) +
          builder->children_.size() * sizeof(NGLinkStorage),
      ::WTF::GetStringWithTypeName<NGPhysicalLineBoxFragment>());
  new (data) NGPhysicalLineBoxFragment(builder);
  return base::AdoptRef(static_cast<NGPhysicalLineBoxFragment*>(data));
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    NGLineBoxFragmentBuilder* builder)
    : NGPhysicalContainerFragment(builder,
                                  builder->GetWritingMode(),
                                  children_,
                                  kFragmentLineBox,
                                  0),
      metrics_(builder->metrics_) {
  base_direction_ = static_cast<unsigned>(builder->base_direction_);
}

NGLineHeightMetrics NGPhysicalLineBoxFragment::BaselineMetrics(
    FontBaseline) const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

NGPhysicalOffsetRect NGPhysicalLineBoxFragment::InkOverflow() const {
  return ContentsInkOverflow();
}

NGPhysicalOffsetRect NGPhysicalLineBoxFragment::ContentsInkOverflow() const {
  // Cannot be cached, because children might change their self-painting flag.
  NGPhysicalOffsetRect overflow({}, Size());
  for (const auto& child : Children()) {
    child->PropagateContentsInkOverflow(&overflow, child.Offset());
  }
  return overflow;
}

NGPhysicalOffsetRect NGPhysicalLineBoxFragment::ScrollableOverflow(
    const ComputedStyle* container_style,
    NGPhysicalSize container_physical_size) const {
  WritingMode container_writing_mode = container_style->GetWritingMode();
  TextDirection container_direction = container_style->Direction();
  NGPhysicalOffsetRect overflow({}, Size());
  for (const auto& child : Children()) {
    NGPhysicalOffsetRect child_scroll_overflow = child->ScrollableOverflow();
    child_scroll_overflow.offset += child.Offset();
    // If child has the same style as parent, parent will compute relative
    // offset.
    if (&child->Style() != container_style) {
      child_scroll_overflow.offset +=
          ComputeRelativeOffset(child->Style(), container_writing_mode,
                                container_direction, container_physical_size);
    }
    overflow.Unite(child_scroll_overflow);
  }
  return overflow;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::FirstLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (runner->IsContainer() && !runner->IsBlockFormattingContextRoot()) {
    const NGPhysicalContainerFragment* runner_as_container =
        ToNGPhysicalContainerFragment(runner);
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().front().get()
                 : runner_as_container->Children().back().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::LastLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (runner->IsContainer() && !runner->IsBlockFormattingContextRoot()) {
    const NGPhysicalContainerFragment* runner_as_container =
        ToNGPhysicalContainerFragment(runner);
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().back().get()
                 : runner_as_container->Children().front().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  DCHECK(BreakToken());
  DCHECK(BreakToken()->IsInlineType());
  const NGInlineBreakToken& break_token = ToNGInlineBreakToken(*BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

}  // namespace blink
