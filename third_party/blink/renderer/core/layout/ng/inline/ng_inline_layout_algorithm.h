// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGConstraintSpace;
class NGExclusionSpace;
class NGInlineBreakToken;
class NGInlineChildLayoutContext;
class NGInlineNode;
class NGInlineItem;
class NGInlineLayoutStateStack;
class NGLineInfo;
struct NGInlineBoxState;
struct NGInlineItemResult;

// A class for laying out an inline formatting context, i.e. a block with inline
// children.
//
// This class determines the position of NGInlineItem and build line boxes.
//
// Uses NGLineBreaker to find NGInlineItems to form a line.
class CORE_EXPORT NGInlineLayoutAlgorithm final
    : public NGLayoutAlgorithm<NGInlineNode,
                               NGLineBoxFragmentBuilder,
                               NGInlineBreakToken> {
 public:
  NGInlineLayoutAlgorithm(NGInlineNode,
                          const NGConstraintSpace&,
                          const NGInlineBreakToken*,
                          NGInlineChildLayoutContext* context);
  ~NGInlineLayoutAlgorithm() override;

  void CreateLine(const NGLineLayoutOpportunity&,
                  NGLineInfo*,
                  NGLogicalLineItems* line_box,
                  NGExclusionSpace*);

  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override {
    NOTREACHED();
    return MinMaxSizesResult();
  }

 private:
  unsigned PositionLeadingFloats(NGExclusionSpace*, NGPositionedFloatVector*);
  NGPositionedFloat PositionFloat(LayoutUnit origin_block_bfc_offset,
                                  LayoutObject* floating_object,
                                  NGExclusionSpace*) const;

  void PrepareBoxStates(const NGLineInfo&, const NGInlineBreakToken*);
  void RebuildBoxStates(const NGLineInfo&,
                        const NGInlineBreakToken*,
                        NGInlineLayoutStateStack*) const;
#if DCHECK_IS_ON()
  void CheckBoxStates(const NGLineInfo&, const NGInlineBreakToken*) const;
#endif

  NGInlineBoxState* HandleOpenTag(const NGInlineItem&,
                                  const NGInlineItemResult&,
                                  NGLogicalLineItems*,
                                  NGInlineLayoutStateStack*) const;
  NGInlineBoxState* HandleCloseTag(const NGInlineItem&,
                                   const NGInlineItemResult&,
                                   NGLogicalLineItems* line_box,
                                   NGInlineBoxState*);

  void BidiReorder(TextDirection base_direction, NGLogicalLineItems* line_box);

  void PlaceControlItem(const NGInlineItem&,
                        const NGLineInfo&,
                        NGInlineItemResult*,
                        NGLogicalLineItems* line_box,
                        NGInlineBoxState*);
  void PlaceHyphen(const NGInlineItemResult&,
                   LayoutUnit hyphen_inline_size,
                   NGLogicalLineItems* line_box,
                   NGInlineBoxState*);
  NGInlineBoxState* PlaceAtomicInline(const NGInlineItem&,
                                      const NGLineInfo&,
                                      NGInlineItemResult*,
                                      NGLogicalLineItems* line_box);
  void PlaceLayoutResult(NGInlineItemResult*,
                         NGLogicalLineItems* line_box,
                         NGInlineBoxState*,
                         LayoutUnit inline_offset = LayoutUnit());
  void PlaceOutOfFlowObjects(const NGLineInfo&,
                             const FontHeight&,
                             NGLogicalLineItems* line_box);
  void PlaceFloatingObjects(const NGLineInfo&,
                            const FontHeight&,
                            const NGLineLayoutOpportunity&,
                            NGLogicalLineItems* line_box,
                            NGExclusionSpace*);
  void PlaceRelativePositionedItems(NGLogicalLineItems* line_box);
  void PlaceListMarker(const NGInlineItem&,
                       NGInlineItemResult*,
                       const NGLineInfo&);

  LayoutUnit ApplyTextAlign(NGLineInfo*);
  base::Optional<LayoutUnit> ApplyJustify(LayoutUnit space, NGLineInfo*);

  LayoutUnit ComputeContentSize(const NGLineInfo&,
                                const NGExclusionSpace&,
                                LayoutUnit line_height);

  NGInlineLayoutStateStack* box_states_;
  NGInlineChildLayoutContext* context_;

  FontBaseline baseline_type_ = FontBaseline::kAlphabeticBaseline;

  // True if in quirks or limited-quirks mode, which require line-height quirks.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  unsigned quirks_mode_ : 1;

#if DCHECK_IS_ON()
  // True if |box_states_| is taken from |context_|, to check the |box_states_|
  // is the same as when it is rebuilt.
  bool is_box_states_from_context_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_LAYOUT_ALGORITHM_H_
