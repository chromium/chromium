// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_overflow_calculator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

struct SameSizeAsNGPhysicalFragment
    : public GarbageCollected<SameSizeAsNGPhysicalFragment> {
  Member<void*> layout_object;
  PhysicalSize size;
  unsigned flags;
  Member<void*> members[2];
};

ASSERT_SIZE(NGPhysicalFragment, SameSizeAsNGPhysicalFragment);

String StringForBoxType(const NGPhysicalFragment& fragment) {
  StringBuilder result;
  switch (fragment.BoxType()) {
    case NGPhysicalFragment::NGBoxType::kNormalBox:
      break;
    case NGPhysicalFragment::NGBoxType::kInlineBox:
      result.Append("inline");
      break;
    case NGPhysicalFragment::NGBoxType::kColumnBox:
      result.Append("column");
      break;
    case NGPhysicalFragment::NGBoxType::kPageBox:
      result.Append("page");
      break;
    case NGPhysicalFragment::NGBoxType::kAtomicInline:
      result.Append("atomic-inline");
      break;
    case NGPhysicalFragment::NGBoxType::kFloating:
      result.Append("floating");
      break;
    case NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned:
      result.Append("out-of-flow-positioned");
      break;
    case NGPhysicalFragment::NGBoxType::kBlockFlowRoot:
      result.Append("block-flow-root");
      break;
    case NGPhysicalFragment::NGBoxType::kRenderedLegend:
      result.Append("rendered-legend");
      break;
  }
  if (fragment.IsLegacyLayoutRoot()) {
    if (result.length())
      result.Append(" ");
    result.Append("legacy-layout-root");
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
      To<NGPhysicalBoxFragment>(fragment).IsInlineFormattingContext()) {
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
                     NGPhysicalFragment::DumpFlags flags,
                     const NGPhysicalFragment* target = nullptr)
      : builder_(builder), target_fragment_(target), flags_(flags) {}

  void Append(const NGPhysicalFragment* fragment,
              absl::optional<PhysicalOffset> fragment_offset,
              unsigned indent = 2) {
    AppendIndentation(indent, fragment);

    bool has_content = false;
    if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(fragment)) {
      if (box->IsLayoutObjectDestroyedOrMoved()) {
        builder_->Append("DEAD LAYOUT OBJECT!\n");
        return;
      }
      const LayoutObject* layout_object = box->GetLayoutObject();
      if (flags_ & NGPhysicalFragment::DumpType) {
        builder_->Append("Box");
        String box_type = StringForBoxType(*fragment);
        has_content = true;
        if (!box_type.empty()) {
          builder_->Append(" (");
          builder_->Append(box_type);
          builder_->Append(")");
        }
        if (flags_ & NGPhysicalFragment::DumpSelfPainting &&
            box->HasSelfPaintingLayer()) {
          if (box_type.empty())
            builder_->Append(" ");
          builder_->Append("(self paint)");
        }
      }
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);

      if (flags_ & NGPhysicalFragment::DumpNodeName && layout_object) {
        if (has_content)
          builder_->Append(" ");
        builder_->Append(layout_object->DebugName());
      }
      builder_->Append("\n");

      bool has_fragment_items = false;
      if (flags_ & NGPhysicalFragment::DumpItems) {
        if (const NGFragmentItems* fragment_items = box->Items()) {
          NGInlineCursor cursor(*box, *fragment_items);
          Append(&cursor, indent + 2);
          has_fragment_items = true;
        }
      }
      if (flags_ & NGPhysicalFragment::DumpSubtree) {
        if (flags_ & NGPhysicalFragment::DumpLegacyDescendants &&
            layout_object && !layout_object->IsLayoutNGObject()) {
          DCHECK(box->Children().empty());
          AppendLegacySubtree(*layout_object, indent);
          return;
        }
        for (auto& child : box->Children()) {
          if (has_fragment_items && child->IsLineBox())
            continue;
          Append(child.get(), child.Offset(), indent + 2);
        }
      }
      return;
    }

    if (const auto* line_box = DynamicTo<NGPhysicalLineBoxFragment>(fragment)) {
      if (flags_ & NGPhysicalFragment::DumpType) {
        builder_->Append("LineBox");
        has_content = true;
      }
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);
      builder_->Append("\n");

      if (flags_ & NGPhysicalFragment::DumpSubtree) {
        for (auto& child : line_box->Children()) {
          Append(child.get(), child.Offset(), indent + 2);
        }
        return;
      }
    }

    if (flags_ & NGPhysicalFragment::DumpType) {
      builder_->Append("Unknown fragment type");
      has_content = true;
    }
    has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);
    builder_->Append("\n");
  }

  void AppendLegacySubtree(const LayoutObject& layout_object, unsigned indent) {
    for (const LayoutObject* descendant = &layout_object; descendant;) {
      if (!IsNGRootWithFragments(*descendant)) {
        if (const auto* block = DynamicTo<LayoutBlock>(descendant)) {
          if (const auto* positioned_descendants = block->PositionedObjects()) {
            for (const auto& positioned_object : *positioned_descendants) {
              if (IsNGRootWithFragments(*positioned_object))
                AppendNGRootInLegacySubtree(*positioned_object, indent);
              else
                AppendLegacySubtree(*positioned_object, indent);
            }
          }
        }
        if (descendant->IsOutOfFlowPositioned() && descendant != &layout_object)
          descendant = descendant->NextInPreOrderAfterChildren(&layout_object);
        else
          descendant = descendant->NextInPreOrder(&layout_object);
        continue;
      }
      AppendNGRootInLegacySubtree(*descendant, indent);
      descendant = descendant->NextInPreOrderAfterChildren(&layout_object);
    }
  }

  void AppendLegacySubtree(const LayoutObject& layout_object) {
    AppendLegacySubtree(layout_object, 0);
    if (target_fragment_ && !target_fragment_found_) {
      if (flags_ & NGPhysicalFragment::DumpHeaderText) {
        builder_->Append("(Fragment not found when searching the subtree)\n");
        builder_->Append("(Dumping detached fragment tree now:)\n");
      }
      Append(target_fragment_, absl::nullopt);
    }
  }

  void AppendNGRootInLegacySubtree(const LayoutObject& layout_object,
                                   unsigned indent) {
    DCHECK(IsNGRootWithFragments(layout_object));
    if (flags_ & NGPhysicalFragment::DumpHeaderText) {
      AppendIndentation(indent + 2);
      builder_->Append(
          "(NG fragment root inside fragment-less or legacy subtree:)\n");
    }
    const LayoutBox& box_descendant = To<LayoutBox>(layout_object);
    DCHECK_EQ(box_descendant.PhysicalFragmentCount(), 1u);
    Append(box_descendant.GetPhysicalFragment(0), absl::nullopt, indent + 4);
  }

 private:
  void Append(NGInlineCursor* cursor, unsigned indent) {
    for (; *cursor; cursor->MoveToNextSkippingChildren()) {
      const NGInlineCursorPosition& current = cursor->Current();
      const NGPhysicalFragment* box = current.BoxFragment();
      if (box && !box->IsInlineBox()) {
        Append(box, current.OffsetInContainerFragment(), indent);
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

      if (flags_ & NGPhysicalFragment::DumpOffset) {
        builder_->Append(" offset:");
        builder_->Append(current.OffsetInContainerFragment().ToString());
      }
      if (flags_ & NGPhysicalFragment::DumpSize) {
        builder_->Append(" size:");
        builder_->Append(current.Size().ToString());
      }

      builder_->Append("\n");

      if (flags_ & NGPhysicalFragment::DumpSubtree && current.HasChildren()) {
        NGInlineCursor descendants = cursor->CursorForDescendants();
        Append(&descendants, indent + 2);
      }
    }
  }

  bool AppendOffsetAndSize(const NGPhysicalFragment* fragment,
                           absl::optional<PhysicalOffset> fragment_offset,
                           bool has_content) {
    if (flags_ & NGPhysicalFragment::DumpOffset) {
      if (has_content)
        builder_->Append(" ");
      builder_->Append("offset:");
      if (fragment_offset)
        builder_->Append(fragment_offset->ToString());
      else
        builder_->Append("unplaced");
      has_content = true;
    }
    if (flags_ & NGPhysicalFragment::DumpSize) {
      if (has_content)
        builder_->Append(" ");
      builder_->Append("size:");
      builder_->Append(fragment->Size().ToString());
      has_content = true;
    }
    return has_content;
  }

  void AppendIndentation(unsigned indent,
                         const NGPhysicalFragment* fragment = nullptr) {
    if (flags_ & NGPhysicalFragment::DumpIndentation) {
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

  // Check if the object is an NG root ready to be traversed. If layout of the
  // object hasn't finished yet, there'll be no fragment, and false will be
  // returned.
  bool IsNGRootWithFragments(const LayoutObject& object) const {
    if (!object.IsLayoutNGObject())
      return false;
    const LayoutBox* box = DynamicTo<LayoutBox>(&object);
    if (!box)
      return false;
    // A root should only have at most one fragment, or zero if it hasn't been
    // laid out yet.
    DCHECK_LE(box->PhysicalFragmentCount(), 1u);
    return box->PhysicalFragmentCount();
  }

  StringBuilder* builder_;
  const NGPhysicalFragment* target_fragment_ = nullptr;
  NGPhysicalFragment::DumpFlags flags_;
  bool target_fragment_found_ = false;
};

}  // namespace

NGPhysicalFragment::NGPhysicalFragment(NGContainerFragmentBuilder* builder,
                                       WritingMode block_or_line_writing_mode,
                                       NGFragmentType type,
                                       unsigned sub_type)
    : layout_object_(builder->layout_object_),
      size_(ToPhysicalSize(builder->size_, builder->GetWritingMode())),
      has_floating_descendants_for_paint_(false),
      children_valid_(true),
      type_(type),
      sub_type_(sub_type),
      style_variant_((unsigned)builder->style_variant_),
      is_hidden_for_paint_(builder->is_hidden_for_paint_),
      is_opaque_(builder->is_opaque_),
      is_block_in_inline_(builder->is_block_in_inline_),
      may_have_descendant_above_block_start_(
          builder->may_have_descendant_above_block_start_),
      is_fieldset_container_(false),
      is_table_ng_part_(false),
      is_legacy_layout_root_(false),
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
      break_token_(std::move(builder->break_token_)),
      oof_data_(builder->oof_positioned_descendants_.empty() &&
                        !builder->AnchorQuery() &&
                        !has_fragmented_out_of_flow_data_
                    ? nullptr
                    : OutOfFlowDataFromBuilder(builder)) {
  CHECK(builder->layout_object_);
  has_floating_descendants_for_paint_ =
      builder->has_floating_descendants_for_paint_;
  has_adjoining_object_descendants_ =
      builder->has_adjoining_object_descendants_;
  depends_on_percentage_block_size_ = DependsOnPercentageBlockSize(*builder);
  children_valid_ = true;
}

NGPhysicalFragment::OutOfFlowData* NGPhysicalFragment::OutOfFlowDataFromBuilder(
    NGContainerFragmentBuilder* builder) {
  OutOfFlowData* oof_data = nullptr;
  if (has_fragmented_out_of_flow_data_)
    oof_data = FragmentedOutOfFlowDataFromBuilder(builder);

  const WritingModeConverter converter(
      {builder->Style().GetWritingMode(), builder->Direction()}, Size());

  if (!builder->oof_positioned_descendants_.empty()) {
    if (!oof_data)
      oof_data = MakeGarbageCollected<OutOfFlowData>();
    oof_data->oof_positioned_descendants.reserve(
        builder->oof_positioned_descendants_.size());
    for (const auto& descendant : builder->oof_positioned_descendants_) {
      NGInlineContainer<PhysicalOffset> inline_container(
          descendant.inline_container.container,
          converter.ToPhysical(descendant.inline_container.relative_offset,
                               PhysicalSize()));
      oof_data->oof_positioned_descendants.emplace_back(
          descendant.Node(),
          descendant.static_position.ConvertToPhysical(converter),
          inline_container);
    }
  }

  if (const NGLogicalAnchorQuery* anchor_query = builder->AnchorQuery()) {
    DCHECK(RuntimeEnabledFeatures::CSSAnchorPositioningEnabled());
    if (!oof_data)
      oof_data = MakeGarbageCollected<OutOfFlowData>();
    oof_data->anchor_query.SetFromLogical(*anchor_query, converter);
  }

  return oof_data;
}

// Even though the other constructors don't initialize many of these fields
// (instead set by their super-classes), the copy constructor does.
NGPhysicalFragment::NGPhysicalFragment(const NGPhysicalFragment& other)
    : layout_object_(other.layout_object_),
      size_(other.size_),
      has_floating_descendants_for_paint_(
          other.has_floating_descendants_for_paint_),
      has_adjoining_object_descendants_(
          other.has_adjoining_object_descendants_),
      depends_on_percentage_block_size_(
          other.depends_on_percentage_block_size_),
      children_valid_(other.children_valid_),
      has_propagated_descendants_(other.has_propagated_descendants_),
      has_hanging_(other.has_hanging_),
      type_(other.type_),
      sub_type_(other.sub_type_),
      style_variant_(other.style_variant_),
      is_hidden_for_paint_(other.is_hidden_for_paint_),
      is_opaque_(other.is_opaque_),
      is_block_in_inline_(other.is_block_in_inline_),
      is_math_fraction_(other.is_math_fraction_),
      is_math_operator_(other.is_math_operator_),
      may_have_descendant_above_block_start_(
          other.may_have_descendant_above_block_start_),
      is_fieldset_container_(other.is_fieldset_container_),
      is_table_ng_part_(other.is_table_ng_part_),
      is_legacy_layout_root_(other.is_legacy_layout_root_),
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
      break_token_(other.break_token_),
      oof_data_(other.oof_data_ ? other.CloneOutOfFlowData() : nullptr) {
  CHECK(layout_object_);
  DCHECK(other.children_valid_);
  DCHECK(children_valid_);
}

NGPhysicalFragment::~NGPhysicalFragment() {
  Dispose();
}

void NGPhysicalFragment::Dispose() {
  switch (Type()) {
    case kFragmentBox:
      static_cast<NGPhysicalBoxFragment*>(this)->Dispose();
      break;
    case kFragmentLineBox:
      static_cast<NGPhysicalLineBoxFragment*>(this)->Dispose();
      break;
  }
}

bool NGPhysicalFragment::IsBlockFlow() const {
  return !IsLineBox() && layout_object_->IsLayoutBlockFlow();
}

bool NGPhysicalFragment::IsTextControlContainer() const {
  return IsCSSBox() && blink::IsTextControlContainer(layout_object_->GetNode());
}

bool NGPhysicalFragment::IsTextControlPlaceholder() const {
  return IsCSSBox() &&
         blink::IsTextControlPlaceholder(layout_object_->GetNode());
}

base::span<NGPhysicalOutOfFlowPositionedNode>
NGPhysicalFragment::OutOfFlowPositionedDescendants() const {
  if (!HasOutOfFlowPositionedDescendants())
    return base::span<NGPhysicalOutOfFlowPositionedNode>();
  return {oof_data_->oof_positioned_descendants.data(),
          oof_data_->oof_positioned_descendants.size()};
}

NGFragmentedOutOfFlowData* NGPhysicalFragment::FragmentedOutOfFlowData() const {
  if (!has_fragmented_out_of_flow_data_)
    return nullptr;
  auto* oof_data =
      reinterpret_cast<NGFragmentedOutOfFlowData*>(oof_data_.Get());
  DCHECK(!oof_data->multicols_with_pending_oofs.empty() ||
         !oof_data->oof_positioned_fragmentainer_descendants.empty());
  return oof_data;
}

bool NGPhysicalFragment::HasNestedMulticolsWithOOFs() const {
  const NGFragmentedOutOfFlowData* oof_data = FragmentedOutOfFlowData();
  return oof_data && !oof_data->multicols_with_pending_oofs.empty();
}

bool NGPhysicalFragment::NeedsOOFPositionedInfoPropagation() const {
  // If we have |oof_data_|, it should mean at least one of OOF propagation data
  // exists.
  DCHECK_EQ(
      !!oof_data_,
      HasOutOfFlowPositionedDescendants() || HasAnchorQuery() ||
          (FragmentedOutOfFlowData() &&
           FragmentedOutOfFlowData()->NeedsOOFPositionedInfoPropagation()));
  return !!oof_data_;
}

void NGPhysicalFragment::ClearOutOfFlowData() {
  if (!oof_data_)
    return;
  if (HasAnchorQuery())
    oof_data_->oof_positioned_descendants.clear();
  else
    oof_data_ = nullptr;
}

NGPhysicalFragment::OutOfFlowData* NGPhysicalFragment::CloneOutOfFlowData()
    const {
  DCHECK(oof_data_);
  if (!has_fragmented_out_of_flow_data_)
    return MakeGarbageCollected<OutOfFlowData>(*oof_data_);
  DCHECK(FragmentedOutOfFlowData());
  return MakeGarbageCollected<NGFragmentedOutOfFlowData>(
      *FragmentedOutOfFlowData());
}

bool NGPhysicalFragment::IsMonolithic() const {
  // Line boxes are monolithic, except for line boxes that are just there to
  // contain a block inside an inline, in which case the anonymous block child
  // wrapper inside the line is breakable.
  if (IsLineBox())
    return !IsBlockInInline();
  if (const auto* box_fragment = DynamicTo<NGPhysicalBoxFragment>(this))
    return box_fragment->IsMonolithic();
  return false;
}

bool NGPhysicalFragment::IsImplicitAnchor() const {
  if (Element* element = DynamicTo<Element>(GetNode()))
    return element->HasAnchoredPopover();
  return false;
}

const FragmentData* NGPhysicalFragment::GetFragmentData() const {
  const LayoutBox* box = DynamicTo<LayoutBox>(GetLayoutObject());
  if (!box) {
    DCHECK(!GetLayoutObject());
    return nullptr;
  }
  return box->FragmentDataFromPhysicalFragment(
      To<NGPhysicalBoxFragment>(*this));
}

const NGPhysicalFragment* NGPhysicalFragment::PostLayout() const {
  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(this))
    return box->PostLayout();
  return this;
}

