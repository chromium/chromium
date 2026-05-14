// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"

#include <optional>

#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

struct TextCheckEntryPoint {
  STACK_ALLOCATED();

 public:
  LocalFrame* frame;
  WebTextCheckClient* client;
};

std::optional<TextCheckEntryPoint> GetTextCheckEntryPoint(
    ScriptState* script_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || !execution_context->IsWindow()) {
    return std::nullopt;
  }
  LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
  if (!frame) {
    return std::nullopt;
  }
  WebTextCheckClient* client = frame->Client()->GetTextCheckerClient();
  if (!client || !client->IsSpellCheckingEnabled()) {
    return std::nullopt;
  }
  return TextCheckEntryPoint{frame, client};
}

}  // namespace

void SpellCheckCustomDictionary::addWords(ScriptState* script_state,
                                          const Vector<String>& words) {
  auto entry = GetTextCheckEntryPoint(script_state);
  if (!entry) {
    return;
  }
  std::vector<std::string> custom_words;
  custom_words.reserve(words.size());
  for (const auto& word : words) {
    custom_words.push_back(word.Utf8());
  }
  entry->client->SpellCheckCustomDictionaryChanged(/*words_added=*/custom_words,
                                                   /*words_removed=*/{});
}

void SpellCheckCustomDictionary::removeWords(ScriptState* script_state,
                                             const Vector<String>& words) {
  auto entry = GetTextCheckEntryPoint(script_state);
  if (!entry) {
    return;
  }
  std::vector<std::string> custom_words;
  custom_words.reserve(words.size());
  for (const auto& word : words) {
    custom_words.push_back(word.Utf8());
  }
  entry->client->SpellCheckCustomDictionaryChanged(
      /*words_added=*/{}, /*words_removed=*/custom_words);

  // Force a fresh spell-check pass on the document. The downstream
  // DictionaryUpdateObserver only reacts to words_added, so without an
  // explicit kick here removed words wouldn't get squiggles until the user
  // typed in each editable.
  //   1. InvalidateFullyCheckedRoots drops cold mode's "already fully
  //      checked" cache so its next pass re-walks every editable root, not
  //      just the focused one.
  //   2. RespondToChangedContents asks the idle controller to schedule a
  //      hot-mode pass on the next idle slice (and arm cold mode for the
  //      rest).
  SpellChecker& checker = entry->frame->GetSpellChecker();
  checker.GetIdleSpellCheckController()
      .GetColdModeRequester()
      .InvalidateFullyCheckedRoots();
  checker.RespondToChangedContents();
}

void SpellCheckCustomDictionary::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
