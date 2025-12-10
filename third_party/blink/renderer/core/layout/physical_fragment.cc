// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/fragment_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/scrollable_overflow_calculator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

struct SameSizeAsPhysicalFragment
    : public GarbageCollected<SameSizeAsPhysicalFragment> {
  Member<void*> layout_object;
  PhysicalSize size;
  uint8_t flags[4];
  Member<void*> members[3];
};

ASSERT_SIZE(PhysicalFragment, SameSizeAsPhysicalFragment);

String StringForBoxType(const PhysicalFragment& fragment) {
  StringBuilder result;
  switch (fragment.GetBoxType()) {
    case PhysicalFragment::BoxType::kNormalBox:
      break;
    case PhysicalFragment::BoxType::kInlineBox:
      result.Append("inline");
      break;
    case PhysicalFragment::BoxType::kColumnBox:
      result.Append("column");
      break;
    case PhysicalFragment::BoxType::kPageContainer:
      result.Append("page container");
      break;
    case PhysicalFragment::BoxType::kPageBorderBox:
      result.Append("page border box");
      break;
    case PhysicalFragment::BoxType::kPageMargin:
      result.Append("page margin");
      break;
    case PhysicalFragment::BoxType::kPageArea:
      result.Append("page area");
      break;
    case PhysicalFragment::BoxType::kAtomicInline:
      result.Append("atomic-inline");
      break;
    case PhysicalFragment::BoxType::kFloating:
      result.Append("floating");
      break;
    case PhysicalFragment::BoxType::kOutOfFlowPositioned:
      result.Append("out-of-flow-positioned");
      break;
    case PhysicalFragment::BoxType::kBlockFlowRoot:
      result.Append("block-flow-root");
      break;
    case PhysicalFragment::BoxType::kRenderedLegend:
      result.Append("rendered-legend");
      break;
  }
  if (fragment.IsBlockFlow()) {
    if (result.length())
      result.Append(" ");
    result.Append("block-flow");
  }
  if (fragment.IsFieldsetContainer()) {
    if (result.length())
      result.Append(" ");
    result.Append("fieldset-container");
  }
  if (fragment.IsBox() &&
      To<PhysicalBoxFragment>(fragment).IsInlineFormattingContext()) {
    if (result.length())
      result.Append(" ");
    result.Append("children-inline");
  }

  return result.ToString();
}

class FragmentTreeDumper {
  STACK_ALLOCATED();

 public:
  FragmentTreeDumper(StringBuilder* builder,
                     PhysicalFragment::DumpFlags flags,
                     const PhysicalFragment* target = nullptr)
      : builder_(builder), target_fragment_(target), flags_(flags) {}

  void Append(const PhysicalFragment* fragment,
              std::optional<PhysicalOffset> fragment_offset,
              unsigned indent = 2) {
    Vector<String> attributes;
    Append(fragment, fragment_offset, attributes, indent);
  }

