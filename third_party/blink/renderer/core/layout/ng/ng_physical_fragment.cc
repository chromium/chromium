// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

bool AppendFragmentOffsetAndSize(
    const NGPhysicalFragment* fragment,
    base::Optional<NGPhysicalOffset> fragment_offset,
    StringBuilder* builder,
    NGPhysicalFragment::DumpFlags flags,
    bool has_content) {
  if (flags & NGPhysicalFragment::DumpOffset) {
    if (has_content)
      builder->Append(" ");
    builder->Append("offset:");
    if (fragment_offset)
      builder->Append(fragment_offset->ToString());
    else
      builder->Append("unplaced");
    has_content = true;
  }
  if (flags & NGPhysicalFragment::DumpSize) {
    if (has_content)
      builder->Append(" ");
    builder->Append("size:");
    builder->Append(fragment->Size().ToString());
    has_content = true;
  }
  if (flags & NGPhysicalFragment::DumpOverflow) {
    if (has_content)
      builder->Append(" ");
    NGPhysicalOffsetRect overflow = fragment->InkOverflow();
    if (overflow.size.width != fragment->Size().width ||
        overflow.size.height != fragment->Size().height) {
      builder->Append(" InkOverflow: ");
      builder->Append(overflow.ToString());
      has_content = true;
    }
    if (has_content)
      builder->Append(" ");
    overflow = fragment->SelfInkOverflow();
    if (overflow.size.width != fragment->Size().width ||
        overflow.size.height != fragment->Size().height) {
      builder->Append(" visualRect: ");
      builder->Append(overflow.ToString());
      has_content = true;
    }
  }
  return has_content;
}

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
  }
  if (fragment.IsOldLayoutRoot()) {
    if (result.length())
      result.Append(" ");
    result.Append("old-layout-root");
  }
  if (fragment.IsBlockFlow()) {
    if (result.length())
      result.Append(" ");
    result.Append("block-flow");
  }
  if (fragment.IsRenderedLegend()) {
    if (result.length())
      result.Append(" ");
    result.Append("rendered-legend");
  }
  if (fragment.IsFieldsetContainer()) {
    if (result.length())
      result.Append(" ");
    result.Append("fieldset-container");
  }
  if (fragment.IsBox() &&
      static_cast<const NGPhysicalBoxFragment&>(fragment).ChildrenInline()) {
    if (result.length())
      result.Append(" ");
    result.Append("children-inline");
  }

  return result.ToString();
}

void AppendFragmentToString(const NGPhysicalFragment* fragment,
                            base::Optional<NGPhysicalOffset> fragment_offset,
                            StringBuilder* builder,
                            NGPhysicalFragment::DumpFlags flags,
                            unsigned indent = 2) {
  if (flags & NGPhysicalFragment::DumpIndentation) {
    for (unsigned i = 0; i < indent; i++)
      builder->Append(" ");
  }

  bool has_content = false;
  if (fragment->IsBox() || fragment->IsRenderedLegend()) {
    const auto* box = ToNGPhysicalBoxFragment(fragment);
    if (flags & NGPhysicalFragment::DumpType) {
      if (fragment->IsRenderedLegend())
        builder->Append("RenderedLegend");
      else
        builder->Append("Box");
      String box_type = StringForBoxType(*fragment);
      has_content = true;
      if (!box_type.IsEmpty()) {
        builder->Append(" (");
        builder->Append(box_type);
        builder->Append(")");
      }
      if (flags & NGPhysicalFragment::DumpSelfPainting &&
          box->HasSelfPaintingLayer()) {
        if (box_type.IsEmpty())
          builder->Append(" ");
        builder->Append("(self paint)");
      }
    }
    has_content = AppendFragmentOffsetAndSize(fragment, fragment_offset,
                                              builder, flags, has_content);

    if (flags & NGPhysicalFragment::DumpNodeName &&
        fragment->GetLayoutObject()) {
      if (has_content)
        builder->Append(" ");
      builder->Append(fragment->GetLayoutObject()->DebugName());
    }
    builder->Append("\n");

    if (flags & NGPhysicalFragment::DumpSubtree) {
      for (auto& child : box->Children()) {
        AppendFragmentToString(child.get(), child.Offset(), builder, flags,
                               indent + 2);
      }
    }
    return;
  }

  if (fragment->IsLineBox()) {
    if (flags & NGPhysicalFragment::DumpType) {
      builder->Append("LineBox");
      has_content = true;
    }
    has_content = AppendFragmentOffsetAndSize(fragment, fragment_offset,
                                              builder, flags, has_content);
    builder->Append("\n");

    if (flags & NGPhysicalFragment::DumpSubtree) {
      const auto* line_box = ToNGPhysicalLineBoxFragment(fragment);
      for (auto& child : line_box->Children()) {
        AppendFragmentToString(child.get(), child.Offset(), builder, flags,
                               indent + 2);
      }
      return;
    }
  }

  if (fragment->IsText()) {
    if (flags & NGPhysicalFragment::DumpType) {
      builder->Append("Text");
      has_content = true;
    }
    has_content = AppendFragmentOffsetAndSize(fragment, fragment_offset,
                                              builder, flags, has_content);

    if (flags & NGPhysicalFragment::DumpTextOffsets) {
      const auto* text = ToNGPhysicalTextFragment(fragment);
      if (has_content)
        builder->Append(" ");
      builder->Append("start: ");
      builder->Append(String::Format("%u", text->StartOffset()));
      builder->Append(" end: ");
      builder->Append(String::Format("%u", text->EndOffset()));
      has_content = true;
    }
    builder->Append("\n");
    return;
  }

  if (flags & NGPhysicalFragment::DumpType) {
    builder->Append("Unknown fragment type");
    has_content = true;
  }
  has_content = AppendFragmentOffsetAndSize(fragment, fragment_offset, builder,
                                            flags, has_content);
  builder->Append("\n");
}

