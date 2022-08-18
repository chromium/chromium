// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
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
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

void CanvasFormattedText::Trace(Visitor* visitor) const {
  visitor->Trace(text_runs_);
  visitor->Trace(block_);
  ScriptWrappable::Trace(visitor);
  CanvasFormattedTextStyle::Trace(visitor);
}

CanvasFormattedText* CanvasFormattedText::Create(
    ExecutionContext* execution_context,
    const String text) {
  CanvasFormattedText* canvas_formatted_text =
      MakeGarbageCollected<CanvasFormattedText>(execution_context);
  CanvasFormattedTextRun* run =
      MakeGarbageCollected<CanvasFormattedTextRun>(execution_context, text);
  canvas_formatted_text->text_runs_.push_back(run);
  canvas_formatted_text->block_->AddChild(run->GetLayoutObject());
  return canvas_formatted_text;
}

CanvasFormattedText::CanvasFormattedText(ExecutionContext* execution_context)
    : CanvasFormattedTextStyle(/* is_text_run */ false) {
  // Refrain from extending the use of document, apart from creating layout
  // block flow. In the future we should handle execution_context's from worker
  // threads that do not have a document.
  auto* document = To<LocalDOMWindow>(execution_context)->document();
  scoped_refptr<ComputedStyle> style =
      document->GetStyleResolver().CreateComputedStyle();
  style->SetDisplay(EDisplay::kBlock);
  block_ =
      LayoutBlockFlow::CreateAnonymous(document, style, LegacyLayout::kAuto);
  block_->SetIsLayoutNGObjectForCanvasFormattedText(true);
}

void CanvasFormattedText::Dispose() {
  // Detach all the anonymous children we added, since block_->Destroy will
  // destroy them. We want the lifetime of the children to be managed by their
  // corresponding CanvasFormattedTextRun and not destroyed at this point.
  while (block_->FirstChild()) {
    block_->RemoveChild(block_->FirstChild());
  }
  AllowDestroyingLayoutObjectInFinalizerScope scope;
  if (block_)
    block_->Destroy();
}

void CanvasFormattedText::SetNeedsStyleRecalc() {
  needs_style_recalc_ = true;
}

void CanvasFormattedText::UpdateComputedStylesIfNeeded(
    Document& document,
    const FontDescription& defaultFont) {
  if (needs_style_recalc_ || current_default_font_ != defaultFont) {
    auto style = document.GetStyleResolver().StyleForCanvasFormattedText(
        /*is_text_run*/ false, defaultFont, GetCssPropertySet());
    block_->SetStyle(style, LayoutObject::ApplyStyleChanges::kNo);
    block_->SetHorizontalWritingMode(style->IsHorizontalWritingMode());
    for (auto& text_run : text_runs_)
      text_run->UpdateStyle(document, /*parent_style*/ *style);
    needs_style_recalc_ = false;
    current_default_font_ = defaultFont;
  }
}

CanvasFormattedTextRun* CanvasFormattedText::appendRun(
    CanvasFormattedTextRun* run,
    ExceptionState& exception_state) {
  if (!CheckRunIsNotParented(run, &exception_state) ||
      !CheckViewExists(run, &exception_state) ||
      !CheckRunBelongsToSameFrame(run, &exception_state))
    return nullptr;
  text_runs_.push_back(run);
  block_->AddChild(run->GetLayoutObject());
  run->SetParent(this);
  SetNeedsStyleRecalc();
  return run;
}

CanvasFormattedTextRun* CanvasFormattedText::setRun(
    unsigned index,
    CanvasFormattedTextRun* run,
    ExceptionState& exception_state) {
  if (!CheckRunsIndexBound(index, &exception_state) ||
      !CheckRunIsNotParented(run, &exception_state) ||
      !CheckViewExists(run, &exception_state) ||
      !CheckRunBelongsToSameFrame(run, &exception_state))
    return nullptr;
  run->SetParent(this);
  block_->AddChild(run->GetLayoutObject(),
                   text_runs_[index]->GetLayoutObject());
  text_runs_[index]->SetParent(nullptr);
  block_->RemoveChild(text_runs_[index]->GetLayoutObject());
  text_runs_[index] = run;
  SetNeedsStyleRecalc();
  return text_runs_[index];
}

