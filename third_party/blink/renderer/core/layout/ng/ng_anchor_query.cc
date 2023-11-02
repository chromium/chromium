// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_logical_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

namespace {

// Represents a fragmentainer. This is in the logical coordinate system
// because the size of the fragmentation context may not have determined yet.
// In that case, physical coordinates can't be computed yet.
struct FragmentainerContext {
  STACK_ALLOCATED();

 public:
  LogicalOffset offset;
  // The block offset when all fragments are stitched in the block direction.
  // That is, the sum of block offsets of preceding fragments.
  LayoutUnit stitched_offset;
  WritingModeConverter converter;
};

// This struct is a variation of |NGAnchorReference|, using the stitched
// coordinate system for the block-fragmented out-of-flow positioned objects.
struct NGStitchedAnchorReference
    : public GarbageCollected<NGStitchedAnchorReference> {
  NGStitchedAnchorReference(const NGPhysicalFragment& fragment_ref,
                            const LogicalRect& rect,
                            const FragmentainerContext& fragmentainer)
      : fragment(&fragment_ref),
        rect_in_first_fragmentainer(rect),
        first_fragmentainer_offset(fragmentainer.offset),
        first_fragmentainer_stitched_offset(fragmentainer.stitched_offset) {
    DCHECK(fragment);
  }

  LogicalRect StitchedRect() const {
    LogicalRect stitched_rect = rect_in_first_fragmentainer;
    stitched_rect.offset.block_offset += first_fragmentainer_stitched_offset;
    return stitched_rect;
  }

  NGLogicalAnchorReference* StitchedAnchorReference() const {
    DCHECK(fragment);
    return MakeGarbageCollected<NGLogicalAnchorReference>(
        *fragment, StitchedRect(), /* is_invalid */ false);
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

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }

  Member<const NGPhysicalFragment> fragment;
  // The |rect_in_first_fragmentainer| is relative to the first fragmentainer,
  // so that it can a) unite following fragments in the physical coordinate
  // system, and b) compute the result in the stitched coordinate system.
  LogicalRect rect_in_first_fragmentainer;
  LogicalOffset first_fragmentainer_offset;
  // The block offset when all fragments are stitched in the block direction.
  LayoutUnit first_fragmentainer_stitched_offset;
};

// This creates anchor queries in the stitched coordinate system. The result
// can be converted to a |NGLogicalAnchorQuery|.
struct NGStitchedAnchorQuery : public GarbageCollected<NGStitchedAnchorQuery> {
  // Convert |this| to a |NGLogicalAnchorQuery|. The result is a regular
  // |NGLogicalAnchorQuery| except that its coordinate system is stitched
  // (i.e., as if they weren't fragmented.)
  NGLogicalAnchorQuery* StitchedAnchorQuery() const {
    auto* anchor_query = MakeGarbageCollected<NGLogicalAnchorQuery>();
    for (const auto& it : references)
      anchor_query->Set(it.key, it.value->StitchedAnchorReference());
    return anchor_query;
  }

  enum class Conflict {
    // The first entry wins. The calls must be in the tree order.
    kFirstInCallOrder,
    // Overwrite existing entry if the new one is before the existing one.
    kOverwriteIfBefore,
  };

  void AddChild(const NGPhysicalFragment& fragment,
                const PhysicalOffset& offset_from_fragmentainer,
                const FragmentainerContext& fragmentainer) {
    const NGPhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;
    for (const auto& it : *anchor_query) {
      DCHECK(it.value->fragment);
      AddAnchorReference(it.key, *it.value->fragment,
                         it.value->rect + offset_from_fragmentainer,
                         fragmentainer, Conflict::kFirstInCallOrder);
    }
  }

