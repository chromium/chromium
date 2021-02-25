// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text_run.h"

namespace blink {

CanvasFormattedTextRun::CanvasFormattedTextRun(
    ExecutionContext* execution_context,
    const String text)
    : text_(text) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetDisplay(EDisplay::kInline);
  // Refrain from extending the use of document, apart from creating layout
  // text. In the future we should handle execution_context's from worker
  // threads that do not have a document.
  auto* window = To<LocalDOMWindow>(execution_context);
  layout_text_ =
      LayoutText::CreateAnonymous(*(window->document()), std::move(style),
                                  text.Impl(), LegacyLayout::kAuto);
  layout_text_->SetIsLayoutNGObjectForCanvasFormattedText(true);
}

void CanvasFormattedTextRun::Dispose() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;
  if (layout_text_)
    layout_text_->Destroy();
}

void CanvasFormattedTextRun::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