  void Append(const PhysicalFragment* fragment,
              std::optional<PhysicalOffset> fragment_offset,
              Vector<String>& attributes,
              unsigned indent = 2) {
    AppendIndentation(indent, fragment);

    bool has_content = false;
    if (const auto* box = DynamicTo<PhysicalBoxFragment>(fragment)) {
      if (box->IsLayoutObjectDestroyedOrMoved()) {
        builder_->Append("DEAD LAYOUT OBJECT!\n");
        return;
      }
      const LayoutObject* layout_object = box->GetLayoutObject();
      if (flags_ & PhysicalFragment::DumpType) {
        builder_->Append("Box");
        String box_type = StringForBoxType(*fragment);
        has_content = true;
        if (!box_type.empty()) {
          attributes.push_back(box_type);
        }
        if (flags_ & PhysicalFragment::DumpSelfPainting &&
            box->HasSelfPaintingLayer()) {
          attributes.push_back("self paint");
        }
        if (flags_ & PhysicalFragment::DumpBreakInfo &&
            !box->IsFirstForNode()) {
          attributes.push_back("resumed");
        }
      }
      AppendAttributes(attributes);
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);

      if (flags_ & PhysicalFragment::DumpNodeName && layout_object) {
        if (has_content)
          builder_->Append(" ");
        builder_->Append(layout_object->DebugName());
        has_content = true;
      }

      if (flags_ & PhysicalFragment::DumpBreakInfo) {
        if (const BlockBreakToken* token = box->GetBreakToken()) {
          builder_->Append(token->ToString(/*skip_node_info=*/true));
          has_content = true;
        }
      }

      builder_->Append("\n");

      bool has_fragment_items = false;
      if (flags_ & PhysicalFragment::DumpItems) {
        if (const FragmentItems* fragment_items = box->Items()) {
          InlineCursor cursor(*box, *fragment_items);
          Append(&cursor, indent + 2);
          has_fragment_items = true;
        }
      }
      if (flags_ & PhysicalFragment::DumpSubtree) {
        for (auto& child : box->Children()) {
          if (has_fragment_items && child->IsLineBox())
            continue;
          Append(child.get(), child.Offset(), indent + 2);
        }
      }
      return;
    }

    if (fragment->IsLineBox()) {
      if (flags_ & PhysicalFragment::DumpType) {
        builder_->Append("LineBox");
        has_content = true;
      }
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);
      builder_->Append("\n");
      return;
    }

    if (flags_ & PhysicalFragment::DumpType) {
      builder_->Append("Unknown fragment type");
      has_content = true;
    }
    has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);
    builder_->Append("\n");
  }

  void AppendAttributes(const Vector<String>& attributes) {
    if (!attributes.empty()) {
      builder_->Append(" (");
      builder_->AppendRange(attributes, ")(");
      builder_->Append(")");
    }
  }

 private:
  void Append(InlineCursor* cursor, unsigned indent) {
    for (; *cursor; cursor->MoveToNextSkippingChildren()) {
      const InlineCursorPosition& current = cursor->Current();
      const PhysicalFragment* box = current.BoxFragment();
      if (box && !box->IsInlineBox()) {
        Vector<String> attributes;
        if (current->IsHiddenForPaint()) {
          attributes.push_back("hidden");
        }
        Append(box, current.OffsetInContainerFragment(), attributes, indent);
        continue;
      }

      if (!box)
        box = current.Item()->LineBoxFragment();
      AppendIndentation(indent, box);

      if (current.Item()->IsLayoutObjectDestroyedOrMoved()) {
        builder_->Append("DEAD LAYOUT OBJECT!\n");
        return;
      }

      // TODO(kojii): Use the same format as layout tree dump for now. We can
      // make this more similar to |AppendFragmentToString| above.
      builder_->Append(current->ToString());

      if (flags_ & PhysicalFragment::DumpOffset) {
        builder_->Append(" offset:");
        builder_->Append(current.OffsetInContainerFragment().ToString());
      }
      if (flags_ & PhysicalFragment::DumpSize) {
        builder_->Append(" size:");
        builder_->Append(current.Size().ToString());
      }

      builder_->Append("\n");

      if (flags_ & PhysicalFragment::DumpSubtree && current.HasChildren()) {
        InlineCursor descendants = cursor->CursorForDescendants();
        Append(&descendants, indent + 2);
      }
    }
  }

  bool AppendOffsetAndSize(const PhysicalFragment* fragment,
                           std::optional<PhysicalOffset> fragment_offset,
                           bool has_content) {
    if (flags_ & PhysicalFragment::DumpOffset) {
      if (has_content)
        builder_->Append(" ");
      builder_->Append("offset:");
      if (fragment_offset)
        builder_->Append(fragment_offset->ToString());
      else
        builder_->Append("unplaced");
      has_content = true;
    }
    if (flags_ & PhysicalFragment::DumpSize) {
      if (has_content)
        builder_->Append(" ");
      builder_->Append("size:");
      builder_->Append(fragment->Size().ToString());
      has_content = true;
    }
    return has_content;
  }

  void AppendIndentation(unsigned indent,
                         const PhysicalFragment* fragment = nullptr) {
    if (flags_ & PhysicalFragment::DumpIndentation) {
      unsigned start_idx = 0;
      if (fragment && fragment == target_fragment_) {
        builder_->Append("*");
        start_idx = 1;
        target_fragment_found_ = true;
      }
      for (unsigned i = start_idx; i < indent; i++)
        builder_->Append(" ");
    }
  }

  StringBuilder* builder_;
  const PhysicalFragment* target_fragment_ = nullptr;
  PhysicalFragment::DumpFlags flags_;
  bool target_fragment_found_ = false;
};

