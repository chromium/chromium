// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

struct SameSizeAsNGPhysicalFragment
    : RefCounted<const NGPhysicalFragment, NGPhysicalFragmentTraits> {
  // |flags_for_free_maybe| is used to support an additional increase in size
  // needed for DCHECK and 32-bit builds.
  unsigned flags_for_free_maybe;
  void* layout_object;
  PhysicalSize size;
  unsigned flags;
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
                     NGPhysicalFragment::DumpFlags flags)
      : builder_(builder), flags_(flags) {}

  void Append(const NGPhysicalFragment* fragment,
              base::Optional<PhysicalOffset> fragment_offset,
              unsigned indent = 2) {
    AppendIndentation(indent);

    bool has_content = false;
    if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(fragment)) {
      if (flags_ & NGPhysicalFragment::DumpType) {
        builder_->Append("Box");
        String box_type = StringForBoxType(*fragment);
        has_content = true;
        if (!box_type.IsEmpty()) {
          builder_->Append(" (");
          builder_->Append(box_type);
          builder_->Append(")");
        }
        if (flags_ & NGPhysicalFragment::DumpSelfPainting &&
            box->HasSelfPaintingLayer()) {
          if (box_type.IsEmpty())
            builder_->Append(" ");
          builder_->Append("(self paint)");
        }
      }
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);

      if (flags_ & NGPhysicalFragment::DumpNodeName &&
          fragment->GetLayoutObject()) {
        if (has_content)
          builder_->Append(" ");
        builder_->Append(fragment->GetLayoutObject()->DebugName());
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

    if (const auto* text = DynamicTo<NGPhysicalTextFragment>(fragment)) {
      if (flags_ & NGPhysicalFragment::DumpType) {
        builder_->Append("Text");
        has_content = true;
      }
      has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);

      if (flags_ & NGPhysicalFragment::DumpTextOffsets) {
        if (has_content)
          builder_->Append(' ');
        builder_->AppendFormat("start: %u end: %u", text->StartOffset(),
                               text->EndOffset());
        has_content = true;
      }
      builder_->Append("\n");
      return;
    }

    if (flags_ & NGPhysicalFragment::DumpType) {
      builder_->Append("Unknown fragment type");
      has_content = true;
    }
    has_content = AppendOffsetAndSize(fragment, fragment_offset, has_content);
    builder_->Append("\n");
  }

 private:
  void Append(NGInlineCursor* cursor, unsigned indent) {
    for (; *cursor; cursor->MoveToNextSkippingChildren()) {
      const NGInlineCursorPosition& current = cursor->Current();
      if (const NGPhysicalBoxFragment* box = current.BoxFragment()) {
        if (!box->IsInlineBox()) {
          Append(box, current.OffsetInContainerFragment(), indent);
          continue;
        }
      }

      AppendIndentation(indent);

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
                           base::Optional<PhysicalOffset> fragment_offset,
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

  void AppendIndentation(unsigned indent) {
    if (flags_ & NGPhysicalFragment::DumpIndentation) {
      for (unsigned i = 0; i < indent; i++)
        builder_->Append(" ");
    }
  }

  StringBuilder* builder_;
  NGPhysicalFragment::DumpFlags flags_;
};

}  // namespace

// static
void NGPhysicalFragmentTraits::Destruct(const NGPhysicalFragment* fragment) {
  fragment->Destroy();
}

NGPhysicalFragment::NGPhysicalFragment(NGFragmentBuilder* builder,
                                       NGFragmentType type,
                                       unsigned sub_type)
    : has_floating_descendants_for_paint_(false),
      layout_object_(builder->layout_object_),
      size_(ToPhysicalSize(builder->size_, builder->GetWritingMode())),
      type_(type),
      sub_type_(sub_type),
      style_variant_((unsigned)builder->style_variant_),
      is_hidden_for_paint_(builder->is_hidden_for_paint_),
      is_fieldset_container_(false),
      is_table_ng_part_(false),
      is_legacy_layout_root_(false),
      is_painted_atomically_(false),
      has_collapsed_borders_(builder->has_collapsed_borders_),
      has_baseline_(false) {
  CHECK(builder->layout_object_);
}

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       NGStyleVariant style_variant,
                                       PhysicalSize size,
                                       NGFragmentType type,
                                       unsigned sub_type)
    : has_floating_descendants_for_paint_(false),
      has_layout_overflow_(false),
      has_inflow_bounds_(false),
      has_rare_data_(false),
      layout_object_(layout_object),
      size_(size),
      type_(type),
      sub_type_(sub_type),
      style_variant_((unsigned)style_variant),
      is_hidden_for_paint_(false),
      is_fieldset_container_(false),
      is_table_ng_part_(false),
      is_legacy_layout_root_(false),
      is_painted_atomically_(false),
      has_collapsed_borders_(false),
      has_baseline_(false),
      has_last_baseline_(false) {
  CHECK(layout_object);
}

