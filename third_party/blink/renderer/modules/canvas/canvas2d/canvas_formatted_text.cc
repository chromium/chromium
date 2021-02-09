// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

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

CanvasFormattedText::~CanvasFormattedText() {
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

}  // namespace blink
