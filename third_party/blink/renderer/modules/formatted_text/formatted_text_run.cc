// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/formatted_text/formatted_text_run.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/modules/formatted_text/formatted_text.h"

namespace blink {

FormattedTextRunInternal::FormattedTextRunInternal(
    ExecutionContext* execution_context,
    const String text)
    : text_(text) {
  // Refrain from extending the use of document, apart from creating layout
  // text. In the future we should handle execution_context's from worker
  // threads that do not have a document.
  auto* document = To<LocalDOMWindow>(execution_context)->document();
  ComputedStyleBuilder builder =
      document->GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetDisplay(EDisplay::kInline);
  layout_text_ = LayoutText::CreateAnonymousForFormattedText(
      *document, builder.TakeStyle(), text.Impl());
  layout_text_->SetIsLayoutNGObjectForFormattedText(true);
}

void FormattedTextRunInternal::UpdateStyle(Document& document,
                                           const ComputedStyle& parent_style) {
  const ComputedStyle* style =
      document.GetStyleResolver().StyleForFormattedText(
          /*is_text_run*/ true, parent_style, GetCssPropertySet());
  layout_text_->SetStyle(style, LayoutObject::ApplyStyleChanges::kNo);
}

void FormattedTextRunInternal::Trace(Visitor* visitor) const {
  visitor->Trace(layout_text_);
  FormattedTextStyle::Trace(visitor);
}

}  // namespace blink
