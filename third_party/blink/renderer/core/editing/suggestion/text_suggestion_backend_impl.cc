// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_backend_impl.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

TextSuggestionBackendImpl::TextSuggestionBackendImpl(LocalFrame& frame)
    : frame_(frame) {}

// static
void TextSuggestionBackendImpl::Create(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::TextSuggestionBackend> receiver) {
  mojo::MakeSelfOwnedReceiver(std::unique_ptr<TextSuggestionBackendImpl>(
                                  new TextSuggestionBackendImpl(*frame)),
                              std::move(receiver));
}

void TextSuggestionBackendImpl::ApplySpellCheckSuggestion(
    const WTF::String& suggestion) {
  if (frame_)
    frame_->GetTextSuggestionController().ApplySpellCheckSuggestion(suggestion);
}

void TextSuggestionBackendImpl::ApplyTextSuggestion(int32_t marker_tag,
                                                    int32_t suggestion_index) {
  if (frame_) {
    frame_->GetTextSuggestionController().ApplyTextSuggestion(marker_tag,
                                                              suggestion_index);
  }
}

void TextSuggestionBackendImpl::DeleteActiveSuggestionRange() {
  if (frame_)
    frame_->GetTextSuggestionController().DeleteActiveSuggestionRange();
}

void TextSuggestionBackendImpl::OnNewWordAddedToDictionary(
    const WTF::String& word) {
  if (frame_)
    frame_->GetTextSuggestionController().OnNewWordAddedToDictionary(word);
}

void TextSuggestionBackendImpl::OnSuggestionMenuClosed() {
  if (frame_)
    frame_->GetTextSuggestionController().OnSuggestionMenuClosed();
}

void TextSuggestionBackendImpl::SuggestionMenuTimeoutCallback(
    int32_t max_number_of_suggestions) {
  if (frame_) {
    frame_->GetTextSuggestionController().SuggestionMenuTimeoutCallback(
        max_number_of_suggestions);
  }
}

}  // namespace blink