// Even though the other constructors don't initialize many of these fields
// (instead set by their super-classes), the copy constructor does.
NGPhysicalFragment::NGPhysicalFragment(const NGPhysicalFragment& other)
    : has_floating_descendants_for_paint_(
          other.has_floating_descendants_for_paint_),
      has_adjoining_object_descendants_(
          other.has_adjoining_object_descendants_),
      depends_on_percentage_block_size_(
          other.depends_on_percentage_block_size_),
      has_propagated_descendants_(other.has_propagated_descendants_),
      has_hanging_(other.has_hanging_),
      is_inline_formatting_context_(other.is_inline_formatting_context_),
      has_fragment_items_(other.has_fragment_items_),
      include_border_top_(other.include_border_top_),
      include_border_right_(other.include_border_right_),
      include_border_bottom_(other.include_border_bottom_),
      include_border_left_(other.include_border_left_),
      has_layout_overflow_(other.has_layout_overflow_),
      has_borders_(other.has_borders_),
      has_padding_(other.has_padding_),
      has_inflow_bounds_(other.has_inflow_bounds_),
      has_rare_data_(other.has_rare_data_),
      is_first_for_node_(other.is_first_for_node_),
      layout_object_(other.layout_object_),
      size_(other.size_),
      type_(other.type_),
      sub_type_(other.sub_type_),
      style_variant_(other.style_variant_),
      is_hidden_for_paint_(other.is_hidden_for_paint_),
      is_math_fraction_(other.is_math_fraction_),
      is_math_operator_(other.is_math_operator_),
      base_or_resolved_direction_(other.base_or_resolved_direction_),
      may_have_descendant_above_block_start_(
          other.may_have_descendant_above_block_start_),
      is_fieldset_container_(other.is_fieldset_container_),
      is_table_ng_part_(other.is_table_ng_part_),
      is_legacy_layout_root_(other.is_legacy_layout_root_),
      is_painted_atomically_(other.is_painted_atomically_),
      has_collapsed_borders_(other.has_collapsed_borders_),
      has_baseline_(other.has_baseline_),
      has_last_baseline_(other.has_last_baseline_),
      ink_overflow_computed_(other.ink_overflow_computed_) {
  CHECK(layout_object_);
}

// Keep the implementation of the destructor here, to avoid dependencies on
// ComputedStyle in the header file.
NGPhysicalFragment::~NGPhysicalFragment() = default;