CanvasFormattedTextRun* CanvasFormattedText::insertRun(
    unsigned index,
    CanvasFormattedTextRun* run,
    ExceptionState& exception_state) {
  if (!CheckRunIsNotParented(run, &exception_state))
    return nullptr;
  if (index == text_runs_.size())
    return appendRun(run, exception_state);
  if (!CheckRunsIndexBound(index, &exception_state) ||
      !CheckViewExists(run, &exception_state) ||
      !CheckRunBelongsToSameFrame(run, &exception_state))
    return nullptr;
  block_->AddChild(run->GetLayoutObject(),
                   text_runs_[index]->GetLayoutObject());
  text_runs_.insert(index, run);
  run->SetParent(this);
  SetNeedsStyleRecalc();
  return text_runs_[index];
}

void CanvasFormattedText::deleteRun(unsigned index,
                                    unsigned length,
                                    ExceptionState& exception_state) {
  if (!CheckRunsIndexBound(index, &exception_state))
    return;
  // Protect against overflow, do not perform math like index + length <
  // text_runs_.size(). The length passed in can be close to INT_MAX.
  if (text_runs_.size() - index < length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("length", length,
                                                    text_runs_.size() - index));
    return;
  }

  for (wtf_size_t i = index; i < index + length; i++) {
    text_runs_[i]->SetParent(nullptr);
    block_->RemoveChild(text_runs_[i]->GetLayoutObject());
  }
  block_->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kCanvasFormattedTextRunChange);
  text_runs_.EraseAt(static_cast<wtf_size_t>(index),
                     static_cast<wtf_size_t>(length));
}

sk_sp<PaintRecord> CanvasFormattedText::PaintFormattedText(
    Document& document,
    const FontDescription& font,
    double x,
    double y,
    double wrap_width,
    double wrap_height,
    gfx::RectF& bounds) {
  UpdateComputedStylesIfNeeded(document, font);
  NGBlockNode block_node(block_);

  NGConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {block_->StyleRef().GetWritingMode(), block_->StyleRef().Direction()},
      /* is_new_fc */ true);
  LayoutUnit available_logical_width(std::max(wrap_width, 0.0));
  LayoutUnit available_logical_height(std::max(wrap_height, 0.0));
  LogicalSize available_size = {available_logical_width,
                                available_logical_height};
  builder.SetAvailableSize(available_size);
  NGConstraintSpace space = builder.ToConstraintSpace();
  const NGLayoutResult* block_results = block_node.Layout(space, nullptr);
  const auto& fragment =
      To<NGPhysicalBoxFragment>(block_results->PhysicalFragment());
  block_->RecalcFragmentsVisualOverflow();
  bounds = gfx::RectF{block_->PhysicalVisualOverflowRect()};
  auto* paint_record_builder = MakeGarbageCollected<PaintRecordBuilder>();
  PaintInfo paint_info(paint_record_builder->Context(), CullRect::Infinite(),
                       PaintPhase::kForeground);
  NGBoxFragmentPainter(fragment).PaintObject(
      paint_info, PhysicalOffset(LayoutUnit(x), LayoutUnit(y)));
  return paint_record_builder->EndRecording();
}

bool CanvasFormattedText::CheckViewExists(
    CanvasFormattedTextRun* run,
    ExceptionState* exception_state) const {
  if (!block_ || !block_->View()) {
    if (exception_state) {
      exception_state->ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The object is owned by a destroyed document.");
    }
    return false;
  }
  if (run) {
    if (!run->GetLayoutObject() || !run->GetLayoutObject()->View()) {
      if (exception_state) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The run is owned by a destroyed document.");
      }
      return false;
    }
  }
  return true;
}

bool CanvasFormattedText::CheckRunBelongsToSameFrame(
    CanvasFormattedTextRun* run,
    ExceptionState* exception_state) const {
  if (run->GetLayoutObject()->GetDocument() != block_->GetDocument()) {
    if (exception_state) {
      exception_state->ThrowDOMException(
          DOMExceptionCode::kWrongDocumentError,
          "The run is owned by a different document.");
    }
    return false;
  }
  return true;
}

}  // namespace blink
