// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_

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
struct NGPhysicalOutOfFlowPositionedNode;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalContainerFragment : public NGPhysicalFragment {
 public:
  // Same as |base::span<const NGLink>|, except that each |NGLink| has the
  // latest generation of post-layout. See
  // |NGPhysicalFragment::UpdatedFragment()| for more details.
  class PostLayoutChildLinkList {
   public:
    PostLayoutChildLinkList(wtf_size_t count, const NGLink* buffer)
        : count_(count), buffer_(buffer) {}

    class ConstIterator {
      STACK_ALLOCATED();

     public:
      ConstIterator(const NGLink* current) : current_(current) {}

      const NGLink& operator*() const { return *PostLayoutOrCurrent(); }
      const NGLink* operator->() const { return PostLayoutOrCurrent(); }

      ConstIterator& operator++() {
        ++current_;
        return *this;
      }
      bool operator==(const ConstIterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const ConstIterator& other) const {
        return current_ != other.current_;
      }

     private:
      const NGLink* PostLayoutOrCurrent() const {
        post_layout_.fragment = current_->fragment->PostLayout();
        if (!post_layout_.fragment)
          return current_;
        post_layout_.offset = current_->offset;
        return &post_layout_;
      }

      const NGLink* current_;
      mutable NGLink post_layout_;
    };
    using const_iterator = ConstIterator;

    const_iterator begin() const { return const_iterator(buffer_); }
    const_iterator end() const { return const_iterator(buffer_ + count_); }

    const NGLink operator[](wtf_size_t idx) const {
      CHECK_LT(idx, count_);
      return buffer_[idx].PostLayout();
    }
    const NGLink front() const { return (*this)[0]; }
    const NGLink back() const { return (*this)[count_ - 1]; }

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

  bool HasOrthogonalFlowRoots() const { return has_orthogonal_flow_roots_; }

  // Returns true if we have a descendant within this formatting context, which
  // is potentially above our block-start edge.
  bool MayHaveDescendantAboveBlockStart() const {
    return may_have_descendant_above_block_start_;
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

  scoped_refptr<const NGBreakToken> break_token_;
  const std::unique_ptr<Vector<NGPhysicalOutOfFlowPositionedNode>>
      oof_positioned_descendants_;

  // Because flexible arrays need to be the last member in a class, the actual
  // storage is in the subclass and we just keep a pointer to it here.
  const NGLink* buffer_;
  wtf_size_t num_children_;
};

template <>
struct DowncastTraits<NGPhysicalContainerFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.IsContainer();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_CONTAINER_FRAGMENT_H_
