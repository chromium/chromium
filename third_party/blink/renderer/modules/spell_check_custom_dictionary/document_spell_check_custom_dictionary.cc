// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/document_spell_check_custom_dictionary.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class DocumentSpellCheckCustomDictionaryImpl final
    : public GarbageCollected<DocumentSpellCheckCustomDictionaryImpl>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  static DocumentSpellCheckCustomDictionaryImpl& From(Document& document) {
    DocumentSpellCheckCustomDictionaryImpl* supplement =
        Supplement<Document>::template From<
            DocumentSpellCheckCustomDictionaryImpl>(document);
    if (!supplement) {
      supplement = MakeGarbageCollected<DocumentSpellCheckCustomDictionaryImpl>(
          document);
      Supplement<Document>::ProvideTo(document, supplement);
    }
    return *supplement;
  }

  explicit DocumentSpellCheckCustomDictionaryImpl(Document& document)
      : Supplement<Document>(document) {}

  SpellCheckCustomDictionary* GetOrCreate() {
    if (!spell_check_custom_dictionary_) {
      spell_check_custom_dictionary_ =
          MakeGarbageCollected<SpellCheckCustomDictionary>();
    }
    return spell_check_custom_dictionary_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(spell_check_custom_dictionary_);
    Supplement<Document>::Trace(visitor);
  }

 private:
  Member<SpellCheckCustomDictionary> spell_check_custom_dictionary_;
};

// static
const char DocumentSpellCheckCustomDictionaryImpl::kSupplementName[] =
    "DocumentSpellCheckCustomDictionaryImpl";

}  // namespace

// static
SpellCheckCustomDictionary*
DocumentSpellCheckCustomDictionary::spellCheckCustomDictionary(
    Document& document) {
  return DocumentSpellCheckCustomDictionaryImpl::From(document).GetOrCreate();
}

}  // namespace blink
