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
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

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

// True if `word` is well-formed UTF-16.
bool IsWellFormed(const String& word) {
  return word.Is8Bit() || blink::IsWellFormed(word.Span16());
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
    // Reject ill-formed entries before they reach the spellchecker, matching
    // the browser custom dictionary's IsValidWord(): skip empty words and words
    // padded with leading or trailing whitespace.
    // It also rejects ill-formed UTF-16 (unpaired surrogates) rather than
    // converting it to a U+FFFD-mangled entry.
    if (word.empty() || word.length() != word.LengthWithStrippedWhiteSpace() ||
        !IsWellFormed(word)) {
      continue;
    }
    // Only well-formed UTF-16 reaches here.
    custom_words.push_back(
        word.Utf8(Utf8ConversionMode::kStrictReplacingErrors));
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
    // Unlike addWords(), there's no need to skip empty or whitespace-padded
    // words here: addWords() never stores them, so a removal request for one
    // matches nothing and is a harmless no-op. Their UTF-8 conversion is
    // identity, so they can't collide with a stored entry either.
    //
    // Ill-formed UTF-16 is different and must be rejected:
    // kStrictReplacingErrors rewrites an unpaired surrogate to U+FFFD, a valid
    // character a user could have legitimately added. Forwarding such a word
    // could match and remove that unrelated entry.
    if (!IsWellFormed(word)) {
      continue;
    }
    custom_words.push_back(
        word.Utf8(Utf8ConversionMode::kStrictReplacingErrors));
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
