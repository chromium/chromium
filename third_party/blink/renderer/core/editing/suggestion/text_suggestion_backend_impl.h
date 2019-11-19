// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SUGGESTION_TEXT_SUGGESTION_BACKEND_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SUGGESTION_TEXT_SUGGESTION_BACKEND_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/input/input_messages.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class LocalFrame;

// Implementation of mojom::blink::TextSuggestionBackend
class CORE_EXPORT TextSuggestionBackendImpl final
    : public mojom::blink::TextSuggestionBackend {
 public:
  static void Create(
      LocalFrame*,
      mojo::PendingReceiver<mojom::blink::TextSuggestionBackend>);

  void ApplySpellCheckSuggestion(const String& suggestion) final;
  void ApplyTextSuggestion(int32_t marker_tag, int32_t suggestion_index) final;
  void DeleteActiveSuggestionRange() final;
  void OnNewWordAddedToDictionary(const String& word) final;
  void OnSuggestionMenuClosed() final;
  void SuggestionMenuTimeoutCallback(int32_t max_number_of_suggestions) final;

 private:
  explicit TextSuggestionBackendImpl(LocalFrame&);

  WeakPersistent<LocalFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(TextSuggestionBackendImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SUGGESTION_TEXT_SUGGESTION_BACKEND_IMPL_H_