OofContainingBlock<PhysicalOffset> PhysicalContainingBlock(
    FragmentBuilder* builder,
    PhysicalSize outer_size,
    PhysicalSize inner_size,
    const OofContainingBlock<LogicalOffset>& containing_block) {
  return OofContainingBlock<PhysicalOffset>(
      containing_block.Offset().ConvertToPhysical(
          builder->Style().GetWritingDirection(), outer_size, inner_size),
      RelativeInsetToPhysical(containing_block.RelativeOffset(),
                              builder->Style().GetWritingDirection()),
      containing_block.Fragment(),
      containing_block.ClippedContainerBlockOffset(),
      containing_block.IsInsideColumnSpanner());
}

OofContainingBlock<PhysicalOffset> PhysicalContainingBlock(
    FragmentBuilder* builder,
    PhysicalSize size,
    const OofContainingBlock<LogicalOffset>& containing_block) {
  PhysicalSize containing_block_size =
      containing_block.Fragment() ? containing_block.Fragment()->Size() : size;
  return PhysicalContainingBlock(builder, size, containing_block_size,
                                 containing_block);
}

}  // namespace

PhysicalFragment::PhysicalFragment(FragmentBuilder* builder,
                                   WritingMode block_or_line_writing_mode,
                                   FragmentType type,
                                   unsigned sub_type)
    : layout_object_(builder->layout_object_),
      size_(ToPhysicalSize(builder->size_, builder->GetWritingMode())),
      type_(type),
      sub_type_(sub_type),
      style_variant_((unsigned)builder->style_variant_),
      is_hidden_for_paint_(builder->is_hidden_for_paint_),
      has_floating_descendants_for_paint_(false),
      has_running_anchor_transform_animation_(
          builder->has_running_anchor_transform_animation_),
      children_valid_(true),
      is_opaque_(builder->is_opaque_),
      is_block_in_inline_(builder->is_block_in_inline_),
      is_line_for_parallel_flow_(builder->is_line_for_parallel_flow_),
      may_have_descendant_above_block_start_(
          builder->may_have_descendant_above_block_start_),
      is_fieldset_container_(false),
      is_table_part_(false),
      is_painted_atomically_(false),
      has_collapsed_borders_(builder->has_collapsed_borders_),
      has_first_baseline_(false),
      has_last_baseline_(false),
      use_last_baseline_for_inline_baseline_(false),
      has_fragmented_out_of_flow_data_(
          !builder->oof_positioned_fragmentainer_descendants_.empty() ||
          !builder->multicols_with_pending_oofs_.empty()),
      has_out_of_flow_fragment_child_(builder->HasOutOfFlowFragmentChild()),
      has_out_of_flow_in_fragmentainer_subtree_(
          builder->HasOutOfFlowInFragmentainerSubtree()),
      propagated_data_((builder->sticky_descendants_ || builder->snap_areas_ ||
                        builder->scroll_start_target_ ||
                        builder->named_triggers_)
                           ? MakeGarbageCollected<PropagatedData>(
                                 builder->sticky_descendants_,
                                 builder->snap_areas_,
                                 builder->scroll_start_target_,
                                 builder->named_triggers_)
                           : nullptr),
      break_token_(std::move(builder->break_token_)),
      oof_data_(builder->oof_positioned_descendants_.empty() &&
                        !builder->GetAnchorMap() &&
                        !has_fragmented_out_of_flow_data_
                    ? nullptr
                    : OofDataFromBuilder(builder)) {
  CHECK(builder->layout_object_);

  // A line with a float / block in a parallel flow should not have an outgoing
  // break token associated. An outgoing inline break token from a line means
  // that it is to be resumed in the main flow of the container.
  DCHECK(!is_line_for_parallel_flow_ || !break_token_);

  has_floating_descendants_for_paint_ =
      builder->has_floating_descendants_for_paint_;
  has_adjoining_object_descendants_ =
      builder->has_adjoining_object_descendants_;
  depends_on_percentage_block_size_ = DependsOnPercentageBlockSize(*builder);
  children_valid_ = true;
}