LayoutUnit BorderWidth(unsigned edges, unsigned edge, float border_width) {
  return (edges & edge) ? LayoutUnit(border_width) : LayoutUnit();
}

}  // namespace

// static
void NGPhysicalFragmentTraits::Destruct(const NGPhysicalFragment* fragment) {
  fragment->Destroy();
}

NGPhysicalFragment::NGPhysicalFragment(NGFragmentBuilder* builder,
                                       NGFragmentType type,
                                       unsigned sub_type)
    : layout_object_(builder->layout_object_),
      style_(builder->style_),
      size_(builder->size_.ConvertToPhysical(builder->GetWritingMode())),
      break_token_(std::move(builder->break_token_)),
      type_(type),
      sub_type_(sub_type),
      is_fieldset_container_(false),
      is_old_layout_root_(false),
      style_variant_((unsigned)builder->style_variant_) {}

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       const ComputedStyle& style,
                                       NGStyleVariant style_variant,
                                       NGPhysicalSize size,
                                       NGFragmentType type,
                                       unsigned sub_type,
                                       scoped_refptr<NGBreakToken> break_token)
    : layout_object_(layout_object),
      style_(&style),
      size_(size),
      break_token_(std::move(break_token)),
      type_(type),
      sub_type_(sub_type),
      is_fieldset_container_(false),
      is_old_layout_root_(false),
      style_variant_((unsigned)style_variant) {}

// Keep the implementation of the destructor here, to avoid dependencies on
// ComputedStyle in the header file.
NGPhysicalFragment::~NGPhysicalFragment() = default;

