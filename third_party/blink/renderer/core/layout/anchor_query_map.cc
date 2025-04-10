// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_query_map.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/logical_fragment_link.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

namespace {

LogicalFragmentLink GetFragmentainerLink(
    const LogicalFragmentLink& fragmentation_context_root_child) {
  // Multicol:
  if (fragmentation_context_root_child->IsColumnBox()) {
    return fragmentation_context_root_child;
  }
  // Pagination:
  if (fragmentation_context_root_child->GetBoxType() ==
      PhysicalFragment::kPageContainer) {
    // The fragmentainer is the page area, which is wrapped inside a page border
    // box fragment inside a page container fragment. Offsets don't matter here,
    // as each page establishes its own coordinate system.
    const PhysicalBoxFragment& page_area = GetPageArea(GetPageBorderBox(
        *To<PhysicalBoxFragment>(fragmentation_context_root_child.get())));
    return LogicalFragmentLink(page_area, LogicalOffset());
  }

  return LogicalFragmentLink();
}

// Represents a fragmentainer. This is in the logical coordinate system
// for convenience reasons.
struct FragmentainerContext {
  STACK_ALLOCATED();

 public:
  LogicalOffset offset;
  // The block offset when all fragments are stitched in the block direction.
  // That is, the sum of block offsets of preceding fragments.
  LayoutUnit stitched_offset;
  WritingModeConverter converter;
};

// This struct is a variation of |AnchorReference|, using the stitched
// coordinate system for the block-fragmented out-of-flow positioned objects.
struct StitchedAnchorReference
    : public GarbageCollected<StitchedAnchorReference> {
  StitchedAnchorReference(const Element& element,
                          const LogicalRect& rect,
                          const FragmentainerContext& fragmentainer)
      : element(&element),
        rect_in_first_fragmentainer(rect),
        first_fragmentainer_offset(fragmentainer.offset),
        first_fragmentainer_stitched_offset(fragmentainer.stitched_offset) {}

  LogicalRect StitchedRect() const {
    LogicalRect stitched_rect = rect_in_first_fragmentainer;
    stitched_rect.offset.block_offset += first_fragmentainer_stitched_offset;
    return stitched_rect;
  }

  PhysicalAnchorReference* GetStitchedAnchorReference(
      const WritingModeConverter& converter) const {
    PhysicalRect physical_rect = converter.ToPhysical(StitchedRect());

    return MakeGarbageCollected<PhysicalAnchorReference>(
        *element, physical_rect, /* is_out_of_flow */ false, nullptr);
  }

  void Unite(const LogicalRect& other_rect,
             const LogicalOffset& fragmentainer_offset) {
    // To unite fragments in the physical coordinate system as defined in the
    // spec while keeping the |reference.rect| relative to the first
    // fragmentainer, make the |fragmentainer_offset| relative to the first
    // fragmentainer.
    const LogicalRect other_rect_in_first_fragmentainer =
        other_rect + (fragmentainer_offset - first_fragmentainer_offset);
    rect_in_first_fragmentainer.Unite(other_rect_in_first_fragmentainer);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(element); }

  Member<const Element> element;
  // The |rect_in_first_fragmentainer| is relative to the first fragmentainer,
  // so that it can a) unite following fragments in the physical coordinate
  // system, and b) compute the result in the stitched coordinate system.
  LogicalRect rect_in_first_fragmentainer;
  LogicalOffset first_fragmentainer_offset;
  // The block offset when all fragments are stitched in the block direction.
  LayoutUnit first_fragmentainer_stitched_offset;
};

// This creates anchor queries in the stitched coordinate system. The result
// can be converted to a |PhysicalAnchorQuery|.
struct StitchedAnchorQuery : public GarbageCollected<StitchedAnchorQuery>,
                             public AnchorQueryBase<StitchedAnchorReference> {
  using Base = AnchorQueryBase<StitchedAnchorReference>;

  // Convert |this| to a |PhysicalAnchorQuery|. The result is a regular
  // |PhysicalAnchorQuery| except that its coordinate system is stitched
  // (i.e., as if they weren't fragmented.)
  PhysicalAnchorQuery* GetStitchedAnchorQuery(
      const WritingModeConverter& converter) const {
    auto* anchor_query = MakeGarbageCollected<PhysicalAnchorQuery>();
    for (const auto entry : *this) {
      anchor_query->Set(entry.key,
                        entry.value->GetStitchedAnchorReference(converter));
    }
    return anchor_query;
  }

  enum class Conflict {
    // The last entry wins. The calls must be in the tree order.
    kLastInCallOrder,
    // Overwrite existing entry if the new one is before the existing one.
    kOverwriteIfAfter,
  };

  void AddAnchorQuery(const PhysicalFragment& fragment,
                      const PhysicalOffset& offset_from_fragmentainer,
                      const FragmentainerContext& fragmentainer) {
    const PhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;
    for (auto entry : *anchor_query) {
      DCHECK(entry.value->GetLayoutObject());
      AddAnchorReference(entry.key, *entry.value->GetLayoutObject(),
                         entry.value->rect + offset_from_fragmentainer,
                         fragmentainer, Conflict::kLastInCallOrder);
    }
  }

  void AddAnchorReference(const AnchorKey& key,
                          const LayoutObject& new_object,
                          const PhysicalRect& physical_rect_in_fragmentainer,
                          const FragmentainerContext& fragmentainer,
                          Conflict conflict) {
    const LogicalRect rect_in_fragmentainer =
        fragmentainer.converter.ToLogical(physical_rect_in_fragmentainer);
    auto* new_value = MakeGarbageCollected<StitchedAnchorReference>(
        *To<Element>(new_object.GetNode()), rect_in_fragmentainer,
        fragmentainer);
    const auto result = Base::insert(key, new_value);
    if (result.is_new_entry)
      return;

    // If this is a fragment of the existing box, unite it with other fragments.
    StitchedAnchorReference* existing = *result.stored_value;
    const Element* existing_element = existing->element;
    const LayoutObject* existing_object = existing_element->GetLayoutObject();
    DCHECK(existing_object);
    if (existing_element == new_object.GetNode()) {
      existing->Unite(rect_in_fragmentainer, fragmentainer.offset);
      return;
    }

    // If this is the same anchor-name on a different box, the last one in the
    // pre-order wins. Normally, the call order is in the layout-order, which is
    // pre-order of the box tree. But OOFs may be laid out later, check the tree
    // order in such case.
    switch (conflict) {
      case Conflict::kLastInCallOrder:
        DCHECK(existing_object->IsBeforeInPreOrder(new_object));
        *existing = *new_value;
        break;
      case Conflict::kOverwriteIfAfter:
        if (!new_object.IsBeforeInPreOrder(*existing_object)) {
          *existing = *new_value;
        }
        break;
    }
  }
};

// This collects |StitchedAnchorQuery| for each containing block.
struct StitchedAnchorQueryCollector {
  STACK_ALLOCATED();

