// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class MODULES_EXPORT SpellCheckCustomDictionary final
    : public ScriptWrappable,
      public GarbageCollectedMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SpellCheckCustomDictionary() = default;
  ~SpellCheckCustomDictionary() override = default;

  void addWords(ScriptState* script_state, const Vector<String>& words);
  void removeWords(ScriptState* script_state, const Vector<String>& words);

  void Trace(Visitor*) const override;

 private:
  // Whether addWords() has already warned about words it ignored. This object
  // is owned by a single Document, so the warning is logged once per document.
  bool ignored_words_warned_ = false;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_
