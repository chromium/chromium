// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ComputedStyle;
class Document;
class LayoutObject;
class LayoutBox;
struct MinMaxSizes;
struct PhysicalSize;

// The input to the min/max inline size calculation algorithm for child nodes.
// Child nodes within the same formatting context need to know which floats are
// beside them.
struct MinMaxSizesFloatInput {
  explicit MinMaxSizesFloatInput() = default;
  LayoutUnit float_left_inline_size;
  LayoutUnit float_right_inline_size;
};

// The output of the min/max inline size calculation algorithm. Contains the
// min/max sizes, and if this calculation will change if the block constraints
// change.
struct MinMaxSizesResult {
  MinMaxSizesResult() = default;
  MinMaxSizesResult(MinMaxSizes sizes, bool depends_on_block_constraints)
      : sizes(sizes),
        depends_on_block_constraints(depends_on_block_constraints) {}

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;
};

// Represents the input to a layout algorithm for a given node. The layout
// engine should use the style, node type to determine which type of layout
// algorithm to use to produce fragments for this node.
class CORE_EXPORT NGLayoutInputNode {
  DISALLOW_NEW();

 public:
  enum NGLayoutInputNodeType {
    kBlock,
    kInline
    // When adding new values, ensure type_ below has enough bits.
  };

  static NGLayoutInputNode Create(LayoutBox* box, NGLayoutInputNodeType type) {
    // This function should create an instance of the subclass. This works
    // because subclasses are not virtual and do not add fields.
    return NGLayoutInputNode(box, type);
  }

  NGLayoutInputNode(std::nullptr_t) : box_(nullptr), type_(kBlock) {}

  NGLayoutInputNodeType Type() const {
    return static_cast<NGLayoutInputNodeType>(type_);
  }
  bool IsInline() const { return type_ == kInline; }
  bool IsBlock() const { return type_ == kBlock; }

  bool IsBlockFlow() const { return IsBlock() && box_->IsLayoutBlockFlow(); }
  bool IsBlockInInline() const { return box_->IsBlockInInline(); }
  bool IsLayoutNGCustom() const {
    return IsBlock() && box_->IsLayoutNGCustom();
  }
  bool IsColumnSpanAll() const { return IsBlock() && box_->IsColumnSpanAll(); }
  bool IsFloating() const { return IsBlock() && box_->IsFloating(); }
  bool IsOutOfFlowPositioned() const {
    return IsBlock() && box_->IsOutOfFlowPositioned();
  }
  bool IsFloatingOrOutOfFlowPositioned() const {
    return IsFloating() || IsOutOfFlowPositioned();
  }
  bool IsReplaced() const { return box_->IsLayoutReplaced(); }
  bool IsAbsoluteContainer() const {
    return box_->CanContainAbsolutePositionObjects();
  }
  bool IsFixedContainer() const {
    return box_->CanContainFixedPositionObjects();
  }
  bool IsBody() const { return IsBlock() && box_->IsBody(); }
  bool IsView() const { return IsBlock() && box_->IsLayoutNGView(); }
  bool IsDocumentElement() const { return box_->IsDocumentElement(); }
  bool IsFlexItem() const { return IsBlock() && box_->IsFlexItemIncludingNG(); }
  bool IsFlexibleBox() const {
    return IsBlock() && box_->IsFlexibleBoxIncludingNG();
  }
  bool IsGrid() const { return IsBlock() && box_->IsLayoutGridIncludingNG(); }
  bool ShouldBeConsideredAsReplaced() const {
    return box_->ShouldBeConsideredAsReplaced();
  }
  bool IsListItem() const { return IsBlock() && box_->IsLayoutNGListItem(); }
  // Returns the list marker if |this.IsListItem()| with an outside list marker.
  // Otherwise |nullptr|.
  NGBlockNode ListMarkerBlockNodeIfListItem() const;
  bool IsListMarker() const {
    return IsBlock() && box_->IsLayoutNGOutsideListMarker();
  }
  bool ListMarkerOccupiesWholeLine() const {
    DCHECK(IsListMarker());
    return To<LayoutNGOutsideListMarker>(box_.Get())->NeedsOccupyWholeLine();
  }
  bool IsButton() const { return IsBlock() && box_->IsLayoutNGButton(); }
  bool IsFieldsetContainer() const {
    return IsBlock() && box_->IsLayoutNGFieldset();
  }
  bool IsInitialLetterBox() const { return box_->IsInitialLetterBox(); }
  bool IsMedia() const { return box_->IsMedia(); }
  bool IsRubyRun() const { return IsBlock() && box_->IsRubyRun(); }
  bool IsRubyText() const { return box_->IsRubyText(); }

