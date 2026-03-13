// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_WINDOW_SPELL_CHECK_CUSTOM_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_WINDOW_SPELL_CHECK_CUSTOM_DICTIONARY_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class SpellCheckCustomDictionary;
class LocalDOMWindow;

// The "spellCheckCustomDictionary" attribute on the Window global scope.
class WindowSpellCheckCustomDictionary {
  STATIC_ONLY(WindowSpellCheckCustomDictionary);

 public:
  static SpellCheckCustomDictionary* spellCheckCustomDictionary(
      LocalDOMWindow&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_WINDOW_SPELL_CHECK_CUSTOM_DICTIONARY_H_