// Even though the other constructors don't initialize many of these fields
// (instead set by their super-classes), the copy constructor does.
PhysicalFragment::PhysicalFragment(const PhysicalFragment& other)
    : layout_object_(other.layout_object_),
      size_(other.size_),
      type_(other.type_),
      sub_type_(other.sub_type_),
      style_variant_(other.style_variant_),
      is_hidden_for_paint_(other.is_hidden_for_paint_),
      has_floating_descendants_for_paint_(
          other.has_floating_descendants_for_paint_),
      has_adjoining_object_descendants_(
          other.has_adjoining_object_descendants_),
      depends_on_percentage_block_size_(
          other.depends_on_percentage_block_size_),
      children_valid_(other.children_valid_),
      has_propagated_descendants_(other.has_propagated_descendants_),
      has_hanging_(other.has_hanging_),
      is_opaque_(other.is_opaque_),
      is_block_in_inline_(other.is_block_in_inline_),
      is_line_for_parallel_flow_(other.is_line_for_parallel_flow_),
      is_math_fraction_(other.is_math_fraction_),
      is_math_operator_(other.is_math_operator_),
      may_have_descendant_above_block_start_(
          other.may_have_descendant_above_block_start_),
      is_fieldset_container_(other.is_fieldset_container_),
      is_table_part_(other.is_table_part_),
      is_painted_atomically_(other.is_painted_atomically_),
      has_collapsed_borders_(other.has_collapsed_borders_),
      has_first_baseline_(other.has_first_baseline_),
      has_last_baseline_(other.has_last_baseline_),
      use_last_baseline_for_inline_baseline_(
          other.use_last_baseline_for_inline_baseline_),
      has_fragmented_out_of_flow_data_(other.has_fragmented_out_of_flow_data_),
      has_out_of_flow_fragment_child_(other.has_out_of_flow_fragment_child_),
      has_out_of_flow_in_fragmentainer_subtree_(
          other.has_out_of_flow_in_fragmentainer_subtree_),
      base_direction_(other.base_direction_),
      propagated_data_(other.propagated_data_),
      break_token_(other.break_token_),
      oof_data_(other.oof_data_ ? other.CloneOofData() : nullptr) {
  CHECK(layout_object_);
  DCHECK(other.children_valid_);
  DCHECK(children_valid_);
}

bool PhysicalFragment::IsBlockFlow() const {
  return !IsLineBox() && layout_object_->IsLayoutBlockFlow();
}

bool PhysicalFragment::IsTextControlContainer() const {
  return IsCSSBox() && blink::IsTextControlContainer(layout_object_->GetNode());
}

bool PhysicalFragment::IsTextControlPlaceholder() const {
  return IsCSSBox() &&
         blink::IsTextControlPlaceholder(layout_object_->GetNode());
}