 public:
  StitchedAnchorQueryCollector(const LayoutBox& root,
                               const HeapHashSet<Member<const LayoutObject>>&
                                   anchored_oof_containers_and_ancestors)
      : anchored_oof_containers_and_ancestors_(
            anchored_oof_containers_and_ancestors),
        root_(root) {}

  void AddFragmentainerChildren(base::span<const LogicalFragmentLink> children,
                                WritingDirectionMode writing_direction) {
    LayoutUnit fragmentainer_stitched_offset;
    for (const LogicalFragmentLink& candidate : children) {
      LogicalFragmentLink child = GetFragmentainerLink(candidate);
      if (!child) {
        // Skip the child if it isn't a fragmentainer. This may for instance be
        // a column spanner or a list item marker. They are are direct child
        // fragments of a multicol container fragment, but do not participate in
        // fragmentation.
        continue;
      }

      const FragmentainerContext fragmentainer{
          child.offset,
          fragmentainer_stitched_offset,
          {writing_direction, child->Size()}};
      AddChild(*child, /* offset_from_fragmentainer */ {}, fragmentainer);
      if (const auto* token = To<BlockBreakToken>(child->GetBreakToken())) {
        fragmentainer_stitched_offset = token->ConsumedBlockSize();
      }
    }
  }

  void AddChild(const PhysicalFragment& fragment,
                const PhysicalOffset& offset_from_fragmentainer,
                const FragmentainerContext& fragmentainer) {
    if (const auto* box = DynamicTo<PhysicalBoxFragment>(&fragment)) {
      AddBoxChild(*box, offset_from_fragmentainer, fragmentainer);
    }
  }

