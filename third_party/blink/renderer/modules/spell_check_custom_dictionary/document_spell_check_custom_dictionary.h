// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_DOCUMENT_SPELL_CHECK_CUSTOM_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_DOCUMENT_SPELL_CHECK_CUSTOM_DICTIONARY_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class SpellCheckCustomDictionary;

// `document.spellCheckCustomDictionary` IDL entry point. Returns the
// per-Document SpellCheckCustomDictionary instance (lazy, cached via
// Supplement<Document>).
class MODULES_EXPORT DocumentSpellCheckCustomDictionary {
  STATIC_ONLY(DocumentSpellCheckCustomDictionary);

 public:
  static SpellCheckCustomDictionary* spellCheckCustomDictionary(Document&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_DOCUMENT_SPELL_CHECK_CUSTOM_DICTIONARY_H_
