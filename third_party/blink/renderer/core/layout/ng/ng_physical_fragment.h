// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragment_h
#define NGPhysicalFragment_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

#include <unicode/ubidi.h>

namespace blink {

class ComputedStyle;
class LayoutObject;
class Node;
class NGFragmentBuilder;
class NGBreakToken;
class NGInlineItem;
struct NGPixelSnappedPhysicalBoxStrut;
class PaintLayer;

class NGPhysicalFragment;

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
    kFragmentRenderedLegend = 3,
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
    // When adding new values, make sure the bit size of |sub_type_| is large
    // enough to store.

    // Also, add after kMinimumBlockFormattingContextRoot if the box type is a
    // block formatting context root, or before otherwise. See
    // IsBlockFormattingContextRoot().
    kMinimumBlockFormattingContextRoot = kAtomicInline
  };

  ~NGPhysicalFragment();

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  bool IsContainer() const {
    return Type() == NGFragmentType::kFragmentBox ||
           Type() == NGFragmentType::kFragmentLineBox ||
           Type() == NGFragmentType::kFragmentRenderedLegend;
  }
  bool IsBox() const { return Type() == NGFragmentType::kFragmentBox; }
  bool IsText() const { return Type() == NGFragmentType::kFragmentText; }
  bool IsLineBox() const { return Type() == NGFragmentType::kFragmentLineBox; }

  // Return true if this is the legend child of a fieldset that gets special
  // treatment (i.e. placed over the block-start border).
  bool IsRenderedLegend() const {
    return Type() == NGFragmentType::kFragmentRenderedLegend;
  }

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
  bool IsBlockFlow() const;
  bool IsListMarker() const;

  // Return true if this fragment is a container established by a fieldset
  // element. Such a fragment contains an optional rendered legend fragment and
  // an optional fieldset contents wrapper fragment (which holds everything
  // inside the fieldset except the rendered legend).
  bool IsFieldsetContainer() const { return is_fieldset_container_; }

  // Returns whether the fragment is old layout root.
  bool IsOldLayoutRoot() const { return is_old_layout_root_; }

  bool IsBlockFormattingContextRoot() const {
    return (IsBox() &&
            BoxType() >= NGBoxType::kMinimumBlockFormattingContextRoot) ||
           IsOldLayoutRoot();
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
  NGPhysicalSize Size() const { return size_; }

  // Returns the rect in the local coordinate of this fragment; i.e., offset is
  // (0, 0).
  NGPhysicalOffsetRect LocalRect() const { return {{}, size_}; }

  // Bitmask for border edges, see NGBorderEdges::Physical.
  unsigned BorderEdges() const { return border_edge_; }
  NGPixelSnappedPhysicalBoxStrut BorderWidths() const;

  NGBreakToken* BreakToken() const { return break_token_.get(); }
  NGStyleVariant StyleVariant() const {
    return static_cast<NGStyleVariant>(style_variant_);
  }
  bool UsesFirstLineStyle() const {
    return StyleVariant() == NGStyleVariant::kFirstLine;
  }
  const ComputedStyle& Style() const;
  Node* GetNode() const;

  // Whether there is a PaintLayer associated with the fragment.
  bool HasLayer() const;

  // The PaintLayer associated with the fragment.
  PaintLayer* Layer() const;

  // GetLayoutObject should only be used when necessary for compatibility
  // with LegacyLayout.
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  // InkOverflow of itself, not including contents, in the local coordinate.
  NGPhysicalOffsetRect SelfInkOverflow() const;

  // InkOverflow of itself including contents, in the local coordinate.
  NGPhysicalOffsetRect InkOverflow(bool apply_clip = true) const;

  // Scrollable overflow. including contents, in the local coordinate.
  NGPhysicalOffsetRect ScrollableOverflow() const;

  // Unite visual rect to propagate to parent's ContentsVisualRect.
  void PropagateContentsInkOverflow(NGPhysicalOffsetRect*,
                                    NGPhysicalOffset) const;

  // The whitelisted touch action is the union of the effective touch action
  // (from style) and blocking touch event handlers.
  TouchAction EffectiveWhitelistedTouchAction() const;

  // Returns the bidi level of a text or atomic inline fragment.
  virtual UBiDiLevel BidiLevel() const;

  // Returns the resolved direction of a text or atomic inline fragment. Not to
  // be confused with the CSS 'direction' property.
  virtual TextDirection ResolvedDirection() const;

  String ToString() const;

  enum DumpFlag {
    DumpHeaderText = 0x1,
    DumpSubtree = 0x2,
    DumpIndentation = 0x4,
    DumpType = 0x8,
    DumpOffset = 0x10,
    DumpSize = 0x20,
    DumpTextOffsets = 0x40,
    DumpSelfPainting = 0x80,
    DumpOverflow = 0x100,
    DumpNodeName = 0x200,
    DumpAll = -1
  };
  typedef int DumpFlags;

  String DumpFragmentTree(DumpFlags,
                          base::Optional<NGPhysicalOffset> = base::nullopt,
                          unsigned indent = 2) const;

#ifndef NDEBUG
  void ShowFragmentTree() const;
#endif

 protected:
  NGPhysicalFragment(NGFragmentBuilder*,
                     NGFragmentType type,
                     unsigned sub_type);

  NGPhysicalFragment(LayoutObject* layout_object,
                     const ComputedStyle& style,
                     NGStyleVariant,
                     NGPhysicalSize size,
                     NGFragmentType type,
                     unsigned sub_type,
                     scoped_refptr<NGBreakToken> break_token = nullptr);

  const Vector<NGInlineItem>& InlineItemsOfContainingBlock() const;

  LayoutObject* const layout_object_;
  scoped_refptr<const ComputedStyle> style_;
  const NGPhysicalSize size_;
  scoped_refptr<NGBreakToken> break_token_;

  const unsigned type_ : 2;      // NGFragmentType
  const unsigned sub_type_ : 3;  // Union of NGBoxType and NGTextType
  unsigned is_fieldset_container_ : 1;
  unsigned is_old_layout_root_ : 1;
  unsigned border_edge_ : 4;  // NGBorderEdges::Physical
  const unsigned style_variant_ : 2;  // NGStyleVariant
  unsigned base_direction_ : 1;  // TextDirection, for NGPhysicalLineBoxFragment

  // The following bitfield is only to be used by NGPhysicalBoxFragment (it's
  // defined here to save memory, since that class has no bitfields).
  unsigned children_inline_ : 1;

 private:
  friend struct NGPhysicalFragmentTraits;
  void Destroy() const;
};

// Used for return value of traversing fragment tree.
struct CORE_EXPORT NGPhysicalFragmentWithOffset {
  DISALLOW_NEW();

  scoped_refptr<const NGPhysicalFragment> fragment;
  NGPhysicalOffset offset_to_container_box;

  NGPhysicalOffsetRect RectInContainerBox() const;
};

}  // namespace blink

#endif  // NGPhysicalFragment_h
