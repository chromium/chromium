// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;

class MODULES_EXPORT SpellCheckCustomDictionary final
    : public ScriptWrappable,
      public GarbageCollectedMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SpellCheckCustomDictionary(Document& document);
  ~SpellCheckCustomDictionary() override = default;

  void addWords(const Vector<String>& words);
  void removeWords(const Vector<String>& words);

  void Trace(Visitor*) const override;

 private:
  // The document this dictionary belongs to. Calls only change this
  // document's words, whichever realm the calling script runs in, and do
  // nothing while the document is not the active document of a frame.
  WeakMember<Document> document_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPELL_CHECK_CUSTOM_DICTIONARY_SPELL_CHECK_CUSTOM_DICTIONARY_H_