  void AddAnchorReference(const AtomicString& anchor_name,
                          const NGPhysicalFragment& fragment,
                          const PhysicalRect& physical_rect_in_fragmentainer,
                          const FragmentainerContext& fragmentainer,
                          Conflict conflict) {
    const LogicalRect rect_in_fragmentainer =
        fragmentainer.converter.ToLogical(physical_rect_in_fragmentainer);
    auto* new_value = MakeGarbageCollected<NGStitchedAnchorReference>(
        fragment, rect_in_fragmentainer, fragmentainer);
    const auto result = references.insert(anchor_name, new_value);
    if (result.is_new_entry)
      return;

    // If this is a fragment of the existing box, unite it with other fragments.
    NGStitchedAnchorReference* existing = result.stored_value->value;
    const LayoutObject* existing_object = existing->fragment->GetLayoutObject();
    DCHECK(existing_object);
    const LayoutObject* new_object = new_value->fragment->GetLayoutObject();
    DCHECK(new_object);
    if (existing_object == new_object) {
      existing->Unite(rect_in_fragmentainer, fragmentainer.offset);
      return;
    }

    // If this is the same anchor-name on a different box, the first one in the
    // pre-order wins. Normally, the call order is in the layout-order, which is
    // pre-order of the box tree. But OOFs may be laid out later, check the tree
    // order in such case.
    switch (conflict) {
      case Conflict::kFirstInCallOrder:
        DCHECK(existing_object->IsBeforeInPreOrder(*new_object));
        break;
      case Conflict::kOverwriteIfBefore:
        if (new_object->IsBeforeInPreOrder(*existing_object))
          *existing = *new_value;
        break;
    }
  }

  void Trace(Visitor* visitor) const { visitor->Trace(references); }

  HeapHashMap<AtomicString, Member<NGStitchedAnchorReference>> references;
};

// This collects |NGStitchedAnchorQuery| for each containing block.
struct NGStitchedAnchorQueries {
  STACK_ALLOCATED();

 public:
  NGStitchedAnchorQueries(const LayoutBox& root,
                          const HeapHashSet<Member<const LayoutObject>>&
                              anchored_oof_containers_and_ancestors)
      : anchored_oof_containers_and_ancestors_(
            anchored_oof_containers_and_ancestors),
        root_(root) {}

  void AddFragmentainerChildren(base::span<const NGLogicalLink> children,
                                WritingDirectionMode writing_direction) {
    LayoutUnit fragmentainer_stitched_offset;
    for (const NGLogicalLink& child : children) {
      if (child->IsFragmentainerBox()) {
        const FragmentainerContext fragmentainer{
            child.offset,
            fragmentainer_stitched_offset,
            {writing_direction, child->Size()}};
        AddChild(*child, /* offset_from_fragmentainer */ {}, fragmentainer);
        fragmentainer_stitched_offset +=
            child->Size()
                .ConvertToLogical(writing_direction.GetWritingMode())
                .block_size;
        continue;
      }

      // The containing block of the spanner is the multicol container itself.
      // https://drafts.csswg.org/css-multicol/#column-span
      // So anchor queries in column spanners should not be added to any
      // containing blocks in the multicol.
      DCHECK(child->IsColumnSpanAll());
    }
  }

  void AddChild(const NGPhysicalFragment& fragment,
                const PhysicalOffset& offset_from_fragmentainer,
                const FragmentainerContext& fragmentainer) {
    if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(&fragment))
      AddBoxChild(*box, offset_from_fragmentainer, fragmentainer);
  }

  void AddBoxChild(const NGPhysicalBoxFragment& fragment,
                   const PhysicalOffset& offset_from_fragmentainer,
                   const FragmentainerContext& fragmentainer) {
    if (fragment.IsOutOfFlowPositioned()) {
      AddOutOfFlowChild(fragment, offset_from_fragmentainer, fragmentainer);
      return;
    }

    // Return early if the |fragment| doesn't have any anchors. No need to
    // traverse descendants.
    const NGPhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;

    // Create |NGStitchedAnchorQuery| if this is a containing block.
    if (const LayoutObject* layout_object = fragment.GetLayoutObject()) {
      if (!anchored_oof_containers_and_ancestors_.Contains(layout_object))
        return;
      if (layout_object->CanContainAbsolutePositionObjects() ||
          layout_object->CanContainFixedPositionObjects()) {
        EnsureStitchedAnchorQuery(*layout_object)
            .AddChild(fragment, offset_from_fragmentainer, fragmentainer);
      }
    }

    if (fragment.IsFragmentationContextRoot()) {
      AddFragmentationContextRootChild(fragment, offset_from_fragmentainer,
                                       fragmentainer);
      return;
    }

    // Add inline children if any.
    if (const NGFragmentItems* items = fragment.Items()) {
      for (NGInlineCursor cursor(fragment, *items); cursor;
           cursor.MoveToNext()) {
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
    for (const NGLink& child : fragment.Children()) {
      DCHECK(!child->IsFragmentainerBox());
      const auto child_offset_from_fragmentainer =
          offset_from_fragmentainer + child.offset;
      AddChild(*child, child_offset_from_fragmentainer, fragmentainer);
    }
  }

  void AddFragmentationContextRootChild(
      const NGPhysicalBoxFragment& fragment,
      const PhysicalOffset& offset_from_fragmentainer,
      const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsFragmentationContextRoot());
    DCHECK(!fragment.Items());
    HeapVector<NGLogicalLink> children;
    for (const NGLink& child : fragment.Children()) {
      const LogicalOffset child_offset =
          fragmentainer.converter.ToLogical(
              offset_from_fragmentainer + child.offset, child->Size()) +
          fragmentainer.offset;
      children.push_back(NGLogicalLink{child.fragment, child_offset});
    }
    AddFragmentainerChildren(children,
                             fragmentainer.converter.GetWritingDirection());
  }

  void AddOutOfFlowChild(const NGPhysicalBoxFragment& fragment,
                         const PhysicalOffset& offset_from_fragmentainer,
                         const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsOutOfFlowPositioned());
    const AtomicString anchor_name = fragment.Style().AnchorName();
    if (anchor_name.IsNull() && !fragment.AnchorQuery())
      return;

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
    // https://tabatkins.github.io/specs/css-anchor-position/#determining
    containing_block = containing_block->Container(&skip_info);
    while (containing_block && containing_block != root_ &&
           !skip_info.AncestorSkipped()) {
      NGStitchedAnchorQuery& query =
          EnsureStitchedAnchorQuery(*containing_block);
      if (!anchor_name.IsNull()) {
        query.AddAnchorReference(
            anchor_name, fragment, {offset_from_fragmentainer, fragment.Size()},
            fragmentainer, NGStitchedAnchorQuery::Conflict::kOverwriteIfBefore);
      }
      query.AddChild(fragment, offset_from_fragmentainer, fragmentainer);
      containing_block = containing_block->Container(&skip_info);
    }
  }