base::span<PhysicalOofPositionedNode>
PhysicalFragment::OutOfFlowPositionedDescendants() const {
  if (!HasOutOfFlowPositionedDescendants())
    return base::span<PhysicalOofPositionedNode>();
  return oof_data_->OofPositionedDescendants();
}

const FragmentedOofData* PhysicalFragment::GetFragmentedOofData() const {
  if (!has_fragmented_out_of_flow_data_)
    return nullptr;
  auto* oof_data = reinterpret_cast<FragmentedOofData*>(oof_data_.Get());
  DCHECK(!oof_data->multicols_with_pending_oofs.empty() ||
         !oof_data->oof_positioned_fragmentainer_descendants.empty());
  return oof_data;
}

bool PhysicalFragment::HasNestedMulticolsWithOOFs() const {
  const auto* oof_data = GetFragmentedOofData();
  return oof_data && !oof_data->multicols_with_pending_oofs.empty();
}

bool PhysicalFragment::NeedsOOFPositionedInfoPropagation() const {
  // If we have |oof_data_|, it should mean at least one of OOF propagation data
  // exists.
  DCHECK_EQ(!!oof_data_,
            HasOutOfFlowPositionedDescendants() || HasChildAnchors() ||
                (GetFragmentedOofData() &&
                 GetFragmentedOofData()->NeedsOOFPositionedInfoPropagation()));
  return !!oof_data_;
}

PhysicalFragment::OofData* PhysicalFragment::OofDataFromBuilder(
    FragmentBuilder* builder) {
  OofData* oof_data = nullptr;
  if (has_fragmented_out_of_flow_data_) {
    oof_data = FragmentedOofDataFromBuilder(builder);
  }

  const WritingModeConverter converter(
      {builder->Style().GetWritingMode(), builder->Direction()}, Size());

  if (!builder->oof_positioned_descendants_.empty()) {
    if (!oof_data) {
      oof_data = MakeGarbageCollected<OofData>();
    }
    oof_data->OofPositionedDescendants().reserve(
        builder->oof_positioned_descendants_.size());
    for (const LogicalOofPositionedNode& descendant :
         builder->oof_positioned_descendants_) {
      OofInlineContainer<PhysicalOffset> inline_container(
          descendant.inline_container.container,
          converter.ToPhysical(descendant.inline_container.relative_offset,
                               PhysicalSize()));
      oof_data->OofPositionedDescendants().emplace_back(
          descendant.Node(), descendant.break_token,
          descendant.static_position.ConvertToPhysical(converter),
          descendant.requires_content_before_breaking, inline_container);
    }
  }

  if (builder->anchor_map_) {
    if (!oof_data) {
      oof_data = MakeGarbageCollected<OofData>();
    }
    oof_data->SetAnchorMap(builder->anchor_map_);
  }

  return oof_data;
}