  // Return true if this is the legend child of a fieldset that gets special
  // treatment (i.e. placed over the block-start border).
  bool IsRenderedLegend() const {
    return IsBlock() && box_->IsRenderedLegend();
  }
  // Return true if this node is for <input type=range>.
  bool IsSlider() const;
  // Return true if this node is for a slider thumb in <input type=range>.
  bool IsSliderThumb() const;
  bool IsSvgText() const;
  bool IsTable() const { return IsBlock() && box_->IsTable(); }
  bool IsTextCombine() const { return box_->IsLayoutNGTextCombine(); }
  bool IsNGTable() const { return IsTable() && box_->IsLayoutNGObject(); }

  bool IsTableCaption() const { return IsBlock() && box_->IsTableCaption(); }
  bool IsTableSection() const { return IsBlock() && box_->IsTableSection(); }

  // Section with empty rows is considered empty.
  bool IsEmptyTableSection() const;

  bool IsTableCol() const {
    return Style().Display() == EDisplay::kTableColumn;
  }

  bool IsTableColgroup() const {
    return Style().Display() == EDisplay::kTableColumnGroup;
  }

  wtf_size_t TableColumnSpan() const;

  wtf_size_t TableCellColspan() const;

  wtf_size_t TableCellRowspan() const;

  bool IsTextArea() const { return box_->IsTextAreaIncludingNG(); }
  bool IsTextControl() const { return box_->IsTextControlIncludingNG(); }
  bool IsTextControlPlaceholder() const;
  bool IsTextField() const { return box_->IsTextFieldIncludingNG(); }

  bool IsMathRoot() const { return box_->IsMathMLRoot(); }
  bool IsMathML() const { return box_->IsMathML(); }

  bool IsAnonymous() const { return box_->IsAnonymous(); }
  bool IsAnonymousBlock() const { return box_->IsAnonymousBlock(); }

  // If the node is a quirky container for margin collapsing, see:
  // https://html.spec.whatwg.org/C/#margin-collapsing-quirks
  // NOTE: The spec appears to only somewhat match reality.
  bool IsQuirkyContainer() const {
    return box_->GetDocument().InQuirksMode() &&
           (box_->IsBody() || box_->IsTableCell());
  }

  // Return true if this node is monolithic for block fragmentation.
  bool IsMonolithic() const {
    // Lines are always monolithic. We cannot block-fragment inside them.
    if (IsInline())
      return true;
    return box_->GetNGPaginationBreakability() == LayoutBox::kForbidBreaks;
  }

  AtomicString PageName() const {
    return IsBlock() ? Style().Page() : AtomicString();
  }

  bool IsScrollContainer() const {
    return IsBlock() && box_->IsScrollContainer();
  }

  // Return true if this is the document root and it is paginated. A paginated
  // root establishes a fragmentation context.
  bool IsPaginatedRoot() const;

  bool CreatesNewFormattingContext() const {
    return IsBlock() && box_->CreatesNewFormattingContext();
  }

  // Returns true if this node should pass its percentage resolution block-size
  // to its children. Typically only quirks-mode, auto block-size, block nodes.
  bool UseParentPercentageResolutionBlockSizeForChildren() const {
    auto* layout_block = DynamicTo<LayoutBlock>(box_.Get());
    if (IsBlock() && layout_block) {
      return LayoutBoxUtils::SkipContainingBlockForPercentHeightCalculation(
          layout_block);
    }

    return false;
  }