void NGPhysicalFragment::Destroy() const {
  switch (Type()) {
    case kFragmentBox:
      delete static_cast<const NGPhysicalBoxFragment*>(this);
      break;
    case kFragmentText:
      delete static_cast<const NGPhysicalTextFragment*>(this);
      break;
    case kFragmentLineBox:
      delete static_cast<const NGPhysicalLineBoxFragment*>(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool NGPhysicalFragment::IsBlockFlow() const {
  return !IsLineBox() && layout_object_->IsLayoutBlockFlow();
}

bool NGPhysicalFragment::IsTextControlPlaceholder() const {
  return blink::IsTextControlPlaceholder(layout_object_->GetNode());
}

bool NGPhysicalFragment::IsPlacedByLayoutNG() const {
  // TODO(kojii): Move this to a flag for |LayoutNGBlockFlow::UpdateBlockLayout|
  // to set.
  if (IsLineBox())
    return false;
  if (IsFragmentainerBox())
    return true;
  const LayoutBlock* container = layout_object_->ContainingBlock();
  if (!container)
    return false;
  return container->IsLayoutNGMixin();
}

const FragmentData* NGPhysicalFragment::GetFragmentData() const {
  DCHECK(CanTraverse());
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
      if (IsColumnBox()) {
        // Column fragments are associated with the same layout object as their
        // multicol container. The fragments themselves are regular in-flow
        // block container fragments for most purposes.
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
    case kFragmentText:
      if (To<NGPhysicalTextFragment>(this)->IsGeneratedText()) {
        // Ellipsis has the truncated in-flow LayoutObject.
        DCHECK(layout_object_->IsText() ||
               (layout_object_->IsInline() &&
                layout_object_->IsAtomicInlineLevel()) ||
               layout_object_->IsLayoutInline());
      } else {
        DCHECK(layout_object_->IsText());
      }
      DCHECK(!IsFloating());
      DCHECK(!IsOutOfFlowPositioned());
      DCHECK(!IsInlineBox());
      DCHECK(!IsAtomicInline());
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

void NGPhysicalFragment::CheckCanUpdateInkOverflow() const {
  if (!GetLayoutObject())
    return;
  const DocumentLifecycle& lifecycle = GetDocument().Lifecycle();
  DCHECK(lifecycle.GetState() >= DocumentLifecycle::kLayoutClean &&
         lifecycle.GetState() < DocumentLifecycle::kCompositingAssignmentsClean)
      << lifecycle.GetState();
}
#endif

PhysicalRect NGPhysicalFragment::ScrollableOverflow(
    const NGPhysicalBoxFragment& container,
    TextHeightType height_type) const {
  switch (Type()) {
    case kFragmentBox:
      return To<NGPhysicalBoxFragment>(*this).ScrollableOverflow(height_type);
    case kFragmentText:
      if (height_type == TextHeightType::kNormalHeight)
        return {{}, Size()};
      return AdjustTextRectForEmHeight(
          LocalRect(), Style(),
          To<NGPhysicalTextFragment>(this)->TextShapeResult(),
          container.Style().GetWritingMode());
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
    TransformationMatrix transform;
    layout_object->GetTransformFromContainer(container_layout_object,
                                             PhysicalOffset(), transform);
    *overflow =
        PhysicalRect::EnclosingRect(transform.MapRect(FloatRect(*overflow)));
  }
}

const Vector<NGInlineItem>& NGPhysicalFragment::InlineItemsOfContainingBlock()
    const {
  DCHECK(IsInline());
  DCHECK(GetLayoutObject());
  LayoutBlockFlow* block_flow = GetLayoutObject()->ContainingNGBlockFlow();
  // TODO(xiaochengh): Code below is copied from ng_offset_mapping.cc with
  // modification. Unify them.
  DCHECK(block_flow);
  NGBlockNode block_node = NGBlockNode(block_flow);
  DCHECK(block_node.IsInlineFormattingContextRoot());
  DCHECK(block_node.CanUseNewLayout());
  NGLayoutInputNode node = block_node.FirstChild();

  // TODO(xiaochengh): Handle ::first-line.
  return To<NGInlineNode>(node).ItemsData(false).items;
}

TouchAction NGPhysicalFragment::EffectiveAllowedTouchAction() const {
  DCHECK(layout_object_);
  return layout_object_->EffectiveAllowedTouchAction();
}

bool NGPhysicalFragment::InsideBlockingWheelEventHandler() const {
  DCHECK(layout_object_);
  return layout_object_->InsideBlockingWheelEventHandler();
}

UBiDiLevel NGPhysicalFragment::BidiLevel() const {
  switch (Type()) {
    case kFragmentText:
      return To<NGPhysicalTextFragment>(*this).BidiLevel();
    case kFragmentBox:
      return To<NGPhysicalBoxFragment>(*this).BidiLevel();
    case kFragmentLineBox:
      break;
  }
  NOTREACHED();
  return 0;
}

TextDirection NGPhysicalFragment::ResolvedDirection() const {
  switch (Type()) {
    case kFragmentText:
      return To<NGPhysicalTextFragment>(*this).ResolvedDirection();
    case kFragmentBox:
      DCHECK(IsInline() && IsAtomicInline());
      // TODO(xiaochengh): Store direction in |base_direction_| flag.
      return DirectionFromLevel(BidiLevel());
    case kFragmentLineBox:
      break;
  }
  NOTREACHED();
  return TextDirection::kLtr;
}

bool NGPhysicalFragment::ShouldPaintCursorCaret() const {
  // TODO(xiaochengh): Merge cursor caret painting functions from LayoutBlock to
  // FrameSelection.
  if (const auto* block = DynamicTo<LayoutBlock>(GetLayoutObject()))
    return block->ShouldPaintCursorCaret();
  return false;
}

bool NGPhysicalFragment::ShouldPaintDragCaret() const {
  // TODO(xiaochengh): Merge drag caret painting functions from LayoutBlock to
  // DragCaret.
  if (const auto* block = DynamicTo<LayoutBlock>(GetLayoutObject()))
    return block->ShouldPaintDragCaret();
  return false;
}

LogicalRect NGPhysicalFragment::ConvertChildToLogical(
    const PhysicalRect& physical_rect) const {
  return WritingModeConverter(Style().GetWritingDirection(), Size())
      .ToLogical(physical_rect);
}

PhysicalRect NGPhysicalFragment::ConvertChildToPhysical(
    const LogicalRect& logical_rect) const {
  return WritingModeConverter(Style().GetWritingDirection(), Size())
      .ToPhysical(logical_rect);
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
    case kFragmentText: {
      const auto& text = To<NGPhysicalTextFragment>(*this);
      output.AppendFormat(", TextType: %u, Text: (%u,%u) \"", text.TextType(),
                          text.StartOffset(), text.EndOffset());
      output.Append(text.Text());
      output.Append("\"");
      break;
    }
    case kFragmentLineBox:
      break;
  }
  return output.ToString();
}

String NGPhysicalFragment::DumpFragmentTree(
    DumpFlags flags,
    base::Optional<PhysicalOffset> fragment_offset,
    unsigned indent) const {
  StringBuilder string_builder;
  if (flags & DumpHeaderText)
    string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  FragmentTreeDumper(&string_builder, flags)
      .Append(this, fragment_offset, indent);
  return string_builder.ToString();
}

#if DCHECK_IS_ON()
void NGPhysicalFragment::ShowFragmentTree() const {
  DumpFlags dump_flags = DumpAll;
  LOG(INFO) << "\n" << DumpFragmentTree(dump_flags).Utf8();
}
#endif

PhysicalRect NGPhysicalFragmentWithOffset::RectInContainerBox() const {
  return {offset_to_container_box, fragment->Size()};
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