  NGStitchedAnchorQuery& EnsureStitchedAnchorQuery(
      const LayoutObject& containing_block) {
    const auto result = anchor_queries_.insert(
        &containing_block, MakeGarbageCollected<NGStitchedAnchorQuery>());
    DCHECK(result.stored_value->value);
    return *result.stored_value->value;
  }

  HeapHashMap<const LayoutObject*, Member<NGStitchedAnchorQuery>>
      anchor_queries_;
  // The set of |LayoutObject| to traverse. When adding children, children not
  // in this set are skipped.
  const HeapHashSet<Member<const LayoutObject>>&
      anchored_oof_containers_and_ancestors_;
  const LayoutBox& root_;
};

}  // namespace

NGPhysicalAnchorReference::NGPhysicalAnchorReference(
    const NGLogicalAnchorReference& logical_reference,
    const WritingModeConverter& converter)
    : rect(converter.ToPhysical(logical_reference.rect)),
      fragment(logical_reference.fragment),
      is_invalid(logical_reference.is_invalid) {}

void NGLogicalAnchorReference::InsertInPreOrderInto(
    Member<NGLogicalAnchorReference>* head_ptr) {
  const LayoutObject* const object = fragment->GetLayoutObject();
  for (;;) {
    NGLogicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->fragment->GetLayoutObject());
    if (!head ||
        object->IsBeforeInPreOrder(*head->fragment->GetLayoutObject())) {
      next = head;
      *head_ptr = this;
      break;
    }

    // Skip adding if there is a reference with the same validity status and is
    // before in the tree order. Only the first one in the tree order is needed
    // for each validity status.
    if (is_invalid == head->is_invalid)
      break;

    head_ptr = &head->next;
  }
}

// static
const NGLogicalAnchorQuery& NGLogicalAnchorQuery::Empty() {
  DEFINE_STATIC_LOCAL(Persistent<NGLogicalAnchorQuery>, empty,
                      (MakeGarbageCollected<NGLogicalAnchorQuery>()));
  return *empty;
}

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const AtomicString& name) const {
  const auto& it = anchor_references_.find(name);
  if (it != anchor_references_.end()) {
    const NGPhysicalAnchorReference& result = *it->value;
    if (!result.is_invalid)
      return &result;
  }
  return nullptr;
}

