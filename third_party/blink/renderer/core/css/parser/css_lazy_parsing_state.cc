// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

CSSLazyParsingState::CSSLazyParsingState(const CSSParserContext* context,
                                         const String& sheet_text,
                                         StyleSheetContents* contents)
    : context_(context),
      sheet_text_(sheet_text),
      owning_contents_(contents),
      should_use_count_(context_->IsUseCounterRecordingEnabled()) {}

const CSSParserContext* CSSLazyParsingState::Context() {
  DCHECK(owning_contents_);
  if (!should_use_count_) {
    DCHECK(!context_->IsUseCounterRecordingEnabled());
    return context_.Get();
  }

  // Try as best as possible to grab a valid Document if the old Document has
  // gone away so we can still use UseCounter.
  if (!document_) {
    document_ = owning_contents_->AnyOwnerDocument();
  }

  if (!context_->IsDocumentHandleEqual(document_)) {
    context_ = MakeGarbageCollected<CSSParserContext>(context_, document_);
  }
  return context_.Get();
}

void CSSLazyParsingState::Trace(Visitor* visitor) const {
  visitor->Trace(owning_contents_);
  visitor->Trace(document_);
  visitor->Trace(context_);
}

}  // namespace blink