void NGPhysicalFragment::Destroy() const {
  switch (Type()) {
    case kFragmentBox:
    case kFragmentRenderedLegend:
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

const ComputedStyle& NGPhysicalFragment::Style() const {
  DCHECK(style_);
  // TODO(kojii): Returning |style_| locks the style at the layout time, and
  // will not be updated when its base style is updated later. Line styles and
  // ellipsis styles have this problem.
  if (!GetLayoutObject())
    return *style_;
  switch (StyleVariant()) {
    case NGStyleVariant::kStandard:
      return *GetLayoutObject()->Style();
    case NGStyleVariant::kFirstLine:
      return *GetLayoutObject()->FirstLineStyle();
    case NGStyleVariant::kEllipsis:
      return *style_;
  }
  return *style_;
}

Node* NGPhysicalFragment::GetNode() const {
  return layout_object_ ? layout_object_->GetNode() : nullptr;
}

bool NGPhysicalFragment::HasLayer() const {
  return layout_object_ && layout_object_->HasLayer();
}

PaintLayer* NGPhysicalFragment::Layer() const {
  if (!HasLayer())
    return nullptr;

  // If the underlying LayoutObject has a layer it's guranteed to be a
  // LayoutBoxModelObject.
  return static_cast<LayoutBoxModelObject*>(layout_object_)->Layer();
}

bool NGPhysicalFragment::IsBlockFlow() const {
  return layout_object_ && layout_object_->IsLayoutBlockFlow();
}

bool NGPhysicalFragment::IsListMarker() const {
  return layout_object_ && layout_object_->IsLayoutNGListMarker();
}

bool NGPhysicalFragment::IsPlacedByLayoutNG() const {
  // TODO(kojii): Move this to a flag for |LayoutNGBlockFlow::UpdateBlockLayout|
  // to set.
  if (!layout_object_)
    return false;
  const LayoutBlock* container = layout_object_->ContainingBlock();
  if (!container)
    return false;
  return container->IsLayoutNGMixin() || container->IsLayoutNGFlexibleBox();
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalFragment::BorderWidths() const {
  unsigned edges = BorderEdges();
  NGPhysicalBoxStrut box_strut(
      BorderWidth(edges, NGBorderEdges::kTop, Style().BorderTopWidth()),
      BorderWidth(edges, NGBorderEdges::kRight, Style().BorderRightWidth()),
      BorderWidth(edges, NGBorderEdges::kBottom, Style().BorderBottomWidth()),
      BorderWidth(edges, NGBorderEdges::kLeft, Style().BorderLeftWidth()));
  return box_strut.SnapToDevicePixels();
}

NGPhysicalOffsetRect NGPhysicalFragment::SelfInkOverflow() const {
  switch (Type()) {
    case kFragmentBox:
    case kFragmentRenderedLegend:
      return ToNGPhysicalBoxFragment(*this).SelfInkOverflow();
    case kFragmentText:
      return ToNGPhysicalTextFragment(*this).SelfInkOverflow();
    case kFragmentLineBox:
      return {{}, Size()};
  }
  NOTREACHED();
  return {{}, Size()};
}

NGPhysicalOffsetRect NGPhysicalFragment::InkOverflow(bool apply_clip) const {
  switch (Type()) {
    case kFragmentBox:
    case kFragmentRenderedLegend:
      return ToNGPhysicalBoxFragment(*this).InkOverflow(apply_clip);
    case kFragmentText:
      return ToNGPhysicalTextFragment(*this).SelfInkOverflow();
    case kFragmentLineBox:
      return ToNGPhysicalLineBoxFragment(*this).InkOverflow();
  }
  NOTREACHED();
  return {{}, Size()};
}

NGPhysicalOffsetRect NGPhysicalFragment::ScrollableOverflow() const {
  switch (Type()) {
    case kFragmentBox:
    case kFragmentRenderedLegend:
      return ToNGPhysicalBoxFragment(*this).ScrollableOverflow();
    case kFragmentText:
      return {{}, Size()};
    case kFragmentLineBox:
      NOTREACHED()
          << "You must call NGLineBoxFragment::ScrollableOverflow explicitly.";
      break;
  }
  NOTREACHED();
  return {{}, Size()};
}

void NGPhysicalFragment::PropagateContentsInkOverflow(
    NGPhysicalOffsetRect* parent_ink_overflow,
    NGPhysicalOffset fragment_offset) const {
  // Add in visual overflow from the child.  Even if the child clips its
  // overflow, it may still have visual overflow of its own set from box shadows
  // or reflections. It is unnecessary to propagate this overflow if we are
  // clipping our own overflow.
  if (IsBox() && ToNGPhysicalBoxFragment(*this).HasSelfPaintingLayer())
    return;

  NGPhysicalOffsetRect ink_overflow = InkOverflow();
  ink_overflow.offset += fragment_offset;
  parent_ink_overflow->Unite(ink_overflow);
}

const Vector<NGInlineItem>& NGPhysicalFragment::InlineItemsOfContainingBlock()
    const {
  DCHECK(IsInline());
  DCHECK(GetLayoutObject());
  LayoutBlockFlow* block_flow = GetLayoutObject()->ContainingNGBlockFlow();
  // TODO(xiaochengh): Code below is copied from ng_offset_mapping.cc with
  // modification. Unify them.
  DCHECK(block_flow);
  DCHECK(block_flow->ChildrenInline());
  NGBlockNode block_node = NGBlockNode(block_flow);
  DCHECK(block_node.CanUseNewLayout());
  NGLayoutInputNode node = block_node.FirstChild();
  DCHECK(node);
  DCHECK(node.IsInline());

  // TODO(xiaochengh): Handle ::first-line.
  return ToNGInlineNode(node).ItemsData(false).items;
}

TouchAction NGPhysicalFragment::EffectiveWhitelistedTouchAction() const {
  DCHECK(GetLayoutObject());
  return GetLayoutObject()->EffectiveWhitelistedTouchAction();
}

UBiDiLevel NGPhysicalFragment::BidiLevel() const {
  NOTREACHED();
  return 0;
}

TextDirection NGPhysicalFragment::ResolvedDirection() const {
  DCHECK(IsInline());
  DCHECK(IsText() || IsAtomicInline());
  // TODO(xiaochengh): Store direction in |base_direction_| flag.
  return DirectionFromLevel(BidiLevel());
}

String NGPhysicalFragment::ToString() const {
  StringBuilder output;
  output.Append(String::Format("Type: '%d' Size: '%s'", Type(),
                               Size().ToString().Ascii().data()));
  switch (Type()) {
    case kFragmentBox:
    case kFragmentRenderedLegend:
      output.Append(String::Format(", BoxType: '%s'",
                                   StringForBoxType(*this).Ascii().data()));
      break;
    case kFragmentText: {
      const NGPhysicalTextFragment& text = ToNGPhysicalTextFragment(*this);
      output.Append(String::Format(", TextType: %u, Text: (%u,%u) \"",
                                   text.TextType(), text.StartOffset(),
                                   text.EndOffset()));
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
    base::Optional<NGPhysicalOffset> fragment_offset,
    unsigned indent) const {
  StringBuilder string_builder;
  if (flags & DumpHeaderText)
    string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  AppendFragmentToString(this, fragment_offset, &string_builder, flags, indent);
  return string_builder.ToString();
}

#ifndef NDEBUG
void NGPhysicalFragment::ShowFragmentTree() const {
  DumpFlags dump_flags = DumpAll;  // & ~DumpOverflow;
  LOG(INFO) << "\n" << DumpFragmentTree(dump_flags).Utf8().data();
}
#endif  // !NDEBUG

NGPhysicalOffsetRect NGPhysicalFragmentWithOffset::RectInContainerBox() const {
  return {offset_to_container_box, fragment->Size()};
}

}  // namespace blink