const PhysicalRect* NGPhysicalAnchorQuery::Rect(
    const AtomicString& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGPhysicalAnchorQuery::Fragment(
    const AtomicString& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment.Get();
  return nullptr;
}

const NGLogicalAnchorReference* NGLogicalAnchorQuery::AnchorReference(
    const AtomicString& name) const {
  const auto& it = anchor_references_.find(name);
  if (it != anchor_references_.end()) {
    for (const NGLogicalAnchorReference* result = it->value; result;
         result = result->next) {
      if (!result->is_invalid)
        return result;
    }
  }
  return nullptr;
}

const LogicalRect* NGLogicalAnchorQuery::Rect(const AtomicString& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGLogicalAnchorQuery::Fragment(
    const AtomicString& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment;
  return nullptr;
}

void NGLogicalAnchorQuery::Set(const AtomicString& name,
                               const NGPhysicalFragment& fragment,
                               const LogicalRect& rect,
                               SetOptions options) {
  DCHECK(fragment.GetLayoutObject());
  Set(name,
      MakeGarbageCollected<NGLogicalAnchorReference>(
          fragment, rect, options == SetOptions::kInvalid),
      options == SetOptions::kValidOutOfOrder);
}

void NGLogicalAnchorQuery::Set(const AtomicString& name,
                               NGLogicalAnchorReference* reference,
                               bool maybe_out_of_order) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = anchor_references_.insert(name, reference);
  if (result.is_new_entry)
    return;

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<NGLogicalAnchorReference>* const existing_head_ptr =
      &result.stored_value->value;
  NGLogicalAnchorReference* const existing_head = *existing_head_ptr;
  DCHECK(existing_head);
  const NGLogicalAnchorReference* last_valid_existing = nullptr;
  const LayoutObject* new_object = reference->fragment->GetLayoutObject();
  DCHECK(new_object);
  for (NGLogicalAnchorReference* existing = existing_head; existing;
       existing = existing->next) {
    const LayoutObject* existing_object = existing->fragment->GetLayoutObject();
    DCHECK(existing_object);
    if (existing_object == new_object) {
      existing->rect.Unite(reference->rect);
      return;
    }
    if (!existing->is_invalid)
      last_valid_existing = existing;
  }

  // Ignore the new value if both new and existing values are valid, and the
  // call order is in the tree order.
  if (!maybe_out_of_order && !reference->is_invalid && last_valid_existing) {
    DCHECK(last_valid_existing->fragment->GetLayoutObject()->IsBeforeInPreOrder(
        *new_object));
    return;
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the tree order.
  reference->InsertInPreOrderInto(existing_head_ptr);
}

void NGPhysicalAnchorQuery::SetFromLogical(
    const NGLogicalAnchorQuery& logical_query,
    const WritingModeConverter& converter) {
  // This function assumes |this| is empty on the entry. Merging multiple
  // references is not supported.
  DCHECK(IsEmpty());
  for (const auto& it : logical_query.anchor_references_) {
    // For each key, only the first one in the tree order, valid or invalid, is
    // needed to be propagated, because the validity is re-computed for each
    // containing block. Please see |SetFromPhysical|.
    const auto result = anchor_references_.Set(
        it.key,
        MakeGarbageCollected<NGPhysicalAnchorReference>(*it.value, converter));
    DCHECK(result.is_new_entry);
  }
}

void NGLogicalAnchorQuery::SetFromPhysical(
    const NGPhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset,
    SetOptions options) {
  for (const auto& it : physical_query.anchor_references_) {
    LogicalRect rect = converter.ToLogical(it.value->rect);
    rect.offset += additional_offset;
    Set(it.key,
        MakeGarbageCollected<NGLogicalAnchorReference>(
            *it.value->fragment, rect, options == SetOptions::kInvalid),
        options == SetOptions::kValidOutOfOrder);
  }
}

const NGLogicalAnchorQuery&
NGLogicalAnchorQueryForFragmentation::StitchedAnchorQuery(
    const LayoutObject& containing_block) const {
  DCHECK(&containing_block);
  DCHECK(containing_block.CanContainAbsolutePositionObjects() ||
         containing_block.CanContainFixedPositionObjects());
  const auto& it = queries_.find(&containing_block);
  if (it != queries_.end())
    return *it->value;
  return NGLogicalAnchorQuery::Empty();
}

void NGLogicalAnchorQueryForFragmentation::Update(
    const base::span<const NGLogicalLink>& children,
    const base::span<const NGLogicalOOFNodeForFragmentation>& oof_nodes,
    const LayoutBox& root,
    WritingDirectionMode writing_direction) {
  DCHECK(&root);

  has_anchors_on_oofs_ = false;
  for (const NGLogicalOOFNodeForFragmentation& oof_node : oof_nodes) {
    if (oof_node.box->MayHaveAnchorQuery()) {
      has_anchors_on_oofs_ = true;
      break;
    }
  }

  // Early return before expensive work if there are no anchor queries.
  bool has_anchor_queries = false;
  for (const NGLogicalLink& child : children) {
    if (child->AnchorQuery()) {
      has_anchor_queries = true;
      break;
    }
  }
  if (!has_anchor_queries) {
    queries_.clear();
    return;
  }

  // Compute descendants to collect anchor queries from. This helps reducing the
  // number of descendants to traverse.
  HeapHashSet<Member<const LayoutObject>> anchored_oof_containers_and_ancestors;
  for (const NGLogicalOOFNodeForFragmentation& oof_node : oof_nodes) {
    DCHECK(oof_node.box->IsOutOfFlowPositioned());
    // Only OOF nodes that have `anchor*()` functions are needed, but computing
    // it is not cheap. Adding unnecessary nodes is not expensive, because
    // |NGStitchedAnchorQueries| checks if the node has `AnchorQuery()` and
    // return early if not.
    for (const LayoutObject* parent = oof_node.box->Container();
         parent && parent != &root; parent = parent->Parent()) {
      const auto result = anchored_oof_containers_and_ancestors.insert(parent);
      if (!result.is_new_entry)
        break;
    }
  }

  // Traverse descendants and collect anchor queries for each containing block.
  NGStitchedAnchorQueries stitched_anchor_queries(
      root, anchored_oof_containers_and_ancestors);
  stitched_anchor_queries.AddFragmentainerChildren(children, writing_direction);

  // TODO(kojii): Currently this clears and rebuilds all anchor queries on
  // incremental updates. It may be possible to reduce the computation when
  // there are previous results.
  queries_.clear();
  for (const auto& it : stitched_anchor_queries.anchor_queries_) {
    const auto result =
        queries_.insert(it.key, it.value->StitchedAnchorQuery());
    DCHECK(result.is_new_entry);
  }
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const AtomicString& anchor_name,
    AnchorValue anchor_value,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
    const PhysicalOffset& offset_to_padding_box,
    bool is_y_axis,
    bool is_right_or_bottom) const {
  const NGLogicalAnchorReference* reference = AnchorReference(anchor_name);
  if (!reference)
    return absl::nullopt;  // No targets.

  const PhysicalRect anchor = container_converter.ToPhysical(reference->rect);
  LayoutUnit value;
  switch (anchor_value) {
    case AnchorValue::kLeft:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // Make the offset relative to the padding box, because the containing
      // block is formed by the padding edge.
      // https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
      value = anchor.X() - offset_to_padding_box.left;
      break;
    case AnchorValue::kRight:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Right() - offset_to_padding_box.left;
      break;
    case AnchorValue::kTop:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Y() - offset_to_padding_box.top;
      break;
    case AnchorValue::kBottom:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Bottom() - offset_to_padding_box.top;
      break;
    default:
      NOTREACHED();
      return absl::nullopt;
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_right_or_bottom)
    return available_size - value;
  return value;
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateSize(
    const AtomicString& anchor_name,
    AnchorSizeValue anchor_size_value,
    WritingMode container_writing_mode,
    WritingMode self_writing_mode) const {
  const NGLogicalAnchorReference* reference = AnchorReference(anchor_name);
  if (!reference)
    return absl::nullopt;  // No targets.

  const LogicalSize& anchor = reference->rect.size;
  switch (anchor_size_value) {
    case AnchorSizeValue::kInline:
      return anchor.inline_size;
    case AnchorSizeValue::kBlock:
      return anchor.block_size;
    case AnchorSizeValue::kWidth:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case AnchorSizeValue::kHeight:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
    case AnchorSizeValue::kSelfInline:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case AnchorSizeValue::kSelfBlock:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
  }
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchor(
    const AtomicString& anchor_name,
    AnchorValue anchor_value) const {
  has_anchor_functions_ = true;
  if (anchor_query_) {
    return anchor_query_->EvaluateAnchor(
        anchor_name, anchor_value, available_size_, container_converter_,
        offset_to_padding_box_, is_y_axis_, is_right_or_bottom_);
  }
  return absl::nullopt;
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchorSize(
    const AtomicString& anchor_name,
    AnchorSizeValue anchor_size_value) const {
  has_anchor_functions_ = true;
  if (anchor_query_) {
    return anchor_query_->EvaluateSize(anchor_name, anchor_size_value,
                                       container_converter_.GetWritingMode(),
                                       self_writing_mode_);
  }
  return absl::nullopt;
}

void NGLogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
  visitor->Trace(next);
}

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

void NGLogicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

void NGPhysicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

}  // namespace blink
