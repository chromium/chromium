// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_backend_impl.h"

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
const char TextSuggestionBackendImpl::kSupplementName[] =
    "TextSuggestionBackendImpl";

// static
TextSuggestionBackendImpl* TextSuggestionBackendImpl::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<TextSuggestionBackendImpl>(frame);
}

// static
void TextSuggestionBackendImpl::Bind(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::TextSuggestionBackend> receiver) {
  DCHECK(frame);
  DCHECK(!TextSuggestionBackendImpl::From(*frame));
  auto* text_suggestion = MakeGarbageCollected<TextSuggestionBackendImpl>(
      base::PassKey<TextSuggestionBackendImpl>(), *frame, std::move(receiver));
  Supplement<LocalFrame>::ProvideTo(*frame, text_suggestion);
}

TextSuggestionBackendImpl::TextSuggestionBackendImpl(
    base::PassKey<TextSuggestionBackendImpl>,
    LocalFrame& frame,
    mojo::PendingReceiver<mojom::blink::TextSuggestionBackend> receiver)
    : Supplement<LocalFrame>(frame), receiver_(this, frame.DomWindow()) {
  receiver_.Bind(std::move(receiver),
                 frame.GetTaskRunner(TaskType::kInternalUserInteraction));
}

void TextSuggestionBackendImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<LocalFrame>::Trace(visitor);
}

void TextSuggestionBackendImpl::ApplySpellCheckSuggestion(
    const WTF::String& suggestion) {
  GetSupplementable()->GetTextSuggestionController().ApplySpellCheckSuggestion(
      suggestion);
}

void TextSuggestionBackendImpl::ApplyTextSuggestion(int32_t marker_tag,
                                                    int32_t suggestion_index) {
  GetSupplementable()->GetTextSuggestionController().ApplyTextSuggestion(
      marker_tag, suggestion_index);
}

void TextSuggestionBackendImpl::DeleteActiveSuggestionRange() {
  GetSupplementable()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();
}

void TextSuggestionBackendImpl::OnNewWordAddedToDictionary(
    const WTF::String& word) {
  GetSupplementable()->GetTextSuggestionController().OnNewWordAddedToDictionary(
      word);
}

void TextSuggestionBackendImpl::OnSuggestionMenuClosed() {
  GetSupplementable()->GetTextSuggestionController().OnSuggestionMenuClosed();
}

void TextSuggestionBackendImpl::SuggestionMenuTimeoutCallback(
    int32_t max_number_of_suggestions) {
  GetSupplementable()
      ->GetTextSuggestionController()
      .SuggestionMenuTimeoutCallback(max_number_of_suggestions);
}

}  // namespace blink
