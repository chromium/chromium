// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"

#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

WebTextCheckClient* GetTextCheckClient(ScriptState* script_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsWindow()) {
    Document* document = To<LocalDOMWindow>(execution_context)->document();
    if (document->GetFrame()) {
      WebTextCheckClient* client =
          document->GetFrame()->Client()->GetTextCheckerClient();
      if (client && client->IsSpellCheckingEnabled()) {
        return client;
      }
    }
  }
  return nullptr;
}

}  // namespace

void SpellCheckCustomDictionary::addWords(ScriptState* script_state,
                                          const Vector<String>& words) {
  if (auto* client = GetTextCheckClient(script_state)) {
    std::vector<std::string> custom_words;
    for (const auto& word : words) {
      custom_words.push_back(word.Utf8());
    }
    client->SpellCheckCustomDictionaryChanged(custom_words, {""});
  }
}

void SpellCheckCustomDictionary::removeWords(ScriptState* script_state,
                                             const Vector<String>& words) {
  if (auto* client = GetTextCheckClient(script_state)) {
    std::vector<std::string> custom_words;
    for (const auto& word : words) {
      custom_words.push_back(word.Utf8());
    }
    client->SpellCheckCustomDictionaryChanged({""}, custom_words);
  }
}

void SpellCheckCustomDictionary::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
