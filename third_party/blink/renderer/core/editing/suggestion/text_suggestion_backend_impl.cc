// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_backend_impl.h"

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
TextSuggestionBackendImpl* TextSuggestionBackendImpl::From(LocalFrame& frame) {
  return frame.GetTextSuggestionBackendImpl();
}

// static
void TextSuggestionBackendImpl::Bind(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::TextSuggestionBackend> receiver) {
  DCHECK(frame);
  DCHECK(!TextSuggestionBackendImpl::From(*frame));
  auto* text_suggestion = MakeGarbageCollected<TextSuggestionBackendImpl>(
      base::PassKey<TextSuggestionBackendImpl>(), *frame, std::move(receiver));
  frame->SetTextSuggestionBackendImpl(text_suggestion);
}

TextSuggestionBackendImpl::TextSuggestionBackendImpl(
    base::PassKey<TextSuggestionBackendImpl>,
    LocalFrame& frame,
    mojo::PendingReceiver<mojom::blink::TextSuggestionBackend> receiver)
    : local_frame_(frame), receiver_(this, frame.DomWindow()) {
  receiver_.Bind(std::move(receiver),
                 frame.GetTaskRunner(TaskType::kInternalUserInteraction));
}

void TextSuggestionBackendImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(local_frame_);
}

void TextSuggestionBackendImpl::ApplySpellCheckSuggestion(
    const String& suggestion) {
  local_frame_->GetTextSuggestionController().ApplySpellCheckSuggestion(
      suggestion);
}

void TextSuggestionBackendImpl::ApplyTextSuggestion(int32_t marker_tag,
                                                    int32_t suggestion_index) {
  local_frame_->GetTextSuggestionController().ApplyTextSuggestion(
      marker_tag, suggestion_index);
}

void TextSuggestionBackendImpl::DeleteActiveSuggestionRange() {
  local_frame_->GetTextSuggestionController().DeleteActiveSuggestionRange();
}

void TextSuggestionBackendImpl::OnNewWordAddedToDictionary(const String& word) {
  local_frame_->GetTextSuggestionController().OnNewWordAddedToDictionary(word);
}

void TextSuggestionBackendImpl::OnSuggestionMenuClosed() {
  local_frame_->GetTextSuggestionController().OnSuggestionMenuClosed();
}

void TextSuggestionBackendImpl::SuggestionMenuTimeoutCallback(
    int32_t max_number_of_suggestions) {
  local_frame_->GetTextSuggestionController().SuggestionMenuTimeoutCallback(
      max_number_of_suggestions);
}

}  // namespace blink
