// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class ComputedStyle;
class DisplayLockContext;
class Document;
class LayoutObject;
class LayoutBox;
class NGConstraintSpace;
class NGPaintFragment;
struct MinMaxSize;
struct LogicalSize;
struct PhysicalSize;

enum class NGMinMaxSizeType { kContentBoxSize, kBorderBoxSize };

// Input to the min/max inline size calculation algorithm for child nodes. Child
// nodes within the same formatting context need to know which floats are beside
// them.
struct MinMaxSizeInput {
  // The min-max size calculation (un-intuitively) requires a percentage
  // resolution size!
  // This occurs when a replaced element has an intrinsic size. E.g.
  // <div style="float: left; height: 100px">
  //   <img sr="intrinsic-ratio-1x1.png" style="height: 50%;" />
  // </div>
  // In the above example float ends up with a width of 50px.
  //
  // As we don't perform any tree walking, we need to pass the percentage
  // resolution block-size for min/max down the min/max size calculation.
  explicit MinMaxSizeInput(LayoutUnit percentage_resolution_block_size)
      : percentage_resolution_block_size(percentage_resolution_block_size) {}
  LayoutUnit float_left_inline_size;
  LayoutUnit float_right_inline_size;
  LayoutUnit percentage_resolution_block_size;

  // Whether to return the size as a content-box size or border-box size.
  NGMinMaxSizeType size_type = NGMinMaxSizeType::kBorderBoxSize;
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
  bool IsLayoutNGCustom() const {
    return IsBlock() && box_->IsLayoutNGCustom();
  }
  bool IsColumnSpanAll() const { return IsBlock() && box_->IsColumnSpanAll(); }
  bool IsFloating() const { return IsBlock() && box_->IsFloating(); }
  bool IsOutOfFlowPositioned() const {
    return IsBlock() && box_->IsOutOfFlowPositioned();
  }
  bool IsReplaced() const { return box_->IsLayoutReplaced(); }
  bool IsAbsoluteContainer() const {
    return box_->CanContainAbsolutePositionObjects();
  }
  bool IsFixedContainer() const {
    return box_->CanContainFixedPositionObjects();
  }
  bool IsBody() const { return IsBlock() && box_->IsBody(); }
  bool IsDocumentElement() const { return box_->IsDocumentElement(); }
  bool IsFlexItem() const { return IsBlock() && box_->IsFlexItemIncludingNG(); }
  bool IsFlexibleBox() const {
    return IsBlock() && box_->IsFlexibleBoxIncludingNG();
  }
  bool ShouldBeConsideredAsReplaced() const {
    return box_->ShouldBeConsideredAsReplaced();
  }
  bool IsListItem() const { return IsBlock() && box_->IsLayoutNGListItem(); }
  bool IsListMarker() const {
    return IsBlock() && box_->IsLayoutNGListMarker();
  }
  bool ListMarkerOccupiesWholeLine() const {
    DCHECK(IsListMarker());
    return ToLayoutNGListMarker(box_)->NeedsOccupyWholeLine();
  }
  bool IsFieldsetContainer() const {
    return IsBlock() && box_->IsLayoutNGFieldset();
  }

  // Return true if this is the legend child of a fieldset that gets special
  // treatment (i.e. placed over the block-start border).
  bool IsRenderedLegend() const {
    return IsBlock() && box_->IsRenderedLegend();
  }

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
    return box_->GetPaginationBreakability() == LayoutBox::kForbidBreaks;
  }

  bool CreatesNewFormattingContext() const {
    return IsBlock() && box_->CreatesNewFormattingContext();
  }

  // Returns true if this node should pass its percentage resolution block-size
  // to its children. Typically only quirks-mode, auto block-size, block nodes.
  bool UseParentPercentageResolutionBlockSizeForChildren() const {
    auto* layout_block = DynamicTo<LayoutBlock>(box_);
    if (IsBlock() && layout_block) {
      return LayoutBoxUtils::SkipContainingBlockForPercentHeightCalculation(
          layout_block);
    }

    return false;
  }

  // Returns border box.
  MinMaxSize ComputeMinMaxSize(WritingMode,
                               const MinMaxSizeInput&,
                               const NGConstraintSpace* = nullptr);

  // Returns intrinsic sizing information for replaced elements.
  // ComputeReplacedSize can use it to compute actual replaced size.
  // Corresponds to Legacy's LayoutReplaced::IntrinsicSizingInfo.
  void IntrinsicSize(base::Optional<LayoutUnit>* computed_inline_size,
                     base::Optional<LayoutUnit>* computed_block_size,
                     LogicalSize* aspect_ratio) const;

  // Returns the next sibling.
  NGLayoutInputNode NextSibling();

  Document& GetDocument() const { return box_->GetDocument(); }

  PhysicalSize InitialContainingBlockSize() const;

  // Returns the LayoutObject which is associated with this node.
  LayoutBox* GetLayoutBox() const { return box_; }

  const ComputedStyle& Style() const { return box_->StyleRef(); }

  bool ShouldApplySizeContainment() const {
    return box_->ShouldApplySizeContainment();
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

  // Display locking functionality.
  const DisplayLockContext& GetDisplayLockContext() const {
    DCHECK(box_->GetDisplayLockContext());
    return *box_->GetDisplayLockContext();
  }
  bool LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget target) const {
    return box_->LayoutBlockedByDisplayLock(target);
  }

  // Returns the first NGPaintFragment for this node. When block fragmentation
  // occurs, there will be multiple NGPaintFragment for a node.
  const NGPaintFragment* PaintFragment() const;

  CustomLayoutChild* GetCustomLayoutChild() const {
    // TODO(ikilpatrick): Support NGInlineNode.
    DCHECK(IsBlock());
    return box_->GetCustomLayoutChild();
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

 protected:
  NGLayoutInputNode(LayoutBox* box, NGLayoutInputNodeType type)
      : box_(box), type_(type) {}

  LayoutBox* box_;

  unsigned type_ : 1;  // NGLayoutInputNodeType
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_INPUT_NODE_H_
