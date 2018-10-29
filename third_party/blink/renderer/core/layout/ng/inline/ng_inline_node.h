// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineNode_h
#define NGInlineNode_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NGConstraintSpace;
class NGInlineBreakToken;
class NGInlineChildLayoutContext;
class NGInlineItem;
class NGLayoutResult;
class NGOffsetMapping;
class NGInlineNodeLegacy;
struct MinMaxSize;
struct NGInlineItemsData;

// Represents an anonymous block box to be laid out, that contains consecutive
// inline nodes and their descendants.
class CORE_EXPORT NGInlineNode : public NGLayoutInputNode {
 public:
  NGInlineNode(LayoutBlockFlow*);

  LayoutBlockFlow* GetLayoutBlockFlow() const {
    return ToLayoutBlockFlow(box_);
  }
  NGLayoutInputNode NextSibling() { return nullptr; }

  // True in quirks mode or limited-quirks mode, which require line-height
  // quirks.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  bool InLineHeightQuirksMode() const {
    return GetDocument().InLineHeightQuirksMode();
  }

  scoped_refptr<NGLayoutResult> Layout(const NGConstraintSpace&,
                                       const NGBreakToken*,
                                       NGInlineChildLayoutContext* context);

  // Computes the value of min-content and max-content for this anonymous block
  // box. min-content is the inline size when lines wrap at every break
  // opportunity, and max-content is when lines do not wrap at all.
  MinMaxSize ComputeMinMaxSize(WritingMode container_writing_mode,
                               const MinMaxSizeInput&,
                               const NGConstraintSpace* = nullptr);

  // Instruct to re-compute |PrepareLayout| on the next layout.
  void InvalidatePrepareLayoutForTest() {
    GetLayoutBlockFlow()->ResetNGInlineNodeData();
    DCHECK(!IsPrepareLayoutFinished());
  }

  const NGInlineItemsData& ItemsData(bool is_first_line) const {
    return Data().ItemsData(is_first_line);
  }

  // Returns the DOM to text content offset mapping of this block. If it is not
  // computed before, compute and store it in NGInlineNodeData.
  // This funciton must be called with clean layout.
  const NGOffsetMapping* ComputeOffsetMappingIfNeeded();

  bool IsBidiEnabled() const { return Data().is_bidi_enabled_; }
  TextDirection BaseDirection() const { return Data().BaseDirection(); }

  bool IsEmptyInline() { return EnsureData().is_empty_inline_; }

  // @return if this node can contain the "first formatted line".
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool CanContainFirstFormattedLine() const {
    DCHECK(GetLayoutBlockFlow());
    return GetLayoutBlockFlow()->CanContainFirstFormattedLine();
  }

  void CheckConsistency() const;

  String ToString() const;

 protected:
  bool IsPrepareLayoutFinished() const;

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayoutIfNeeded();

  void CollectInlines(NGInlineNodeData*,
                      NGInlineNodeData* previous_data = nullptr);
  void SegmentText(NGInlineNodeData*);
  void SegmentScriptRuns(NGInlineNodeData*);
  void SegmentFontOrientation(NGInlineNodeData*);
  void SegmentBidiRuns(NGInlineNodeData*);
  void ShapeText(NGInlineItemsData*,
                 NGInlineItemsData* previous_data = nullptr);
  void ShapeText(const String& text,
                 Vector<NGInlineItem>*,
                 const String* previous_text);
  void ShapeTextForFirstLineIfNeeded(NGInlineNodeData*);
  void AssociateItemsWithInlines(NGInlineNodeData*);

  void ClearAssociatedFragments(const NGInlineBreakToken*);

  NGInlineNodeData* MutableData() {
    return ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
  }
  const NGInlineNodeData& Data() const {
    DCHECK(IsPrepareLayoutFinished() &&
           !GetLayoutBlockFlow()->NeedsCollectInlines());
    return *ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
  }
  const NGInlineNodeData& EnsureData();

  friend class NGLineBreakerTest;
  friend class NGInlineNodeLegacy;
};

DEFINE_TYPE_CASTS(NGInlineNode,
                  NGLayoutInputNode,
                  node,
                  node->IsInline(),
                  node.IsInline());

}  // namespace blink

#endif  // NGInlineNode_h