  void AddBoxChild(const PhysicalBoxFragment& fragment,
                   const PhysicalOffset& offset_from_fragmentainer,
                   const FragmentainerContext& fragmentainer) {
    if (fragment.IsOutOfFlowPositioned()) {
      AddOutOfFlowChild(fragment, offset_from_fragmentainer, fragmentainer);
      return;
    }

    // Return early if the |fragment| doesn't have any anchors. No need to
    // traverse descendants.
    const PhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;

    // Create |StitchedAnchorQuery| if this is a containing block.
    if (const LayoutObject* layout_object = fragment.GetLayoutObject()) {
      if (!anchored_oof_containers_and_ancestors_.Contains(layout_object))
        return;
      if (layout_object->CanContainAbsolutePositionObjects() ||
          layout_object->CanContainFixedPositionObjects()) {
        EnsureStitchedAnchorQuery(*layout_object)
            .AddAnchorQuery(fragment, offset_from_fragmentainer, fragmentainer);
      }
    } else if (fragment.GetBoxType() == PhysicalFragment::kPageArea) {
      // The initial containing block (LayoutView) is on the outside of the
      // fragmentation context, so special attention is required when processing
      // page area fragmentainers.
      EnsureStitchedAnchorQuery(*fragment.OwnerLayoutBox())
          .AddAnchorQuery(fragment, offset_from_fragmentainer, fragmentainer);
    }

    if (fragment.IsFragmentationContextRoot()) {
      AddFragmentationContextRootChild(fragment, offset_from_fragmentainer,
                                       fragmentainer);
      return;
    }

    // Add inline children if any.
    if (const FragmentItems* items = fragment.Items()) {
      for (InlineCursor cursor(fragment, *items); cursor; cursor.MoveToNext()) {
        if (cursor.Current().IsInlineBox()) {
          DCHECK(cursor.Current().BoxFragment());
          AddBoxChild(*cursor.Current().BoxFragment(),
                      offset_from_fragmentainer +
                          cursor.Current()->OffsetInContainerFragment(),
                      fragmentainer);
        }
      }
    }

    // Add block children if any.
    for (const PhysicalFragmentLink& child : fragment.Children()) {
      DCHECK(!child->IsFragmentainerBox());
      const auto child_offset_from_fragmentainer =
          offset_from_fragmentainer + child.offset;
      AddChild(*child, child_offset_from_fragmentainer, fragmentainer);
    }
  }

  void AddFragmentationContextRootChild(
      const PhysicalBoxFragment& fragment,
      const PhysicalOffset& offset_from_fragmentainer,
      const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsFragmentationContextRoot());
    DCHECK(!fragment.Items());
    HeapVector<LogicalFragmentLink> children;
    for (const PhysicalFragmentLink& child : fragment.Children()) {
      const LogicalOffset child_offset =
          fragmentainer.converter.ToLogical(
              offset_from_fragmentainer + child.offset, child->Size()) +
          fragmentainer.offset;
      children.push_back(LogicalFragmentLink(*child.fragment, child_offset));
    }
    AddFragmentainerChildren(children,
                             fragmentainer.converter.GetWritingDirection());
  }

  void AddOutOfFlowChild(const PhysicalBoxFragment& fragment,
                         const PhysicalOffset& offset_from_fragmentainer,
                         const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsOutOfFlowPositioned());
    if (!fragment.HasAnchorQueryToPropagate()) {
      return;
    }
    // OOF fragments in block-fragmentation context are children of the
    // fragmentainers, but they should be added to anchor queries of their
    // containing block chain. Traverse the containing block chain and add
    // references to all |LayoutObject|, up to the |root_|.
    const LayoutObject* layout_object = fragment.GetLayoutObject();
    DCHECK(layout_object);
    LayoutObject::AncestorSkipInfo skip_info(&root_);
    const LayoutObject* containing_block = layout_object->Container(&skip_info);
    // If the OOF is to be laid out in the fragmentation context, its containing
    // block should be a descendant of the |root_|.
    DCHECK(containing_block);
    DCHECK_NE(containing_block, &root_);
    DCHECK(!skip_info.AncestorSkipped());
    // Skip the first containing block, because the spec defines "If el has the
    // same containing block as query el, el is not absolutely positioned." That
    // said, for absolutely positioned anchors should be invalid for the first
    // containing block.
    // https://drafts.csswg.org/css-anchor-1/#determining
    containing_block = containing_block->Container(&skip_info);
    while (containing_block && containing_block != root_ &&
           !skip_info.AncestorSkipped()) {
      StitchedAnchorQuery& query = EnsureStitchedAnchorQuery(*containing_block);
      if (fragment.IsExplicitAnchor()) {
        for (const ScopedCSSName* name :
             fragment.Style().AnchorName()->GetNames()) {
          AnchorScopedName* anchor_scoped_name =
              ToAnchorScopedName(*name, *layout_object);
          query.AddAnchorReference(
              anchor_scoped_name, *fragment.GetLayoutObject(),
              {offset_from_fragmentainer, fragment.Size()}, fragmentainer,
              StitchedAnchorQuery::Conflict::kOverwriteIfAfter);
        }
      }
      if (fragment.IsImplicitAnchor()) {
        query.AddAnchorReference(
            To<Element>(layout_object->GetNode()), *fragment.GetLayoutObject(),
            {offset_from_fragmentainer, fragment.Size()}, fragmentainer,
            StitchedAnchorQuery::Conflict::kOverwriteIfAfter);
      }
      query.AddAnchorQuery(fragment, offset_from_fragmentainer, fragmentainer);
      containing_block = containing_block->Container(&skip_info);
    }
  }

  StitchedAnchorQuery& EnsureStitchedAnchorQuery(
      const LayoutObject& containing_block) {
    const auto result = anchor_queries_.insert(
        &containing_block, MakeGarbageCollected<StitchedAnchorQuery>());
    DCHECK(result.stored_value->value);
    return *result.stored_value->value;
  }

  HeapHashMap<Member<const LayoutObject>, Member<StitchedAnchorQuery>>
      anchor_queries_;
  // The set of |LayoutObject| to traverse. When adding children, children not
  // in this set are skipped.
  const HeapHashSet<Member<const LayoutObject>>&
      anchored_oof_containers_and_ancestors_;
  const LayoutBox& root_;
};

}  // namespace

