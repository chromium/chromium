// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_

#include <iterator>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace blink {

class LayoutInline;
struct LayoutSelectionStatus;
struct PaintInfo;
enum class NGOutlineType;

// The NGPaintFragment contains a NGPhysicalFragment and geometry in the paint
// coordinate system.
//
// NGPhysicalFragment is limited to its parent coordinate system for caching and
// sharing subtree. This class makes it possible to compute visual rects in the
// parent transform node.
//
// NGPaintFragment is an ImageResourceObserver, which means that it gets
// notified when associated images are changed.
// This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this
//   case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (<img>, svg <image>) or video (<video>) elements that are
//   placeholders for displaying them.
class CORE_EXPORT NGPaintFragment : public RefCounted<NGPaintFragment>,
                                    public DisplayItemClient,
                                    public ImageResourceObserver {
 public:
  NGPaintFragment(scoped_refptr<const NGPhysicalFragment>,
                  NGPhysicalOffset offset,
                  NGPaintFragment*);
  ~NGPaintFragment() override;

  static scoped_refptr<NGPaintFragment> Create(
      scoped_refptr<const NGPhysicalFragment>,
      NGPhysicalOffset offset,
      scoped_refptr<NGPaintFragment> previous_instance = nullptr);

  const NGPhysicalFragment& PhysicalFragment() const {
    return *physical_fragment_;
  }

  void UpdateFromCachedLayoutResult(
      scoped_refptr<const NGPhysicalFragment> fragment,
      NGPhysicalOffset offset);

  // Next/last fragment for  when this is fragmented.
  NGPaintFragment* Next() { return next_fragmented_.get(); }
  void SetNext(scoped_refptr<NGPaintFragment>);
  NGPaintFragment* Last();
  NGPaintFragment* Last(const NGBreakToken&);
  static scoped_refptr<NGPaintFragment>* Find(scoped_refptr<NGPaintFragment>*,
                                              const NGBreakToken*);

  template <typename Traverse>
  class List final {
   public:
    explicit List(NGPaintFragment* first) : first_(first) {}

    class Iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
     public:
      explicit Iterator(NGPaintFragment* first) : current_(first) {}

      NGPaintFragment* operator*() const { return current_; }
      NGPaintFragment* operator->() const { return current_; }
      Iterator& operator++() {
        DCHECK(current_);
        current_ = Traverse::Next(current_);
        return *this;
      }
      bool operator==(const Iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const Iterator& other) const {
        return current_ != other.current_;
      }

     private:
      NGPaintFragment* current_;
    };

    Iterator begin() const { return Iterator(first_); }
    Iterator end() const { return Iterator(nullptr); }

    // Returns the first |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |front()| for empty range.
    NGPaintFragment& front() const;

    // Returns the last |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |back()| for empty range.
    // Note: The complexity of |back()| is O(n) where n is number of elements
    // in this |FragmentRange|.
    NGPaintFragment& back() const;

    // Returns number of fragments in this range. The complexity is O(n) where n
    // is number of elements.
    wtf_size_t size() const;
    bool IsEmpty() const { return !first_; }

    void ToList(Vector<NGPaintFragment*, 16>*) const;

   private:
    NGPaintFragment* first_;
  };

  class TraverseNextSibling {
   public:
    static NGPaintFragment* Next(NGPaintFragment* current) {
      return current->next_sibling_.get();
    }
  };
  using ChildList = List<TraverseNextSibling>;

  // The parent NGPaintFragment. This is nullptr for a root; i.e., when parent
  // is not for NGPaint. In the first phase, this means that this is a root of
  // an inline formatting context.
  NGPaintFragment* Parent() const { return parent_; }
  NGPaintFragment* FirstChild() const { return first_child_.get(); }
  NGPaintFragment* NextSibling() const { return next_sibling_.get(); }
  ChildList Children() const { return ChildList(first_child_.get()); }

  // Note, as the name implies, |IsDescendantOfNotSelf| returns false for the
  // same object. This is different from |LayoutObject::IsDescendant| but is
  // same as |Node::IsDescendant|.
  bool IsDescendantOfNotSelf(const NGPaintFragment&) const;

  // Returns the first line box for a block-level container.
  NGPaintFragment* FirstLineBox() const;

  // Returns the container line box for inline fragments.
  const NGPaintFragment* ContainerLineBox() const;

  // Returns true if this fragment is line box and marked dirty.
  bool IsDirty() const { return is_dirty_inline_; }

  // Returns offset to its container box for inline and line box fragments.
  const NGPhysicalOffset& InlineOffsetToContainerBox() const {
    DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
    return inline_offset_to_container_box_;
  }

  // Update VisualRect for fragments without LayoutObjects (i.e., line boxes,)
  // after its descendants were updated.
  void UpdateVisualRectForNonLayoutObjectChildren();

  void AddSelfOutlineRect(Vector<LayoutRect>*,
                          const LayoutPoint& offset,
                          NGOutlineType) const;

  // TODO(layout-dev): Implement when we have oveflow support.
  // TODO(eae): Switch to using NG geometry types.
  bool HasOverflowClip() const;
  bool ShouldClipOverflow() const;
  bool HasSelfPaintingLayer() const;
  // This is equivalent to LayoutObject::VisualRect
  LayoutRect VisualRect() const override { return visual_rect_; }
  void SetVisualRect(const LayoutRect& rect) { visual_rect_ = rect; }

  LayoutRect SelectionVisualRect() const { return selection_visual_rect_; }
  void SetSelectionVisualRect(const LayoutRect& rect) {
    selection_visual_rect_ = rect;
  }

  // CSS ink overflow https://www.w3.org/TR/css-overflow-3/#ink
  // Encloses all pixels painted by self + children.
  LayoutRect SelfInkOverflow() const;
  // Union of children's ink overflows.
  LayoutRect ChildrenInkOverflow() const;

  NGPhysicalOffsetRect ComputeLocalSelectionRectForText(
      const LayoutSelectionStatus&) const;
  NGPhysicalOffsetRect ComputeLocalSelectionRectForReplaced() const;

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject.
  void SetShouldDoFullPaintInvalidation();

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject
  // recursively.
  void SetShouldDoFullPaintInvalidationRecursively();

  // Set ShouldDoFullPaintInvalidation flag to all objects in the first line of
  // this block-level fragment.
  void SetShouldDoFullPaintInvalidationForFirstLine();

  // Paint all descendant inline box fragments that belong to the specified
  // LayoutObject.
  void PaintInlineBoxForDescendants(const PaintInfo&,
                                    const LayoutPoint& paint_offset,
                                    const LayoutInline*,
                                    NGPhysicalOffset = {}) const;

  // DisplayItemClient methods.
  String DebugName() const override;

  // Commonly used functions for NGPhysicalFragment.
  Node* GetNode() const { return PhysicalFragment().GetNode(); }
  LayoutObject* GetLayoutObject() const {
    return PhysicalFragment().GetLayoutObject();
  }
  const ComputedStyle& Style() const { return PhysicalFragment().Style(); }
  NGPhysicalOffset Offset() const { return offset_; }
  NGPhysicalSize Size() const { return PhysicalFragment().Size(); }

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree.
  PositionWithAffinity PositionForPoint(const NGPhysicalOffset&) const;

  // The node to return when hit-testing on this fragment. This can be different
  // from GetNode() when this fragment is content of a pseudo node.
  Node* NodeForHitTest() const;

  // Utility functions for caret painting. Note that carets are painted as part
  // of the containing block's foreground.
  bool ShouldPaintCursorCaret() const;
  bool ShouldPaintDragCaret() const;
  bool ShouldPaintCarets() const {
    return ShouldPaintCursorCaret() || ShouldPaintDragCaret();
  }

  // Returns true when associated fragment of |layout_object| has line box.
  static bool TryMarkFirstLineBoxDirtyFor(const LayoutObject& layout_object);
  static bool TryMarkLastLineBoxDirtyFor(const LayoutObject& layout_object);

  // A range of fragments for |FragmentsFor()|.
  class CORE_EXPORT FragmentRange {
   public:
    explicit FragmentRange(
        NGPaintFragment* first,
        bool is_in_layout_ng_inline_formatting_context = true)
        : first_(first),
          is_in_layout_ng_inline_formatting_context_(
              is_in_layout_ng_inline_formatting_context) {}

    bool IsInLayoutNGInlineFormattingContext() const {
      return is_in_layout_ng_inline_formatting_context_;
    }

    bool IsEmpty() const { return !first_; }

    class iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
     public:
      explicit iterator(NGPaintFragment* first) : current_(first) {}

      NGPaintFragment* operator*() const { return current_; }
      NGPaintFragment* operator->() const { return current_; }
      iterator& operator++() {
        CHECK(current_);
        current_ = current_->next_for_same_layout_object_;
        return *this;
      }
      bool operator==(const iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const iterator& other) const {
        return current_ != other.current_;
      }

     private:
      NGPaintFragment* current_;
    };

    iterator begin() const { return iterator(first_); }
    iterator end() const { return iterator(nullptr); }

    // Returns the first |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |front()| for empty range.
    NGPaintFragment& front() const;

    // Returns the last |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |back()| for empty range.
    // Note: The complexity of |back()| is O(n) where n is number of elements
    // in this |FragmentRange|.
    NGPaintFragment& back() const;

    // Returns number of fragments in this range. The complexity is O(n) where n
    // is number of elements.
    wtf_size_t size() const;

   private:
    NGPaintFragment* first_;
    bool is_in_layout_ng_inline_formatting_context_;
  };

  // Returns NGPaintFragment for the inline formatting context the LayoutObject
  // belongs to.
  //
  // When the LayoutObject is an inline block, it belongs to an inline
  // formatting context while it creates its own for its descendants. This
  // function always returns the one it belongs to.
  static NGPaintFragment* GetForInlineContainer(const LayoutObject*);

  // Returns a range of NGPaintFragment in an inline formatting context that are
  // for a LayoutObject.
  static FragmentRange InlineFragmentsFor(const LayoutObject*);

  NGPaintFragment* LastForSameLayoutObject();

  // Called when lines containing |child| is dirty.
  static void DirtyLinesFromChangedChild(LayoutObject* child);

  // Mark this line box was changed, in order to re-use part of an inline
  // formatting context.
  void MarkLineBoxDirty();

  // Computes LocalVisualRect for an inline LayoutObject in the
  // LayoutObject::LocalVisualRect semantics; i.e., physical coordinates with
  // flipped block-flow direction. See layout/README.md for the coordinate
  // spaces.
  // Returns false if the LayoutObject is not in LayoutNG inline formatting
  // context.
  static bool FlippedLocalVisualRectFor(const LayoutObject*, LayoutRect*);

 private:
  static scoped_refptr<NGPaintFragment> CreateOrReuse(
      scoped_refptr<const NGPhysicalFragment> fragment,
      NGPhysicalOffset offset,
      NGPaintFragment* parent,
      scoped_refptr<NGPaintFragment> previous_instance,
      bool* populate_children);

  void PopulateDescendants(
      const NGPhysicalOffset inline_offset_to_container_box,
      HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map);
  void AssociateWithLayoutObject(
      LayoutObject*,
      HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map);

  // Helps for PositionForPoint() when |this| falls in different categories.
  PositionWithAffinity PositionForPointInText(const NGPhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineFormattingContext(
      const NGPhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineLevelBox(
      const NGPhysicalOffset&) const;

  // Dirty line boxes containing |layout_object|.
  static void MarkLineBoxesDirtyFor(const LayoutObject& layout_object);

  //
  // Following fields are computed in the layout phase.
  //

  scoped_refptr<const NGPhysicalFragment> physical_fragment_;
  NGPhysicalOffset offset_;

  NGPaintFragment* parent_;
  scoped_refptr<NGPaintFragment> first_child_;
  scoped_refptr<NGPaintFragment> next_sibling_;

  // The next fragment for when this is fragmented.
  scoped_refptr<NGPaintFragment> next_fragmented_;

  NGPaintFragment* next_for_same_layout_object_ = nullptr;
  NGPhysicalOffset inline_offset_to_container_box_;

  // For a line box, this indicates it is dirty. This helps to determine if the
  // fragment is re-usable when part of an inline formatting context is changed.
  // For an inline box, this flag helps to avoid traversing up to its line box
  // every time.
  unsigned is_dirty_inline_ : 1;

  //
  // Following fields are computed in the pre-paint phase.
  //

  LayoutRect visual_rect_;
  LayoutRect selection_visual_rect_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextSibling>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
