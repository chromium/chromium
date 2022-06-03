// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text_run.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

namespace blink {

CanvasFormattedTextRun::CanvasFormattedTextRun(
    ExecutionContext* execution_context,
    const String text)
    : text_(text) {
  // Refrain from extending the use of document, apart from creating layout
  // text. In the future we should handle execution_context's from worker
  // threads that do not have a document.
  auto* document = To<LocalDOMWindow>(execution_context)->document();
  scoped_refptr<ComputedStyle> style =
      document->GetStyleResolver().CreateComputedStyle();
  style->SetDisplay(EDisplay::kInline);
  layout_text_ = LayoutText::CreateAnonymous(*document, std::move(style),
                                             text.Impl(), LegacyLayout::kAuto);
  layout_text_->SetIsLayoutNGObjectForCanvasFormattedText(true);
}

void CanvasFormattedTextRun::Dispose() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;
  if (layout_text_)
    layout_text_->Destroy();
}

void CanvasFormattedTextRun::Trace(Visitor* visitor) const {
  visitor->Trace(layout_text_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
