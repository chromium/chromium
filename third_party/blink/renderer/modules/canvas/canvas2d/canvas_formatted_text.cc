// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

void CanvasFormattedText::Trace(Visitor* visitor) const {
  visitor->Trace(text_runs_);
  ScriptWrappable::Trace(visitor);
}

CanvasFormattedText::CanvasFormattedText(Document* document) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetDisplay(EDisplay::kBlock);
  block_ =
      LayoutBlockFlow::CreateAnonymous(document, style, LegacyLayout::kAuto);
  block_->SetIsLayoutNGObjectForCanvasFormattedText(true);
}

void CanvasFormattedText::Dispose() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;
  if (block_)
    block_->Destroy();
}

LayoutBlockFlow* CanvasFormattedText::GetLayoutBlock(
    Document& document,
    const FontDescription& defaultFont) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetDisplay(EDisplay::kBlock);
  style->SetFontDescription(defaultFont);
  block_->SetStyle(style);
  return block_;
}

CanvasFormattedTextRun* CanvasFormattedText::appendRun(
    CanvasFormattedTextRun* run) {
  text_runs_.push_back(run);
  scoped_refptr<ComputedStyle> text_style = ComputedStyle::Create();
  text_style->SetDisplay(EDisplay::kInline);
  LayoutText* text =
      LayoutText::CreateAnonymous(block_->GetDocument(), std::move(text_style),
                                  run->text().Impl(), LegacyLayout::kAuto);
  text->SetIsLayoutNGObjectForCanvasFormattedText(true);
  block_->AddChild(text);
  return run;
}

sk_sp<PaintRecord> CanvasFormattedText::PaintFormattedText(
    Document& document,
    const FontDescription& font,
    double x,
    double y,
    double wrap_width,
    FloatRect& bounds) {
  LayoutBlockFlow* block = GetLayoutBlock(document, font);
  NGBlockNode block_node(block);
  NGInlineNode node(block);
  // Call IsEmptyInline to force prepare layout.
  if (node.IsEmptyInline())
    return nullptr;

  // TODO(sushraja) Once we add support for writing mode on the canvas formatted
  // text, fix this to be not hardcoded horizontal top to bottom.
  NGConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ true);
  LayoutUnit available_logical_width(wrap_width);
  LogicalSize available_size = {available_logical_width, kIndefiniteSize};
  builder.SetAvailableSize(available_size);
  NGConstraintSpace space = builder.ToConstraintSpace();
  scoped_refptr<const NGLayoutResult> block_results =
      block_node.Layout(space, nullptr);
  const auto& fragment =
      To<NGPhysicalBoxFragment>(block_results->PhysicalFragment());
  block->RecalcInlineChildrenVisualOverflow();
  bounds = FloatRect(block->PhysicalVisualOverflowRect());

  PaintController paint_controller(PaintController::Usage::kTransient);
  paint_controller.UpdateCurrentPaintChunkProperties(nullptr,
                                                     PropertyTreeState::Root());
  GraphicsContext graphics_context(paint_controller);
  PhysicalOffset physical_offset((LayoutUnit(x)), (LayoutUnit(y)));
  NGBoxFragmentPainter box_fragment_painter(fragment);
  PaintInfo paint_info(graphics_context, CullRect::Infinite(),
                       PaintPhase::kForeground, kGlobalPaintNormalPhase,
                       kPaintLayerPaintingRenderingClipPathAsMask |
                           kPaintLayerPaintingRenderingResourceSubtree);
  box_fragment_painter.PaintObject(paint_info, physical_offset);
  paint_controller.CommitNewDisplayItems();
  paint_controller.FinishCycle();
  sk_sp<PaintRecord> recording =
      paint_controller.GetPaintArtifact().GetPaintRecord(
          PropertyTreeState::Root());
  return recording;
}

}  // namespace blink
