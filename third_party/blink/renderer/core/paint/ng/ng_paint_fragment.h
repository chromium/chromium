// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_

#include <iterator>
#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGBlockBreakToken;
class NGInlineCursor;
struct LayoutSelectionStatus;
struct NGContainerInkOverflow;
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
                                    public DisplayItemClient {
 public:
  NGPaintFragment(scoped_refptr<const NGPhysicalFragment>,
                  PhysicalOffset offset,
                  NGPaintFragment*);
  ~NGPaintFragment() override;

  static scoped_refptr<NGPaintFragment> Create(
      scoped_refptr<const NGPhysicalFragment>,
      const NGBlockBreakToken* break_token,
      scoped_refptr<NGPaintFragment> previous_instance = nullptr);

  const NGPhysicalFragment& PhysicalFragment() const {
    CHECK(IsAlive());
    return *physical_fragment_;
  }

  static scoped_refptr<NGPaintFragment>* Find(scoped_refptr<NGPaintFragment>*,
                                              const NGBlockBreakToken*);

  template <typename Traverse>
  class List {
    STACK_ALLOCATED();

   public:
    explicit List(NGPaintFragment* first) : first_(first) {}

    class iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
     public:
      explicit iterator(NGPaintFragment* first) : current_(first) {}

      NGPaintFragment* operator*() const { return current_; }
      NGPaintFragment* operator->() const { return current_; }
      iterator& operator++() {
        DCHECK(current_);
        current_ = Traverse::Next(current_);
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

    CORE_EXPORT iterator begin() const { return iterator(first_); }
    CORE_EXPORT iterator end() const { return iterator(nullptr); }

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
    CORE_EXPORT bool IsEmpty() const { return !first_; }

    void ToList(Vector<NGPaintFragment*, 16>*) const;

   private:
    NGPaintFragment* first_;
  };

  class TraverseNextSibling {
    STATIC_ONLY(TraverseNextSibling);

   public:
    static NGPaintFragment* Next(NGPaintFragment* current) {
      return current->NextSibling();
    }
  };
  using ChildList = List<TraverseNextSibling>;

  // The parent NGPaintFragment. This is nullptr for a root; i.e., when parent
  // is not for NGPaint. In the first phase, this means that this is a root of
  // an inline formatting context.
  NGPaintFragment* Parent() const { return parent_; }
  NGPaintFragment* FirstChild() const { return FirstAlive(first_child_.get()); }
  NGPaintFragment* NextSibling() const {
    return FirstAlive(next_sibling_.get());
  }
  NGPaintFragment* NextForSameLayoutObject() const {
    return next_for_same_layout_object_;
  }
  ChildList Children() const { return ChildList(FirstChild()); }
  bool IsEllipsis() const;

  // Note, as the name implies, |IsDescendantOfNotSelf| returns false for the
  // same object. This is different from |LayoutObject::IsDescendant| but is
  // same as |Node::IsDescendant|.
  bool IsDescendantOfNotSelf(const NGPaintFragment&) const;

  // Returns the root box containing this.
  const NGPaintFragment* Root() const;

  // Returns the first line box for a block-level container.
  NGPaintFragment* FirstLineBox() const;

  // Returns the container line box for inline fragments.
  const NGPaintFragment* ContainerLineBox() const;

  // Returns true if this fragment is line box and marked dirty.
  bool IsDirty() const { return is_dirty_inline_; }

  // Returns offset to its container box for inline and line box fragments.
  const PhysicalOffset& OffsetInContainerBlock() const {
    DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
    return inline_offset_to_container_box_;
  }
  const PhysicalRect RectInContainerBlock() const {
    return PhysicalRect(OffsetInContainerBlock(), Size());
  }

  // InkOverflow of itself, not including contents, in the local coordinate.
  PhysicalRect SelfInkOverflow() const;

  // InkOverflow of itself, including contents if they contribute to the ink
  // overflow of this object (e.g. when not clipped,) in the local coordinate.
  PhysicalRect InkOverflow() const;

  void RecalcInlineChildrenInkOverflow() const;

  // TODO(layout-dev): Implement when we have oveflow support.
  // TODO(eae): Switch to using NG geometry types.
  bool HasNonVisibleOverflow() const {
    return PhysicalFragment().HasNonVisibleOverflow();
  }

  bool IsScrollContainer() const {
    return PhysicalFragment().IsScrollContainer();
  }
  bool ShouldClipOverflow() const;
  bool HasSelfPaintingLayer() const;

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject.
  void SetShouldDoFullPaintInvalidation();

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject
  // recursively.
  void SetShouldDoFullPaintInvalidationRecursively();

  // Set ShouldDoFullPaintInvalidation flag to all objects in the first line of
  // this block-level fragment.
  void SetShouldDoFullPaintInvalidationForFirstLine() const;

  // DisplayItemClient methods.
  String DebugName() const override;

  // Commonly used functions for NGPhysicalFragment.
  Node* GetNode() const { return PhysicalFragment().GetNode(); }
  const LayoutObject* GetLayoutObject() const {
    return PhysicalFragment().GetLayoutObject();
  }
  LayoutObject* GetMutableLayoutObject() const {
    return PhysicalFragment().GetMutableLayoutObject();
  }
  const ComputedStyle& Style() const { return PhysicalFragment().Style(); }
  PhysicalOffset Offset() const {
    DCHECK(parent_);
    return offset_;
  }
  PhysicalSize Size() const { return PhysicalFragment().Size(); }
  PhysicalRect Rect() const { return {Offset(), Size()}; }

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree.
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const;

  // A range of fragments for |FragmentsFor()|.
  class TraverseNextForSameLayoutObject {
    STATIC_ONLY(TraverseNextForSameLayoutObject);

   public:
    static NGPaintFragment* Next(NGPaintFragment* current) {
      return current->next_for_same_layout_object_;
    }
  };
  class CORE_EXPORT FragmentRange
      : public List<TraverseNextForSameLayoutObject> {
   public:
    explicit FragmentRange(
        NGPaintFragment* first,
        bool is_in_layout_ng_inline_formatting_context = true)
        : List(first),
          is_in_layout_ng_inline_formatting_context_(
              is_in_layout_ng_inline_formatting_context) {}

    bool IsInLayoutNGInlineFormattingContext() const {
      return is_in_layout_ng_inline_formatting_context_;
    }

   private:
    bool is_in_layout_ng_inline_formatting_context_;
  };

  // Returns a range of NGPaintFragment in an inline formatting context that are
  // for a LayoutObject.
  static FragmentRange InlineFragmentsFor(const LayoutObject*);

  const NGPaintFragment* LastForSameLayoutObject() const;
  NGPaintFragment* LastForSameLayoutObject();
  void LayoutObjectWillBeDestroyed();

  void ClearAssociationWithLayoutObject();

  // Computes LocalVisualRect for an inline LayoutObject. Returns nullopt if the
  // LayoutObject is not in LayoutNG inline formatting context.
  static base::Optional<PhysicalRect> LocalVisualRectFor(const LayoutObject&);

 private:
  bool IsAlive() const { return !is_layout_object_destroyed_; }

  // Returns the first "alive" fragment; i.e., fragment that doesn't have
  // destroyed LayoutObject.
  static NGPaintFragment* FirstAlive(NGPaintFragment* fragment) {
    while (UNLIKELY(fragment && !fragment->IsAlive()))
      fragment = fragment->next_sibling_.get();
    return fragment;
  }

  struct CreateContext {
    STACK_ALLOCATED();

   public:
    CreateContext(scoped_refptr<NGPaintFragment> previous_instance,
                  bool populate_children)
        : previous_instance(std::move(previous_instance)),
          populate_children(populate_children) {}
    CreateContext(CreateContext* parent_context, NGPaintFragment* parent)
        : parent(parent),
          last_fragment_map(parent_context->last_fragment_map),
          previous_instance(std::move(parent->first_child_)) {}

    void SkipDestroyedPreviousInstances();
    void DestroyPreviousInstances();

    NGPaintFragment* parent = nullptr;
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map = nullptr;
    scoped_refptr<NGPaintFragment> previous_instance;
    bool populate_children = false;
    bool painting_layer_needs_repaint = false;
  };
  static scoped_refptr<NGPaintFragment> CreateOrReuse(
      scoped_refptr<const NGPhysicalFragment> fragment,
      PhysicalOffset offset,
      CreateContext* context);

  void PopulateDescendants(CreateContext* parent_context);
  void AssociateWithLayoutObject(
      LayoutObject*,
      HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map);

  static void DestroyAll(scoped_refptr<NGPaintFragment> fragment);
  void RemoveChildren();

  // Helps for PositionForPoint() when |this| falls in different categories.
  PositionWithAffinity PositionForPointInText(const PhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineFormattingContext(
      const PhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineLevelBox(
      const PhysicalOffset&) const;

  // Dirty line boxes containing |layout_object|.
  static void MarkLineBoxesDirtyFor(const LayoutObject& layout_object);

  // Returns |LayoutBox| that holds ink overflow for this fragment.
  const LayoutBox* InkOverflowOwnerBox() const;

  // Re-compute ink overflow of children and return the union.
  PhysicalRect RecalcInkOverflow();
  PhysicalRect RecalcContentsInkOverflow() const;

  //
  // Following fields are computed in the layout phase.
  //

  scoped_refptr<const NGPhysicalFragment> physical_fragment_;
  // The offset to |parent_| comes from |NGLink::Offset()|.
  PhysicalOffset offset_;

  NGPaintFragment* parent_;
  scoped_refptr<NGPaintFragment> first_child_;
  scoped_refptr<NGPaintFragment> next_sibling_;

  // The next fragment for when this is fragmented.
  scoped_refptr<NGPaintFragment> next_fragmented_;

  NGPaintFragment* next_for_same_layout_object_ = nullptr;
  PhysicalOffset inline_offset_to_container_box_;

  // The ink overflow storage for when |InkOverflowOwnerBox()| is nullptr.
  std::unique_ptr<NGContainerInkOverflow> ink_overflow_;

  // Set when the corresponding LayoutObject is destroyed.
  // TODO(kojii): This should move to |NGPhysicalFragment|.
  unsigned is_layout_object_destroyed_ : 1;

  // For a line box, this indicates it is dirty. This helps to determine if the
  // fragment is re-usable when part of an inline formatting context is changed.
  // For an inline box, this flag helps to avoid traversing up to its line box
  // every time.
  unsigned is_dirty_inline_ : 1;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextForSameLayoutObject>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextSibling>;

PhysicalRect ComputeLocalSelectionRectForText(
    const NGInlineCursor& cursor,
    const LayoutSelectionStatus& selection_status);

PhysicalRect ComputeLocalSelectionRectForReplaced(const NGInlineCursor& cursor);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
