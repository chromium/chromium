// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/formatted_text/formatted_text.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_formatted_text_run.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_formattedtextrun_formattedtextrunorstringsequence_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_formattedtextrun_string.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
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

void FormattedText::Trace(Visitor* visitor) const {
  visitor->Trace(text_runs_);
  visitor->Trace(block_);
  ScriptWrappable::Trace(visitor);
  FormattedTextStyle::Trace(visitor);
}

FormattedText::FormattedText(ExecutionContext* execution_context) {
  // Refrain from extending the use of document, apart from creating layout
  // block flow. In the future we should handle execution_context's from worker
  // threads that do not have a document.
  auto* document = To<LocalDOMWindow>(execution_context)->document();
  ComputedStyleBuilder builder =
      document->GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetDisplay(EDisplay::kBlock);
  block_ = LayoutBlockFlow::CreateAnonymous(document, builder.TakeStyle());
  block_->SetIsLayoutNGObjectForFormattedText(true);
}

void FormattedText::Dispose() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;
  if (block_)
    block_->Destroy();
}

void FormattedText::UpdateComputedStylesIfNeeded(
    Document& document,
    const FontDescription& defaultFont) {
  auto style = document.GetStyleResolver().StyleForFormattedText(
      /*is_text_run*/ false, defaultFont, GetCssPropertySet());
  block_->SetStyle(style, LayoutObject::ApplyStyleChanges::kNo);
  block_->SetHorizontalWritingMode(style->IsHorizontalWritingMode());
  for (auto& text_run : text_runs_)
    text_run->UpdateStyle(document, /*parent_style*/ *style);
}

FormattedTextRunInternal* FormattedText::AppendRun(
    FormattedTextRunInternal* run,
    ExceptionState& exception_state) {
  if (!CheckViewExists(&exception_state))
    return nullptr;
  text_runs_.push_back(run);
  block_->AddChild(run->GetLayoutObject());
  return run;
}

FormattedText* FormattedText::format(
    ExecutionContext* execution_context,
    V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
    const String& style,
    ExceptionState& exception_state) {
  return FormattedText::FormatImpl(execution_context, text, style,
                                   kIndefiniteSize, kIndefiniteSize,
                                   exception_state);
}

FormattedText* FormattedText::format(
    ExecutionContext* execution_context,
    V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
    const String& style,
    double inline_constraint,
    ExceptionState& exception_state) {
  return FormattedText::FormatImpl(execution_context, text, style,
                                   LayoutUnit(std::max(0.0, inline_constraint)),
                                   kIndefiniteSize, exception_state);
}

FormattedText* FormattedText::format(
    ExecutionContext* execution_context,
    V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
    const String& style,
    double inline_constraint,
    double block_constraint,
    ExceptionState& exception_state) {
  return FormattedText::FormatImpl(execution_context, text, style,
                                   LayoutUnit(std::max(0.0, inline_constraint)),
                                   LayoutUnit(std::max(0.0, block_constraint)),
                                   exception_state);
}

FormattedText* FormattedText::FormatImpl(
    ExecutionContext* execution_context,
    V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString*
        text_runs,
    const String& style,
    LayoutUnit inline_constraint,
    LayoutUnit block_constraint,
    ExceptionState& exception_state) {
  // Create a new FormattedText object
  auto* formatted_text = MakeGarbageCollected<FormattedText>(execution_context);

  // Set the constraints
  formatted_text->inline_constraint_ = inline_constraint;
  formatted_text->block_constraint_ = block_constraint;

  // Helper function to lazily construct a CSSParserContext
  CSSParserContext* parser_context_instance = nullptr;
  auto parser_context = [&]() -> CSSParserContext* {
    if (!parser_context_instance) {
      parser_context_instance =
          MakeGarbageCollected<CSSParserContext>(*execution_context);
    }
    return parser_context_instance;
  };

  // Helper function to construct a single text run
  auto handle_single_run = [&](auto& single_run) {
    if (single_run->IsFormattedTextRun()) {
      // This is a dictionary object with 'text' and 'style' members
      const FormattedTextRun* text_run = single_run->GetAsFormattedTextRun();
      if (text_run->hasText() && !text_run->text().empty()) {
        auto* ft_run = MakeGarbageCollected<FormattedTextRunInternal>(
            execution_context, text_run->text());
        if (text_run->hasStyle() && !text_run->style().empty())
          ft_run->SetStyle(parser_context(), text_run->style());
        formatted_text->AppendRun(ft_run, exception_state);
      }
    } else {
      // This is a simple string, without any styles
      DCHECK(single_run->IsString());
      const String& text = single_run->GetAsString();
      if (!text.empty()) {
        auto* ft_run = MakeGarbageCollected<FormattedTextRunInternal>(
            execution_context, text);
        formatted_text->AppendRun(ft_run, exception_state);
      }
    }
  };

  // Handle the input runs (either a single run or an array of runs)
  if (text_runs) {
    if (text_runs->IsV8FormattedTextRunSingle()) {
      handle_single_run(text_runs);
    } else {
      DCHECK(text_runs->IsV8FormattedTextRunList());
      const auto& run_list = text_runs->GetAsFormattedTextRunOrStringSequence();
      for (auto&& text_run : run_list) {
        handle_single_run(text_run);
      }
    }
  }

  // Apply global styles
  if (!style.empty())
    formatted_text->SetStyle(parser_context(), style);
  return formatted_text;
}

PaintRecord FormattedText::PaintFormattedText(Document& document,
                                              const FontDescription& font,
                                              double x,
                                              double y,
                                              gfx::RectF& bounds,
                                              ExceptionState& exception_state) {
  if (!CheckViewExists(&exception_state))
    return PaintRecord();

  UpdateComputedStylesIfNeeded(document, font);
  NGBlockNode block_node(block_);

  NGConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {block_->StyleRef().GetWritingMode(), block_->StyleRef().Direction()},
      /* is_new_fc */ true);
  LogicalSize available_size = {inline_constraint_, block_constraint_};
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

bool FormattedText::CheckViewExists(ExceptionState* exception_state) const {
  if (!block_ || !block_->View()) {
    if (exception_state) {
      exception_state->ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The object is owned by a destroyed document.");
    }
    return false;
  }
  return true;
}

}  // namespace blink