  // Returns intrinsic sizing information for replaced elements.
  // ComputeReplacedSize can use it to compute actual replaced size.
  // Corresponds to Legacy's LayoutReplaced::IntrinsicSizingInfo.
  // Use NGBlockNode::GetAspectRatio to get the aspect ratio.
  void IntrinsicSize(absl::optional<LayoutUnit>* computed_inline_size,
                     absl::optional<LayoutUnit>* computed_block_size) const;

  // Returns the next sibling.
  NGLayoutInputNode NextSibling() const;

  Document& GetDocument() const { return box_->GetDocument(); }

  Node* GetDOMNode() const { return box_->GetNode(); }

  PhysicalSize InitialContainingBlockSize() const;

  // Returns the LayoutObject which is associated with this node.
  LayoutBox* GetLayoutBox() const { return box_; }

  const ComputedStyle& Style() const { return box_->StyleRef(); }

  bool ShouldApplySizeContainment() const {
    return box_->ShouldApplySizeContainment();
  }
  // Return true if we should apply at least inline-size containment
  // (i.e. "contain" is "size" or "inline-size").
  bool ShouldApplyInlineSizeContainment() const {
    return box_->ShouldApplyInlineSizeContainment();
  }
  // Return true if we should apply at least block-size containment
  // (i.e. "contain" is "size" or "block-size").
  bool ShouldApplyBlockSizeContainment() const {
    return box_->ShouldApplyBlockSizeContainment();
  }

  bool CanMatchSizeContainerQueries() const {
    return box_->CanMatchSizeContainerQueries();
  }

  LogicalAxes ContainedAxes() const {
    LogicalAxes axes = kLogicalAxisNone;
    if (ShouldApplyInlineSizeContainment())
      axes |= kLogicalAxisInline;
    if (ShouldApplyBlockSizeContainment())
      axes |= kLogicalAxisBlock;
    return axes;
  }

  // CSS intrinsic sizing getters.
  // https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
  // Note that this returns kIndefiniteSize if the override was not specified.
  LayoutUnit OverrideIntrinsicContentInlineSize() const {
    if (box_->HasOverrideIntrinsicContentLogicalWidth())
      return box_->OverrideIntrinsicContentLogicalWidth();
    return kIndefiniteSize;
  }
  // Note that this returns kIndefiniteSize if the override was not specified.
  LayoutUnit OverrideIntrinsicContentBlockSize() const {
    if (box_->HasOverrideIntrinsicContentLogicalHeight())
      return box_->OverrideIntrinsicContentLogicalHeight();
    return kIndefiniteSize;
  }

  LayoutUnit DefaultIntrinsicContentInlineSize() const {
    return box_->DefaultIntrinsicContentInlineSize();
  }
  LayoutUnit DefaultIntrinsicContentBlockSize() const {
    return box_->DefaultIntrinsicContentBlockSize();
  }

  bool ChildLayoutBlockedByDisplayLock() const {
    return box_->ChildLayoutBlockedByDisplayLock();
  }

  CustomLayoutChild* GetCustomLayoutChild() const {
    // TODO(ikilpatrick): Support NGInlineNode.
    DCHECK(IsBlock());
    return box_->GetCustomLayoutChild();
  }

  // Return whether we can directly traverse fragments generated from this node
  // (for painting, hit-testing and other layout read operations). If false is
  // returned, we need to traverse the layout object tree instead.
  bool CanTraversePhysicalFragments() const {
    return box_->CanTraversePhysicalFragments();
  }

  String ToString() const;

  explicit operator bool() const { return box_ != nullptr; }

  bool operator==(const NGLayoutInputNode& other) const {
    return box_ == other.box_;
  }

  bool operator!=(const NGLayoutInputNode& other) const {
    return !(*this == other);
  }

#if DCHECK_IS_ON()
  void ShowNodeTree() const;
#endif

  void Trace(Visitor* visitor) const { visitor->Trace(box_); }

 protected:
  NGLayoutInputNode(LayoutBox* box, NGLayoutInputNodeType type)
      : box_(box), type_(type) {}

  void GetOverrideIntrinsicSize(
      absl::optional<LayoutUnit>* computed_inline_size,
      absl::optional<LayoutUnit>* computed_block_size) const;

  Member<LayoutBox> box_;

  unsigned type_ : 1;  // NGLayoutInputNodeType
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_