PhysicalFragment::OofData* PhysicalFragment::FragmentedOofDataFromBuilder(
    FragmentBuilder* builder) {
  DCHECK(has_fragmented_out_of_flow_data_);
  DCHECK_EQ(has_fragmented_out_of_flow_data_,
            !builder->oof_positioned_fragmentainer_descendants_.empty() ||
                !builder->multicols_with_pending_oofs_.empty());
  auto* fragmented_data = MakeGarbageCollected<FragmentedOofData>();
  fragmented_data->oof_positioned_fragmentainer_descendants.reserve(
      builder->oof_positioned_fragmentainer_descendants_.size());
  const PhysicalSize& size = Size();
  WritingDirectionMode writing_direction = builder->GetWritingDirection();
  const WritingModeConverter converter(writing_direction, size);
  for (const auto& descendant :
       builder->oof_positioned_fragmentainer_descendants_) {
    OofInlineContainer<PhysicalOffset> inline_container(
        descendant.inline_container.container,
        converter.ToPhysical(descendant.inline_container.relative_offset,
                             PhysicalSize()));
    OofInlineContainer<PhysicalOffset> fixedpos_inline_container(
        descendant.fixedpos_inline_container.container,
        converter.ToPhysical(
            descendant.fixedpos_inline_container.relative_offset,
            PhysicalSize()));

    // The static position should remain relative to the containing block.
    PhysicalSize containing_block_size =
        descendant.containing_block.Fragment()
            ? descendant.containing_block.Fragment()->Size()
            : size;
    const WritingModeConverter containing_block_converter(
        writing_direction, containing_block_size);

    fragmented_data->oof_positioned_fragmentainer_descendants.emplace_back(
        descendant.Node(),
        descendant.static_position.ConvertToPhysical(
            containing_block_converter),
        descendant.requires_content_before_breaking, inline_container,
        PhysicalContainingBlock(builder, size, containing_block_size,
                                descendant.containing_block),
        PhysicalContainingBlock(builder, size,
                                descendant.fixedpos_containing_block),
        fixedpos_inline_container);
  }
  for (const auto& multicol : builder->multicols_with_pending_oofs_) {
    auto& value = multicol.value;
    OofInlineContainer<PhysicalOffset> fixedpos_inline_container(
        value->fixedpos_inline_container.container,
        converter.ToPhysical(value->fixedpos_inline_container.relative_offset,
                             PhysicalSize()));
    fragmented_data->multicols_with_pending_oofs.insert(
        multicol.key,
        MakeGarbageCollected<MulticolWithPendingOofs<PhysicalOffset>>(
            value->multicol_offset.ConvertToPhysical(
                builder->Style().GetWritingDirection(), size, PhysicalSize()),
            PhysicalContainingBlock(builder, size,
                                    value->fixedpos_containing_block),
            fixedpos_inline_container));
  }
  return fragmented_data;
}

void PhysicalFragment::ClearOofData() {
  if (!oof_data_)
    return;
  if (HasChildAnchors()) {
    oof_data_->OofPositionedDescendants().clear();
  } else {
    oof_data_ = nullptr;
  }
}

PhysicalFragment::OofData* PhysicalFragment::CloneOofData() const {
  DCHECK(oof_data_);
  if (!has_fragmented_out_of_flow_data_)
    return MakeGarbageCollected<OofData>(*oof_data_);
  DCHECK(GetFragmentedOofData());
  return MakeGarbageCollected<FragmentedOofData>(*GetFragmentedOofData());
}

bool PhysicalFragment::IsMonolithic() const {
  // Line boxes are monolithic, except for line boxes that are just there to
  // contain a block inside an inline, in which case the anonymous block child
  // wrapper inside the line is breakable.
  if (IsLineBox())
    return !IsBlockInInline();
  if (const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(this)) {
    return box_fragment->IsMonolithic();
  }
  return false;
}

bool PhysicalFragment::IsImplicitAnchor() const {
  if (Element* element = DynamicTo<Element>(GetNode())) {
    return element->MayBeImplicitAnchor();
  }
  return false;
}

const FragmentData* PhysicalFragment::GetFragmentData() const {
  const LayoutBox* box = DynamicTo<LayoutBox>(GetLayoutObject());
  if (!box) {
    DCHECK(!GetLayoutObject());
    return nullptr;
  }
  return box->FragmentDataFromPhysicalFragment(To<PhysicalBoxFragment>(*this));
}

const PhysicalFragment* PhysicalFragment::PostLayout() const {
  if (const auto* box = DynamicTo<PhysicalBoxFragment>(this)) {
    return box->PostLayout();
  }
  return this;
}