#if DCHECK_IS_ON()
void NGPhysicalFragment::CheckType() const {
  switch (Type()) {
    case kFragmentBox:
      if (IsInlineBox()) {
        DCHECK(layout_object_->IsLayoutInline());
      } else {
        DCHECK(layout_object_->IsBox());
      }
      if (IsFragmentainerBox()) {
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
      if (layout_object_->IsLayoutNGOutsideListMarker()) {
        // List marker is an atomic inline if it appears in a line box, or a
        // block box.
        DCHECK(!IsFloating());
        DCHECK(!IsOutOfFlowPositioned());
        DCHECK(IsAtomicInline() || (IsBox() && BoxType() == kBlockFlowRoot));
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

PhysicalRect NGPhysicalFragment::ScrollableOverflow(
    const NGPhysicalBoxFragment& container,
    TextHeightType height_type) const {
  switch (Type()) {
    case kFragmentBox:
      return To<NGPhysicalBoxFragment>(*this).ScrollableOverflow(height_type);
    case kFragmentLineBox:
      NOTREACHED()
          << "You must call NGLineBoxFragment::ScrollableOverflow explicitly.";
      break;
  }
  NOTREACHED();
  return {{}, Size()};
}

PhysicalRect NGPhysicalFragment::ScrollableOverflowForPropagation(
    const NGPhysicalBoxFragment& container,
    TextHeightType height_type) const {
  PhysicalRect overflow = ScrollableOverflow(container, height_type);
  AdjustScrollableOverflowForPropagation(container, height_type, &overflow);
  return overflow;
}

void NGPhysicalFragment::AdjustScrollableOverflowForPropagation(
    const NGPhysicalBoxFragment& container,
    TextHeightType height_type,
    PhysicalRect* overflow) const {
  DCHECK(!IsLineBox());
  if (!IsCSSBox())
    return;
  if (UNLIKELY(IsLayoutObjectDestroyedOrMoved())) {
    NOTREACHED();
    return;
  }

  if (height_type == TextHeightType::kNormalHeight && Type() == kFragmentBox)
    overflow->Unite({{}, Size()});

  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  const LayoutObject* container_layout_object = container.GetLayoutObject();
  DCHECK(container_layout_object);
  if (layout_object->ShouldUseTransformFromContainer(container_layout_object)) {
    gfx::Transform transform;
    layout_object->GetTransformFromContainer(container_layout_object,
                                             PhysicalOffset(), transform);
    *overflow =
        PhysicalRect::EnclosingRect(transform.MapRect(gfx::RectF(*overflow)));
  }
}

TouchAction NGPhysicalFragment::EffectiveAllowedTouchAction() const {
  DCHECK(layout_object_);
  return layout_object_->EffectiveAllowedTouchAction();
}

bool NGPhysicalFragment::InsideBlockingWheelEventHandler() const {
  DCHECK(layout_object_);
  return layout_object_->InsideBlockingWheelEventHandler();
}

LogicalRect NGPhysicalFragment::ConvertChildToLogical(
    const PhysicalRect& physical_rect) const {
  return WritingModeConverter(Style().GetWritingDirection(), Size())
      .ToLogical(physical_rect);
}

String NGPhysicalFragment::ToString() const {
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

String NGPhysicalFragment::DumpFragmentTree(
    DumpFlags flags,
    const NGPhysicalFragment* target,
    absl::optional<PhysicalOffset> fragment_offset,
    unsigned indent) const {
  StringBuilder string_builder;
  if (flags & DumpHeaderText)
    string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  FragmentTreeDumper(&string_builder, flags, target)
      .Append(this, fragment_offset, indent);
  return string_builder.ToString();
}

String NGPhysicalFragment::DumpFragmentTree(const LayoutObject& root,
                                            DumpFlags flags,
                                            const NGPhysicalFragment* target) {
  if (root.IsLayoutNGObject()) {
    const LayoutBox& root_box = To<LayoutBox>(root);
    DCHECK_EQ(root_box.PhysicalFragmentCount(), 1u);
    return root_box.GetPhysicalFragment(0)->DumpFragmentTree(flags, target);
  }
  StringBuilder string_builder;
  if (flags & DumpHeaderText) {
    string_builder.Append(
        ".:: LayoutNG Physical Fragment Tree at legacy root ");
    string_builder.Append(root.DebugName());
    string_builder.Append(" ::.\n");
  }
  FragmentTreeDumper(&string_builder, flags, target).AppendLegacySubtree(root);
  return string_builder.ToString();
}

void NGPhysicalFragment::Trace(Visitor* visitor) const {
  switch (Type()) {
    case kFragmentBox:
      static_cast<const NGPhysicalBoxFragment*>(this)->TraceAfterDispatch(
          visitor);
      break;
    case kFragmentLineBox:
      static_cast<const NGPhysicalLineBoxFragment*>(this)->TraceAfterDispatch(
          visitor);
      break;
  }
}

void NGPhysicalFragment::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  visitor->Trace(break_token_);
  visitor->Trace(oof_data_);
}

// TODO(dlibby): remove `Children` and `PostLayoutChildren` and move the
// casting and/or branching to the callers.
base::span<const NGLink> NGPhysicalFragment::Children() const {
  if (Type() == kFragmentBox)
    return static_cast<const NGPhysicalBoxFragment*>(this)->Children();
  return base::make_span(static_cast<NGLink*>(nullptr), 0);
}

NGPhysicalFragment::PostLayoutChildLinkList
NGPhysicalFragment::PostLayoutChildren() const {
  if (Type() == kFragmentBox) {
    return static_cast<const NGPhysicalBoxFragment*>(this)
        ->PostLayoutChildren();
  }
  return PostLayoutChildLinkList(0, nullptr);
}

void NGPhysicalFragment::SetChildrenInvalid() const {
  if (!children_valid_)
    return;

  for (const NGLink& child : Children()) {
    const_cast<NGLink&>(child).fragment = nullptr;
  }
  children_valid_ = false;
}

// additional_offset must be offset from the containing_block.
void NGPhysicalFragment::AddOutlineRectsForNormalChildren(
    Vector<PhysicalRect>* outline_rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    const LayoutBoxModelObject* containing_block) const {
  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(this)) {
    DCHECK_EQ(box->PostLayout(), box);
    if (const NGFragmentItems* items = box->Items()) {
      NGInlineCursor cursor(*box, *items);
      AddOutlineRectsForCursor(outline_rects, additional_offset, outline_type,
                               containing_block, &cursor);
      // Don't add |Children()|. If |this| has |NGFragmentItems|, children are
      // either line box, which we already handled in items, or OOF, which we
      // should ignore.
      DCHECK(
          base::ranges::all_of(PostLayoutChildren(), [](const NGLink& child) {
            return child->IsLineBox() || child->IsOutOfFlowPositioned();
          }));
      return;
    }
  }

  for (const auto& child : PostLayoutChildren()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (!child->IsLineBox()) {
      const LayoutObject* child_layout_object = child->GetLayoutObject();
      if (auto* child_layout_block_flow =
              DynamicTo<LayoutBlockFlow>(child_layout_object)) {
        if (child_layout_object->IsElementContinuation() ||
            child_layout_block_flow->IsAnonymousBlockContinuation())
          continue;
      }
    }
    AddOutlineRectsForDescendant(child, outline_rects, additional_offset,
                                 outline_type, containing_block);
  }
}

void NGPhysicalFragment::AddOutlineRectsForCursor(
    Vector<PhysicalRect>* outline_rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    const LayoutBoxModelObject* containing_block,
    NGInlineCursor* cursor) const {
  const auto* const text_combine =
      DynamicTo<LayoutNGTextCombine>(containing_block);
  while (*cursor) {
    DCHECK(cursor->Current().Item());
    const NGFragmentItem& item = *cursor->Current().Item();
    if (UNLIKELY(item.IsLayoutObjectDestroyedOrMoved())) {
      cursor->MoveToNext();
      continue;
    }
    switch (item.Type()) {
      case NGFragmentItem::kLine: {
        AddOutlineRectsForDescendant(
            {item.LineBoxFragment(), item.OffsetInContainerFragment()},
            outline_rects, additional_offset, outline_type, containing_block);
        break;
      }
      case NGFragmentItem::kGeneratedText:
      case NGFragmentItem::kText: {
        if (outline_type == NGOutlineType::kDontIncludeBlockVisualOverflow)
          break;
        PhysicalRect rect = item.RectInContainerFragment();
        if (UNLIKELY(text_combine))
          rect = text_combine->AdjustRectForBoundingBox(rect);
        rect.Move(additional_offset);
        outline_rects->push_back(rect);
        break;
      }
      case NGFragmentItem::kSvgText: {
        auto rect = PhysicalRect::EnclosingRect(
            cursor->Current().ObjectBoundingBox(*cursor));
        DCHECK(!text_combine);
        rect.Move(additional_offset);
        outline_rects->push_back(rect);
        break;
      }
      case NGFragmentItem::kBox: {
        if (const NGPhysicalBoxFragment* child_box =
                item.PostLayoutBoxFragment()) {
          DCHECK(!child_box->IsOutOfFlowPositioned());
          AddOutlineRectsForDescendant(
              {child_box, item.OffsetInContainerFragment()}, outline_rects,
              additional_offset, outline_type, containing_block);
          // Skip descendants as they were already added.
          DCHECK(item.IsInlineBox() || item.DescendantsCount() == 1);
          cursor->MoveToNextSkippingChildren();
          continue;
        }
        break;
      }
    }
    cursor->MoveToNext();
  }
}

void NGPhysicalFragment::AddScrollableOverflowForInlineChild(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const NGFragmentItem& line,
    bool has_hanging,
    const NGInlineCursor& cursor,
    TextHeightType height_type,
    PhysicalRect* overflow) const {
  DCHECK(IsLineBox() || IsInlineBox());
  DCHECK(cursor.Current().Item() &&
         (cursor.Current().Item()->BoxFragment() == this ||
          cursor.Current().Item()->LineBoxFragment() == this));
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  for (NGInlineCursor descendants = cursor.CursorForDescendants();
       descendants;) {
    const NGFragmentItem* item = descendants.CurrentItem();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      NOTREACHED();
      descendants.MoveToNextSkippingChildren();
      continue;
    }
    if (item->IsText()) {
      PhysicalRect child_scroll_overflow = item->RectInContainerFragment();
      if (height_type == TextHeightType::kEmHeight) {
        child_scroll_overflow = AdjustTextRectForEmHeight(
            child_scroll_overflow, item->Style(), item->TextShapeResult(),
            container_writing_mode);
      }
      if (UNLIKELY(has_hanging)) {
        AdjustScrollableOverflowForHanging(line.RectInContainerFragment(),
                                           container_writing_mode,
                                           &child_scroll_overflow);
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    if (const NGPhysicalBoxFragment* child_box =
            item->PostLayoutBoxFragment()) {
      PhysicalRect child_scroll_overflow;
      if (height_type == TextHeightType::kNormalHeight ||
          (child_box->BoxType() != kInlineBox && !IsRubyBox()))
        child_scroll_overflow = item->RectInContainerFragment();
      if (child_box->IsInlineBox()) {
        child_box->AddScrollableOverflowForInlineChild(
            container, container_style, line, has_hanging, descendants,
            height_type, &child_scroll_overflow);
        child_box->AdjustScrollableOverflowForPropagation(
            container, height_type, &child_scroll_overflow);
        if (UNLIKELY(has_hanging)) {
          AdjustScrollableOverflowForHanging(line.RectInContainerFragment(),
                                             container_writing_mode,
                                             &child_scroll_overflow);
        }
      } else {
        child_scroll_overflow =
            child_box->ScrollableOverflowForPropagation(container, height_type);
        child_scroll_overflow.offset += item->OffsetInContainerFragment();
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    // Add all children of a culled inline box; i.e., an inline box without
    // margin/border/padding etc.
    DCHECK_EQ(item->Type(), NGFragmentItem::kBox);
    descendants.MoveToNext();
  }
}

// Chop the hanging part from scrollable overflow. Children overflow in inline
// direction should hang, which should not cause scroll.
// TODO(kojii): Should move to text fragment to make this more accurate.
void NGPhysicalFragment::AdjustScrollableOverflowForHanging(
    const PhysicalRect& rect,
    const WritingMode container_writing_mode,
    PhysicalRect* overflow) {
  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (overflow->offset.left < rect.offset.left)
      overflow->offset.left = rect.offset.left;
    if (overflow->Right() > rect.Right())
      overflow->ShiftRightEdgeTo(rect.Right());
  } else {
    if (overflow->offset.top < rect.offset.top)
      overflow->offset.top = rect.offset.top;
    if (overflow->Bottom() > rect.Bottom())
      overflow->ShiftBottomEdgeTo(rect.Bottom());
  }
}

// additional_offset must be offset from the containing_block because
// LocalToAncestorRect returns rects wrt containing_block.
void NGPhysicalFragment::AddOutlineRectsForDescendant(
    const NGLink& descendant,
    Vector<PhysicalRect>* outline_rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    const LayoutBoxModelObject* containing_block) const {
  DCHECK(!descendant->IsLayoutObjectDestroyedOrMoved());
  if (descendant->IsListMarker())
    return;

  if (const auto* descendant_box =
          DynamicTo<NGPhysicalBoxFragment>(descendant.get())) {
    DCHECK_EQ(descendant_box->PostLayout(), descendant_box);
    const LayoutObject* descendant_layout_object =
        descendant_box->GetLayoutObject();

    // TODO(layoutng): Explain this check. I assume we need it because layers
    // may have transforms and so we have to go through LocalToAncestorRects?
    if (descendant_box->HasLayer()) {
      DCHECK(descendant_layout_object);
      Vector<PhysicalRect> layer_outline_rects;
      descendant_box->AddOutlineRects(PhysicalOffset(), outline_type,
                                      &layer_outline_rects);

      descendant_layout_object->LocalToAncestorRects(
          layer_outline_rects, containing_block, PhysicalOffset(),
          additional_offset);
      outline_rects->AppendVector(layer_outline_rects);
      return;
    }

    if (!descendant_box->IsInlineBox()) {
      descendant_box->AddSelfOutlineRects(
          additional_offset + descendant.Offset(), outline_type, outline_rects,
          nullptr);
      return;
    }

    DCHECK(descendant_layout_object);
    const auto* descendant_layout_inline =
        To<LayoutInline>(descendant_layout_object);
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its line box which cover the line boxes of this LayoutInline. So
    // the LayoutInline needs to add rects for children and continuations
    // only.
    if (descendant_box->IsOutlineOwner()) {
      // We don't pass additional_offset here because the function requires
      // additional_offset to be the offset from the containing block.
      descendant_layout_inline->AddOutlineRectsForChildrenAndContinuations(
          *outline_rects, PhysicalOffset(), outline_type);
    }
    return;
  }

  if (const auto* descendant_line_box =
          DynamicTo<NGPhysicalLineBoxFragment>(descendant.get())) {
    descendant_line_box->AddOutlineRectsForNormalChildren(
        outline_rects, additional_offset + descendant.Offset(), outline_type,
        containing_block);
    // We don't add the line box itself. crbug.com/1203247.
  }
}

bool NGPhysicalFragment::DependsOnPercentageBlockSize(
    const NGContainerFragmentBuilder& builder) {
  NGLayoutInputNode node = builder.node_;

  if (!node || node.IsInline())
    return builder.has_descendant_that_depends_on_percentage_block_size_;

  // For the below if-stmt we only want to consider legacy *containers* as
  // potentially having %-dependent children - i.e. an image doesn't have any
  // children.
  bool is_legacy_container_with_percent_height_descendants =
      builder.is_legacy_layout_root_ && !node.IsReplaced() &&
      node.GetLayoutBox()->MaybeHasPercentHeightDescendant();

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
  if ((builder.has_descendant_that_depends_on_percentage_block_size_ ||
       is_legacy_container_with_percent_height_descendants) &&
      (node.UseParentPercentageResolutionBlockSizeForChildren() ||
       node.IsFlexItem()))
    return true;

  const ComputedStyle& style = builder.Style();
  if (style.LogicalHeight().IsPercentOrCalc() ||
      style.LogicalMinHeight().IsPercentOrCalc() ||
      style.LogicalMaxHeight().IsPercentOrCalc())
    return true;

  return false;
}

void NGPhysicalFragment::OutOfFlowData::Trace(Visitor* visitor) const {
  visitor->Trace(oof_positioned_descendants);
  visitor->Trace(anchor_query);
}

std::ostream& operator<<(std::ostream& out,
                         const NGPhysicalFragment& fragment) {
  return out << fragment.ToString();
}

std::ostream& operator<<(std::ostream& out,
                         const NGPhysicalFragment* fragment) {
  if (!fragment)
    return out << "<null>";
  return out << *fragment;
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowFragmentTree(const blink::NGPhysicalFragment* fragment) {
  if (!fragment) {
    LOG(INFO) << "Cannot show fragment tree. Fragment is null.";
    return;
  }
  blink::NGPhysicalFragment::DumpFlags dump_flags =
      blink::NGPhysicalFragment::DumpAll;
  LOG(INFO) << "\n" << fragment->DumpFragmentTree(dump_flags).Utf8();
}

void ShowFragmentTree(const blink::LayoutObject& root,
                      const blink::NGPhysicalFragment* target) {
  blink::NGPhysicalFragment::DumpFlags dump_flags =
      blink::NGPhysicalFragment::DumpAll;
  LOG(INFO) << "\n"
            << blink::NGPhysicalFragment::DumpFragmentTree(root, dump_flags,
                                                           target)
                   .Utf8();
}

void ShowEntireFragmentTree(const blink::LayoutObject& target) {
  ShowFragmentTree(*target.View());
}

void ShowEntireFragmentTree(const blink::NGPhysicalFragment* target) {
  if (!target) {
    LOG(INFO) << "Cannot show fragment tree. Fragment is null.";
    return;
  }
  ShowFragmentTree(*target->GetSelfOrContainerLayoutObject()->View(), target);
}

#endif  // DCHECK_IS_ON()
