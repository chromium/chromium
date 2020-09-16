// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_FRAGMENT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

#include <unicode/ubidi.h>

namespace blink {

class ComputedStyle;
class FragmentData;
class Node;
class NGFragmentBuilder;
class NGInlineItem;
class NGPhysicalFragment;
class PaintLayer;
struct LogicalRect;

struct CORE_EXPORT NGPhysicalFragmentTraits {
  static void Destruct(const NGPhysicalFragment*);
};

// The NGPhysicalFragment contains the output geometry from layout. The
// fragment stores all of its information in the physical coordinate system for
// use by paint, hit-testing etc.
//
// The fragment keeps a pointer back to the LayoutObject which generated it.
// Once we have transitioned fully to LayoutNG it should be a const pointer
// such that paint/hit-testing/etc don't modify it.
//
// Layout code should only access geometry information through the
// NGFragment wrapper classes which transforms information into the logical
// coordinate system.
class CORE_EXPORT NGPhysicalFragment
    : public RefCounted<const NGPhysicalFragment, NGPhysicalFragmentTraits> {
 public:
  enum NGFragmentType {
    kFragmentBox = 0,
    kFragmentText = 1,
    kFragmentLineBox = 2,
    // When adding new values, make sure the bit size of |type_| is large
    // enough to store.
  };
  enum NGBoxType {
    kNormalBox,
    kInlineBox,
    // A multi-column container creates column boxes as its children, which
    // content is flowed into. https://www.w3.org/TR/css-multicol-1/#column-box
    kColumnBox,
    kAtomicInline,
    kFloating,
    kOutOfFlowPositioned,
    kBlockFlowRoot,
    kRenderedLegend,
    // When adding new values, make sure the bit size of |sub_type_| is large
    // enough to store.

    // Also, add after kMinimumFormattingContextRoot if the box type is a
    // formatting context root, or before otherwise. See
    // IsFormattingContextRoot().
    kMinimumFormattingContextRoot = kAtomicInline
  };

  ~NGPhysicalFragment();

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  bool IsContainer() const {
    return Type() == NGFragmentType::kFragmentBox ||
           Type() == NGFragmentType::kFragmentLineBox;
  }
  bool IsBox() const { return Type() == NGFragmentType::kFragmentBox; }
  bool IsText() const { return Type() == NGFragmentType::kFragmentText; }
  bool IsLineBox() const { return Type() == NGFragmentType::kFragmentLineBox; }

  // Returns the box type of this fragment.
  NGBoxType BoxType() const {
    DCHECK(IsBox());
    return static_cast<NGBoxType>(sub_type_);
  }
  // True if this is an inline box; e.g., <span>. Atomic inlines such as
  // replaced elements or inline block are not included.
  bool IsInlineBox() const {
    return IsBox() && BoxType() == NGBoxType::kInlineBox;
  }
  bool IsColumnBox() const {
    return IsBox() && BoxType() == NGBoxType::kColumnBox;
  }
  bool IsFragmentainerBox() const { return IsColumnBox(); }
  bool IsColumnSpanAll() const {
    if (const LayoutBox* box = ToLayoutBoxOrNull(GetLayoutObject()))
      return box->IsColumnSpanAll();
    return false;
  }
  // An atomic inline is represented as a kFragmentBox, such as inline block and
  // replaced elements.
  bool IsAtomicInline() const {
    return IsBox() && BoxType() == NGBoxType::kAtomicInline;
  }
  // True if this fragment is in-flow in an inline formatting context.
  bool IsInline() const {
    return IsText() || IsInlineBox() || IsAtomicInline();
  }
  bool IsFloating() const {
    return IsBox() && BoxType() == NGBoxType::kFloating;
  }
  bool IsOutOfFlowPositioned() const {
    return IsBox() && BoxType() == NGBoxType::kOutOfFlowPositioned;
  }
  bool IsFloatingOrOutOfFlowPositioned() const {
    return IsFloating() || IsOutOfFlowPositioned();
  }
  // Return true if this is the legend child of a fieldset that gets special
  // treatment (i.e. placed over the block-start border).
  bool IsRenderedLegend() const {
    return IsBox() && BoxType() == NGBoxType::kRenderedLegend;
  }
  bool IsMathMLFraction() const { return IsBox() && is_math_fraction_; }

  // Return true if this fragment corresponds directly to an entry in the CSS
  // box tree [1]. Note that anonymous blocks also exist in the CSS box
  // tree. Returns false otherwise, i.e. if the fragment is generated by the
  // layout engine to contain fragments from CSS boxes (a line or a generated
  // fragmentainer [2], in other words). The main signification of this is
  // whether we can use the LayoutObject associated with this fragment for all
  // purposes.
  //
  // [1] https://www.w3.org/TR/css-display-3/#box-tree
  // [2] https://www.w3.org/TR/css-break-3/#fragmentation-container
  bool IsCSSBox() const { return !IsLineBox() && !IsFragmentainerBox(); }

  bool IsBlockFlow() const;
  bool IsAnonymousBlock() const {
    return IsCSSBox() && layout_object_->IsAnonymousBlock();
  }
  bool IsListMarker() const {
    return IsCSSBox() && layout_object_->IsLayoutNGOutsideListMarker();
  }
  bool IsRubyRun() const { return layout_object_->IsRubyRun(); }

  // Return true if this fragment is for LayoutNGRubyRun, LayoutNGRubyText, or
  // LayoutNGRubyBase. They are handled specially in scrollable overflow
  // computation.
  bool IsRubyBox() const {
    return layout_object_->IsRubyRun() || layout_object_->IsRubyText() ||
           layout_object_->IsRubyBase();
  }

  bool IsTable() const { return layout_object_->IsTable(); }

  // Return true if this fragment is a container established by a fieldset
  // element. Such a fragment contains an optional rendered legend fragment and
  // an optional fieldset contents wrapper fragment (which holds everything
  // inside the fieldset except the rendered legend).
  bool IsFieldsetContainer() const { return is_fieldset_container_; }

  // Returns whether the fragment is legacy layout root.
  bool IsLegacyLayoutRoot() const { return is_legacy_layout_root_; }

  // Returns whether the fragment should be atomically painted.
  bool IsPaintedAtomically() const { return is_painted_atomically_; }

  bool IsFormattingContextRoot() const {
    return (IsBox() && BoxType() >= NGBoxType::kMinimumFormattingContextRoot) ||
           IsLegacyLayoutRoot();
  }

  // |Offset()| is reliable only when this fragment was placed by LayoutNG
  // parent. When the parent is not LayoutNG, the parent may move the
  // |LayoutObject| after this fragment was placed. See comments in
  // |LayoutNGBlockFlow::UpdateBlockLayout()| and crbug.com/788590
  bool IsPlacedByLayoutNG() const;

  // The accessors in this class shouldn't be used by layout code directly,
  // instead should be accessed by the NGFragmentBase classes. These accessors
  // exist for paint, hit-testing, etc.

  // Returns the border-box size.
  PhysicalSize Size() const { return size_; }

  // Returns the rect in the local coordinate of this fragment; i.e., offset is
  // (0, 0).
  PhysicalRect LocalRect() const { return {{}, size_}; }

  NGStyleVariant StyleVariant() const {
    return static_cast<NGStyleVariant>(style_variant_);
  }
  bool UsesFirstLineStyle() const {
    return StyleVariant() == NGStyleVariant::kFirstLine;
  }

  // Returns the style for this fragment.
  //
  // For a line box, this returns the style of the containing block. This mostly
  // represents the style for the line box, except 1) |style.Direction()| maybe
  // incorrect, use |BaseDirection()| instead, and 2) margin/border/padding,
  // background etc. do not apply to the line box.
  const ComputedStyle& Style() const {
    return layout_object_->EffectiveStyle(StyleVariant());
  }

  const Document& GetDocument() const {
    DCHECK(layout_object_);
    return layout_object_->GetDocument();
  }
  Node* GetNode() const {
    return IsCSSBox() ? layout_object_->GetNode() : nullptr;
  }
  Node* GeneratingNode() const {
    return IsCSSBox() ? layout_object_->GeneratingNode() : nullptr;
  }
  // The node to return when hit-testing on this fragment. This can be different
  // from GetNode() when this fragment is content of a pseudo node.
  Node* NodeForHitTest() const { return layout_object_->NodeForHitTest(); }

  bool IsInSelfHitTestingPhase(HitTestAction action) const {
    if (const auto* box = ToLayoutBoxOrNull(GetLayoutObject()))
      return box->IsInSelfHitTestingPhase(action);
    if (IsInlineBox())
      return action == kHitTestForeground;
    // Assuming this is some sort of container, e.g. a fragmentainer (they don't
    // have a LayoutObject associated).
    return action == kHitTestBlockBackground ||
           action == kHitTestChildBlockBackground;
  }

  // Whether there is a PaintLayer associated with the fragment.
  bool HasLayer() const { return IsCSSBox() && layout_object_->HasLayer(); }

  // The PaintLayer associated with the fragment.
  PaintLayer* Layer() const {
    if (!HasLayer())
      return nullptr;
    return To<LayoutBoxModelObject>(layout_object_)->Layer();
  }

  // Whether this object has a self-painting |Layer()|.
  bool HasSelfPaintingLayer() const {
    return HasLayer() &&
           To<LayoutBoxModelObject>(layout_object_)->HasSelfPaintingLayer();
  }

  // True if overflow != 'visible', except for certain boxes that do not allow
  // overflow clip; i.e., AllowOverflowClip() returns false.
  bool HasNonVisibleOverflow() const {
    return IsCSSBox() && layout_object_->HasNonVisibleOverflow();
  }

  // True if this is considered a scroll-container. See
  // ComputedStyle::IsScrollContainer() for details.
  bool IsScrollContainer() const {
    return IsCSSBox() && layout_object_->IsScrollContainer();
  }

  bool ShouldClipOverflow() const {
    return IsCSSBox() && layout_object_->ShouldClipOverflow();
  }

  bool IsFragmentationContextRoot() const {
    // We have no bit that tells us whether this is a fragmentation context
    // root, so some additional checking is necessary here, to make sure that
    // we're actually establishing one. We check that we're not a custom layout
    // box, as specifying columns on such a box has no effect. Note that
    // specifying columns together with a display value of e.g. 'flex', 'grid'
    // or 'table' also has no effect, but we don't need to check for that here,
    // since such display types don't create a block flow (block container).
    return IsCSSBox() && Style().SpecifiesColumns() && IsBlockFlow() &&
           !layout_object_->IsLayoutNGCustom();
  }

  // Return whether we can traverse this fragment and its children directly, for
  // painting, hit-testing and other layout read operations. If false is
  // returned, we need to traverse the layout object tree instead.
  bool CanTraverse() const {
    return layout_object_->CanTraversePhysicalFragments();
  }

  // This fragment is hidden for paint purpose, but exists for querying layout
  // information. Used for `text-overflow: ellipsis`.
  bool IsHiddenForPaint() const { return is_hidden_for_paint_; }

  // Return true if this fragment is monolithic, as far as block fragmentation
  // is concerned.
  bool IsMonolithic() const {
    const LayoutObject* layout_object = GetLayoutObject();
    if (!layout_object || !IsBox() || !layout_object->IsBox())
      return false;
    return ToLayoutBox(layout_object)->GetPaginationBreakability() ==
           LayoutBox::kForbidBreaks;
  }

  // GetLayoutObject should only be used when necessary for compatibility
  // with LegacyLayout.
  //
  // For a line box, |layout_object_| has its containing block but this function
  // returns |nullptr| for the historical reasons. TODO(kojii): We may change
  // this in future. Use |IsLineBox()| instead of testing this is |nullptr|.
  const LayoutObject* GetLayoutObject() const {
    return IsCSSBox() ? layout_object_ : nullptr;
  }
  // TODO(kojii): We should not have mutable version at all, the use of this
  // function should be eliminiated over time.
  LayoutObject* GetMutableLayoutObject() const {
    return IsCSSBox() ? layout_object_ : nullptr;
  }
  // Similar to |GetLayoutObject|, but returns the |LayoutObject| of its
  // container for |!IsCSSBox()| fragments instead of |nullptr|.
  const LayoutObject* GetSelfOrContainerLayoutObject() const {
    return layout_object_;
  }

  const FragmentData* GetFragmentData() const;

  // |NGPhysicalFragment| may live longer than the corresponding |LayoutObject|.
  // Though |NGPhysicalFragment| is immutable, |layout_object_| is cleared to
  // |nullptr| when it was destroyed to avoid reading destroyed objects.
  bool IsLayoutObjectDestroyedOrMoved() const { return !layout_object_; }
  void LayoutObjectWillBeDestroyed() const {
    const_cast<NGPhysicalFragment*>(this)->layout_object_ = nullptr;
  }

  // Returns the latest generation of the post-layout fragment. Returns
  // |nullptr| if |this| is the one.
  //
  // When subtree relayout occurs at the relayout boundary, its containing block
  // may keep the reference to old generations of this fragment. Callers can
  // check if there were newer generations.
  const NGPhysicalFragment* PostLayout() const;

  PhysicalRect InkOverflow() const {
    // TODO(layout-dev): Implement box fragment overflow.
    return LocalRect();
  }

  // Specifies the type of scrollable overflow computation.
  enum TextHeightType {
    // Apply text fragment size as is.
    kNormalHeight,
    // Adjust text fragment size for 'em' height, and skip to unite
    // container's bounding box. This type is useful for ruby annotation.
    kEmHeight
  };
  // Scrollable overflow. including contents, in the local coordinate.
  PhysicalRect ScrollableOverflow(const NGPhysicalBoxFragment& container,
                                  TextHeightType height_type) const;

  // ScrollableOverflow(), with transforms applied wrt container if needed.
  // This does not include any offsets from the parent (including relpos).
  PhysicalRect ScrollableOverflowForPropagation(
      const NGPhysicalBoxFragment& container,
      TextHeightType height_type) const;
  void AdjustScrollableOverflowForPropagation(
      const NGPhysicalBoxFragment& container,
      PhysicalRect* overflow) const;

  // The allowed touch action is the union of the effective touch action
  // (from style) and blocking touch event handlers.
  TouchAction EffectiveAllowedTouchAction() const;

  // Returns the bidi level of a text or atomic inline fragment.
  UBiDiLevel BidiLevel() const;

  // Returns the resolved direction of a text or atomic inline fragment. Not to
  // be confused with the CSS 'direction' property.
  TextDirection ResolvedDirection() const;

  // Helper functions to convert between |PhysicalRect| and |LogicalRect| of a
  // child.
  LogicalRect ConvertChildToLogical(const PhysicalRect& physical_rect) const;
  PhysicalRect ConvertChildToPhysical(const LogicalRect& logical_rect) const;

  // Utility functions for caret painting. Note that carets are painted as part
  // of the containing block's foreground.
  bool ShouldPaintCursorCaret() const;
  bool ShouldPaintDragCaret() const;
  bool ShouldPaintCarets() const {
    return ShouldPaintCursorCaret() || ShouldPaintDragCaret();
  }

  String ToString() const;

  void CheckType() const;
  void CheckCanUpdateInkOverflow() const;

  enum DumpFlag {
    DumpHeaderText = 0x1,
    DumpSubtree = 0x2,
    DumpIndentation = 0x4,
    DumpType = 0x8,
    DumpOffset = 0x10,
    DumpSize = 0x20,
    DumpTextOffsets = 0x40,
    DumpSelfPainting = 0x80,
    DumpNodeName = 0x100,
    DumpItems = 0x200,
    DumpAll = -1
  };
  typedef int DumpFlags;

  String DumpFragmentTree(DumpFlags,
                          base::Optional<PhysicalOffset> = base::nullopt,
                          unsigned indent = 2) const;

#if DCHECK_IS_ON()
  void ShowFragmentTree() const;
#endif

 protected:
  NGPhysicalFragment(NGFragmentBuilder*,
                     NGFragmentType type,
                     unsigned sub_type);

  NGPhysicalFragment(LayoutObject* layout_object,
                     NGStyleVariant,
                     PhysicalSize size,
                     NGFragmentType type,
                     unsigned sub_type);

  const ComputedStyle& SlowEffectiveStyle() const;

  const Vector<NGInlineItem>& InlineItemsOfContainingBlock() const;

  // The following bitfields are only to be used by NGPhysicalContainerFragment
  // (it's defined here to save memory, since that class has no bitfields).
  unsigned has_floating_descendants_for_paint_ : 1;
  unsigned has_adjoining_object_descendants_ : 1;
  unsigned depends_on_percentage_block_size_ : 1;

  // The following bitfields are only to be used by NGPhysicalLineBoxFragment
  // (it's defined here to save memory, since that class has no bitfields).
  unsigned has_propagated_descendants_ : 1;
  unsigned has_hanging_ : 1;

  // The following bitfields are only to be used by NGPhysicalBoxFragment
  // (it's defined here to save memory, since that class has no bitfields).
  unsigned is_inline_formatting_context_ : 1;
  unsigned has_fragment_items_ : 1;
  unsigned include_border_top_ : 1;
  unsigned include_border_right_ : 1;
  unsigned include_border_bottom_ : 1;
  unsigned include_border_left_ : 1;
  unsigned has_borders_ : 1;
  unsigned has_padding_ : 1;
  unsigned is_first_for_node_ : 1;
  unsigned has_rare_data_ : 1;

  LayoutObject* layout_object_;
  const PhysicalSize size_;

  const unsigned type_ : 2;           // NGFragmentType
  const unsigned sub_type_ : 3;       // NGBoxType, NGTextType, or NGLineBoxType
  const unsigned style_variant_ : 2;  // NGStyleVariant
  const unsigned is_hidden_for_paint_ : 1;
  unsigned is_math_fraction_ : 1;
  // base (line box) or resolve (text) direction
  unsigned base_or_resolved_direction_ : 1;  // TextDirection
  unsigned may_have_descendant_above_block_start_ : 1;

  // The following are only used by NGPhysicalBoxFragment but are initialized
  // for all types to allow methods using them to be inlined.
  unsigned is_fieldset_container_ : 1;
  unsigned is_legacy_layout_root_ : 1;
  unsigned is_painted_atomically_ : 1;
  unsigned has_baseline_ : 1;
  unsigned has_last_baseline_ : 1;

  // The following bitfields are only to be used by NGPhysicalTextFragment
  // (it's defined here to save memory, since that class has no bitfields).
  mutable unsigned ink_overflow_computed_ : 1;

  // Note: We've used 32-bit bit field. If you need more bits, please think to
  // share bit fields, or put them before layout_object_ to fill the gap after
  // RefCounted on 64-bit systems.

 private:
  friend struct NGPhysicalFragmentTraits;
  void Destroy() const;
};

// Used for return value of traversing fragment tree.
struct CORE_EXPORT NGPhysicalFragmentWithOffset {
  DISALLOW_NEW();

  scoped_refptr<const NGPhysicalFragment> fragment;
  PhysicalOffset offset_to_container_box;

  PhysicalRect RectInContainerBox() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalFragment*);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalFragment&);

#if !DCHECK_IS_ON()
inline void NGPhysicalFragment::CheckType() const {}
inline void NGPhysicalFragment::CheckCanUpdateInkOverflow() const {}
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_FRAGMENT_H_