#if DCHECK_IS_ON()
void PhysicalFragment::CheckType() const {
  switch (Type()) {
    case kFragmentBox:
      if (IsInlineBox()) {
        DCHECK(layout_object_->IsLayoutInline());
      } else {
        DCHECK(layout_object_->IsBox());
      }
      if (IsFragmentainerBox() || GetBoxType() == kPageContainer ||
          GetBoxType() == kPageBorderBox || GetBoxType() == kPageMargin) {
        // Fragmentainers are associated with the same layout object as their
        // multicol container (or the LayoutView, in case of printing). The
        // fragments themselves are regular in-flow block container fragments
        // for most purposes.
        DCHECK(layout_object_->IsLayoutBlockFlow());
        DCHECK(IsBox());
        DCHECK(!IsFloating());
        DCHECK(!IsOutOfFlowPositioned());
        DCHECK(!IsAtomicInline());
        DCHECK(!IsFormattingContextRoot());
        break;
      }
      if (layout_object_->IsLayoutOutsideListMarker()) {
        // List marker is an atomic inline if it appears in a line box, or a
        // block box.
        DCHECK(!IsFloating());
        DCHECK(!IsOutOfFlowPositioned());
        DCHECK(IsAtomicInline() || (IsBox() && GetBoxType() == kBlockFlowRoot));
        break;
      }
      DCHECK_EQ(IsFloating(), layout_object_->IsFloating());
      DCHECK_EQ(IsOutOfFlowPositioned(),
                layout_object_->IsOutOfFlowPositioned());
      DCHECK_EQ(IsAtomicInline(), layout_object_->IsInline() &&
                                      layout_object_->IsAtomicInlineLevel());
      break;
    case kFragmentLineBox:
      DCHECK(layout_object_->IsLayoutBlockFlow());
      DCHECK(!IsFloating());
      DCHECK(!IsOutOfFlowPositioned());
      DCHECK(!IsInlineBox());
      DCHECK(!IsAtomicInline());
      break;
  }
}
#endif

LogicalRect PhysicalFragment::ConvertChildToLogical(
    const PhysicalRect& physical_rect) const {
  return WritingModeConverter(Style().GetWritingDirection(), Size())
      .ToLogical(physical_rect);
}

String PhysicalFragment::ToString() const {
  StringBuilder output;
  output.AppendFormat("Type: '%d' Size: '%s'", Type(),
                      Size().ToString().Ascii().c_str());
  switch (Type()) {
    case kFragmentBox:
      output.AppendFormat(", BoxType: '%s'",
                          StringForBoxType(*this).Ascii().c_str());
      break;
    case kFragmentLineBox:
      break;
  }
  return output.ToString();
}

String PhysicalFragment::DumpFragmentTree(
    DumpFlags flags,
    const PhysicalFragment* target,
    std::optional<PhysicalOffset> fragment_offset,
    unsigned indent) const {
  StringBuilder string_builder;
  if (flags & DumpHeaderText)
    string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  FragmentTreeDumper(&string_builder, flags, target)
      .Append(this, fragment_offset, indent);
  return string_builder.ToString();
}

String PhysicalFragment::DumpFragmentTree(const LayoutObject& root,
                                          DumpFlags flags,
                                          const PhysicalFragment* target) {
  const LayoutBox& root_box = To<LayoutBox>(root);
  DCHECK_EQ(root_box.PhysicalFragmentCount(), 1u);
  return root_box.GetPhysicalFragment(0)->DumpFragmentTree(flags, target);
}

void PhysicalFragment::Trace(Visitor* visitor) const {
  switch (Type()) {
    case kFragmentBox:
      static_cast<const PhysicalBoxFragment*>(this)->TraceAfterDispatch(
          visitor);
      break;
    case kFragmentLineBox:
      static_cast<const PhysicalLineBoxFragment*>(this)->TraceAfterDispatch(
          visitor);
      break;
  }
}

void PhysicalFragment::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  visitor->Trace(propagated_data_);
  visitor->Trace(break_token_);
  visitor->Trace(oof_data_);
}

