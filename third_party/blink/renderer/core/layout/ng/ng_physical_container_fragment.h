// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_

#include <iterator>

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGContainerFragmentBuilder;
class NGFragmentItem;
struct NGPhysicalOutOfFlowPositionedNode;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalContainerFragment : public NGPhysicalFragment {
 public:
  // Same as |base::span<const NGLink>|, except that:
  // * Each |NGLink| has the latest generation of post-layout. See
  //   |NGPhysicalFragment::UpdatedFragment()| for more details.
  // * The iterator skips fragments for destroyed or moved |LayoutObject|.
  class PostLayoutChildLinkList {
   public:
    PostLayoutChildLinkList(wtf_size_t count, const NGLink* buffer)
        : count_(count), buffer_(buffer) {}

    class ConstIterator
        : public std::iterator<std::input_iterator_tag, NGLink> {
      STACK_ALLOCATED();

     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = NGLink;
      using difference_type = ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;

      ConstIterator(const NGLink* current, wtf_size_t size)
          : current_(current), end_(current + size) {
        SkipInvalidAndSetPostLayout();
      }

      const NGLink& operator*() const { return post_layout_; }
      const NGLink* operator->() const { return &post_layout_; }

      ConstIterator& operator++() {
        ++current_;
        SkipInvalidAndSetPostLayout();
        return *this;
      }
      bool operator==(const ConstIterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const ConstIterator& other) const {
        return current_ != other.current_;
      }

     private:
      void SkipInvalidAndSetPostLayout() {
        for (; current_ != end_; ++current_) {
          const NGPhysicalFragment* fragment = current_->fragment;
          if (UNLIKELY(fragment->IsLayoutObjectDestroyedOrMoved()))
            continue;
          if (const NGPhysicalFragment* post_layout = fragment->PostLayout()) {
            post_layout_.fragment = post_layout;
            post_layout_.offset = current_->offset;
            return;
          }
        }
      }

      const NGLink* current_;
      const NGLink* end_;
      NGLink post_layout_;
    };
    using const_iterator = ConstIterator;

    const_iterator begin() const { return const_iterator(buffer_, count_); }
    const_iterator end() const { return const_iterator(buffer_ + count_, 0); }

    wtf_size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

   private:
    wtf_size_t count_;
    const NGLink* buffer_;
  };

  ~NGPhysicalContainerFragment();

  const NGBreakToken* BreakToken() const { return break_token_.get(); }

  // Returns the children of |this|.
  //
  // Note, children in this collection maybe old generations. Items in this
  // collection are safe, but their children (grandchildren of |this|) maybe
  // from deleted nodes or LayoutObjects. Also see |PostLayoutChildren()|.
  base::span<const NGLink> Children() const {
    return base::make_span(buffer_, num_children_);
  }

  // Similar to |Children()| but all children are the latest generation of
  // post-layout, and therefore all descendants are safe.
  PostLayoutChildLinkList PostLayoutChildren() const {
    return PostLayoutChildLinkList(num_children_, buffer_);
  }

  // Returns true if we have any floating descendants which need to be
  // traversed during the float paint phase.
  bool HasFloatingDescendantsForPaint() const {
    return has_floating_descendants_for_paint_;
  }

  // Returns true if we have any adjoining-object descendants (floats, or
  // inline-level OOF-positioned objects).
  bool HasAdjoiningObjectDescendants() const {
    return has_adjoining_object_descendants_;
  }

  // Returns true if we aren't able to re-use this fragment if the
  // |NGConstraintSpace::PercentageResolutionBlockSize| changes.
  bool DependsOnPercentageBlockSize() const {
    return depends_on_percentage_block_size_;
  }

  bool HasOutOfFlowPositionedDescendants() const {
    DCHECK(!oof_positioned_descendants_ ||
           !oof_positioned_descendants_->IsEmpty());
    return oof_positioned_descendants_.get();
  }

  base::span<NGPhysicalOutOfFlowPositionedNode> OutOfFlowPositionedDescendants()
      const {
    if (!HasOutOfFlowPositionedDescendants())
      return base::span<NGPhysicalOutOfFlowPositionedNode>();
    return {oof_positioned_descendants_->data(),
            oof_positioned_descendants_->size()};
  }

 protected:
  // block_or_line_writing_mode is used for converting the child offsets.
  NGPhysicalContainerFragment(NGContainerFragmentBuilder*,
                              WritingMode block_or_line_writing_mode,
                              NGLink* buffer,
                              NGFragmentType,
                              unsigned sub_type);

  void AddScrollableOverflowForInlineChild(
      const NGPhysicalBoxFragment& container,
      const ComputedStyle& container_style,
      const NGFragmentItem& line,
      bool has_hanging,
      const NGInlineCursor& cursor,
      TextHeightType height_type,
      PhysicalRect* overflow) const;

  static void AdjustScrollableOverflowForHanging(
      const PhysicalRect& rect,
      const WritingMode container_writing_mode,
      PhysicalRect* overflow);

  void AddOutlineRectsForNormalChildren(
      Vector<PhysicalRect>* outline_rects,
      const PhysicalOffset& additional_offset,
      NGOutlineType outline_type,
      const LayoutBoxModelObject* containing_block) const;
  void AddOutlineRectsForDescendant(
      const NGLink& descendant,
      Vector<PhysicalRect>* rects,
      const PhysicalOffset& additional_offset,
      NGOutlineType outline_type,
      const LayoutBoxModelObject* containing_block) const;

  static bool DependsOnPercentageBlockSize(const NGContainerFragmentBuilder&);

  wtf_size_t num_children_;
  scoped_refptr<const NGBreakToken> break_token_;
  const std::unique_ptr<Vector<NGPhysicalOutOfFlowPositionedNode>>
      oof_positioned_descendants_;

  // Because flexible arrays need to be the last member in a class, the actual
  // storage is in the subclass and we just keep a pointer to it here.
  const NGLink* buffer_;
};

template <>
struct DowncastTraits<NGPhysicalContainerFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.IsContainer();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_