StitchedAnchorQueries::StitchedAnchorQueries(
    const LayoutBox& root_box,
    LogicalSize container_size,
    const LogicalFragmentLinkVector& children,
    WritingDirectionMode writing_direction)
    : root_box_(root_box),
      container_size_(container_size),
      writing_direction_(writing_direction) {
  DCHECK(&root_box);
  SetChildren(children);
}

void StitchedAnchorQueries::SetChildren(
    const LogicalFragmentLinkVector& children) {
  children_ = &children;

  // Invalidate the cache when children may have changed.
  computed_for_ = nullptr;

  // To allow early returns, check if any child has anchor queries.
  has_anchor_queries_ = false;
  for (const LogicalFragmentLink& child : children) {
    LogicalFragmentLink fragmentainer = GetFragmentainerLink(child);
    if (fragmentainer && fragmentainer->HasAnchorQuery()) {
      has_anchor_queries_ = true;
      break;
    }
  }
}

const PhysicalAnchorQuery* StitchedAnchorQueries::AnchorQuery(
    const LayoutObject& containing_block) const {
  DCHECK(&containing_block);
  DCHECK(containing_block.CanContainAbsolutePositionObjects() ||
         containing_block.CanContainFixedPositionObjects());

  if (!has_anchor_queries_) {
    return nullptr;
  }

  // Update |queries_| if it hasn't computed for |containing_block|.
  if (!computed_for_ || !computed_for_->IsDescendantOf(&containing_block))
    Update(containing_block);

  const auto& it = queries_.find(&containing_block);
  if (it != queries_.end()) {
    return it->value;
  }
  return nullptr;
}

// Update |queries_| for the given |layout_object| and its ancestors. This is
// `const`, modifies `mutable` caches only, so that other `const` functions such
// as |AnchorQuery| can call.
void StitchedAnchorQueries::Update(const LayoutObject& layout_object) const {
  // Compute descendants to collect anchor queries from. This helps reducing the
  // number of descendants to traverse.
  HeapHashSet<Member<const LayoutObject>> anchored_oof_containers_and_ancestors;
  for (const LayoutObject* runner = &layout_object;
       runner && runner != &root_box_; runner = runner->Parent()) {
    anchored_oof_containers_and_ancestors.insert(runner);
  }

  // Traverse descendants and collect anchor queries for each containing block.
  StitchedAnchorQueryCollector stitched_anchor_queries(
      root_box_, anchored_oof_containers_and_ancestors);
  stitched_anchor_queries.AddFragmentainerChildren(*children_,
                                                   writing_direction_);

  WritingModeConverter converter(writing_direction_, container_size_);

  // TODO(kojii): Currently this clears and rebuilds all anchor queries on
  // incremental updates. It may be possible to reduce the computation when
  // there are previous results.
  queries_.clear();
  for (const auto& it : stitched_anchor_queries.anchor_queries_) {
    const auto result =
        queries_.insert(it.key, it.value->GetStitchedAnchorQuery(converter));
    DCHECK(result.is_new_entry);
  }

  computed_for_ = &layout_object;
}

}  // namespace blink