bool PhysicalFragment::DependsOnPercentageBlockSize(
    const FragmentBuilder& builder) {
  if (!builder.node_ || builder.node_.IsInline()) {
    return builder.has_descendant_that_depends_on_percentage_block_size_;
  }

  // NOTE: If an element is OOF positioned, and has top/bottom constraints
  // which are percentage based, this function will return false.
  //
  // This is fine as the top/bottom constraints are computed *before* layout,
  // and the result is set as a fixed-block-size constraint. (And the caching
  // logic will never check the result of this function).
  //
  // The result of this function still may be used for an OOF positioned
  // element if it has a percentage block-size however, but this will return
  // the correct result from below.

  // There are two conditions where we need to know about an (arbitrary)
  // descendant which depends on a %-block-size.
  //  - In quirks mode, the arbitrary descendant may depend the percentage
  //    resolution block-size given (to this node), and need to relayout if
  //    this size changes.
  //  - A flex-item may have its "definiteness" change, (e.g. if itself is a
  //    flex item which is being stretched). This definiteness change will
  //    affect any %-block-size children.
  //
  // NOTE(ikilpatrick): For the flex-item case this is potentially too general.
  // We only need to know about if this flex-item has a %-block-size child if
  // the "definiteness" changes, not if the percentage resolution size changes.
  const BlockNode node = To<BlockNode>(builder.node_);
  const bool is_flex_item =
      !RuntimeEnabledFeatures::LayoutFlexCacheFixEnabled() && node.IsFlexItem();
  if (builder.has_descendant_that_depends_on_percentage_block_size_ &&
      (node.UseParentPercentageResolutionBlockSizeForChildren() ||
       is_flex_item)) {
    return true;
  }

  const ComputedStyle& style = builder.Style();
  if (style.LogicalHeight().MayHavePercentDependence() ||
      style.LogicalMinHeight().MayHavePercentDependence() ||
      style.LogicalMaxHeight().MayHavePercentDependence()) {
    return true;
  }

  return false;
}

void PhysicalFragment::OofData::Trace(Visitor* visitor) const {
  visitor->Trace(oof_positioned_descendants_);
  visitor->Trace(anchor_map_);
}

AnchorMap& PhysicalFragment::OofData::EnsureAnchorMap() {
  if (!anchor_map_) {
    anchor_map_ = MakeGarbageCollected<AnchorMap>();
  }
  return *anchor_map_;
}

void PhysicalFragment::PropagatedData::Trace(Visitor* visitor) const {
  visitor->Trace(sticky_descendants);
  visitor->Trace(snap_areas);
  visitor->Trace(scroll_initial_target);
  visitor->Trace(named_triggers);
}

std::ostream& operator<<(std::ostream& out, const PhysicalFragment& fragment) {
  return out << fragment.ToString();
}

std::ostream& operator<<(std::ostream& out, const PhysicalFragment* fragment) {
  if (!fragment)
    return out << "<null>";
  return out << *fragment;
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowFragmentTree(const blink::PhysicalFragment* fragment) {
  if (!fragment) {
    LOG(INFO) << "Cannot show fragment tree. Fragment is null.";
    return;
  }
  blink::PhysicalFragment::DumpFlags dump_flags =
      blink::PhysicalFragment::DumpAll;
  LOG(INFO) << "\n" << fragment->DumpFragmentTree(dump_flags).Utf8();
}

void ShowFragmentTree(const blink::LayoutObject& root,
                      const blink::PhysicalFragment* target) {
  blink::PhysicalFragment::DumpFlags dump_flags =
      blink::PhysicalFragment::DumpAll;
  LOG(INFO) << "\n"
            << blink::PhysicalFragment::DumpFragmentTree(root, dump_flags,
                                                         target)
                   .Utf8();
}

void ShowEntireFragmentTree(const blink::LayoutObject& target) {
  ShowFragmentTree(*target.View());
}

void ShowEntireFragmentTree(const blink::PhysicalFragment* target) {
  if (!target) {
    LOG(INFO) << "Cannot show fragment tree. Fragment is null.";
    return;
  }
  ShowFragmentTree(*target->GetSelfOrContainerLayoutObject()->View(), target);
}

#endif  // DCHECK_IS_ON()
