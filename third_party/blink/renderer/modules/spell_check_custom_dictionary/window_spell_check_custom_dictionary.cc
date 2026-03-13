// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/window_spell_check_custom_dictionary.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class WindowSpellCheckCustomDictionaryImpl final
    : public GarbageCollected<WindowSpellCheckCustomDictionaryImpl>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static WindowSpellCheckCustomDictionaryImpl& From(LocalDOMWindow& window) {
    WindowSpellCheckCustomDictionaryImpl* supplement =
        Supplement<LocalDOMWindow>::template From<
            WindowSpellCheckCustomDictionaryImpl>(window);
    if (!supplement) {
      supplement =
          MakeGarbageCollected<WindowSpellCheckCustomDictionaryImpl>(window);
      Supplement<LocalDOMWindow>::ProvideTo(window, supplement);
    }
    return *supplement;
  }

  explicit WindowSpellCheckCustomDictionaryImpl(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window) {}

  SpellCheckCustomDictionary* GetOrCreate(LocalDOMWindow& fetching_scope) {
    if (!spell_check_custom_dictionary_) {
      spell_check_custom_dictionary_ =
          MakeGarbageCollected<SpellCheckCustomDictionary>();
    }
    return spell_check_custom_dictionary_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(spell_check_custom_dictionary_);
    Supplement<LocalDOMWindow>::Trace(visitor);
  }

 private:
  Member<SpellCheckCustomDictionary> spell_check_custom_dictionary_;
};

// static
const char WindowSpellCheckCustomDictionaryImpl::kSupplementName[] =
    "WindowSpellCheckCustomDictionaryImpl";

}  // namespace

SpellCheckCustomDictionary*
WindowSpellCheckCustomDictionary::spellCheckCustomDictionary(
    LocalDOMWindow& window) {
  return WindowSpellCheckCustomDictionaryImpl::From(window).GetOrCreate(window);
}

}  // namespace blink
