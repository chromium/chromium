// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_variant_alternates.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

#include <hb.h>

namespace blink {

FontVariantAlternates::FontVariantAlternates() = default;

bool FontVariantAlternates::IsNormal() {
  return !stylistic_ && !historical_forms_ && !swash_ && !ornaments_ &&
         !annotation_ && styleset_.empty() && character_variant_.empty();
}

void FontVariantAlternates::SetStylistic(AtomicString stylistic) {
  stylistic_ = stylistic;
}

void FontVariantAlternates::SetHistoricalForms() {
  historical_forms_ = true;
}

void FontVariantAlternates::SetSwash(AtomicString swash) {
  swash_ = swash;
}

void FontVariantAlternates::SetOrnaments(AtomicString ornaments) {
  ornaments_ = ornaments;
}

void FontVariantAlternates::SetAnnotation(AtomicString annotation) {
  annotation_ = annotation;
}

void FontVariantAlternates::SetStyleset(Vector<AtomicString> styleset) {
  styleset_ = std::move(styleset);
}

void FontVariantAlternates::SetCharacterVariant(
    Vector<AtomicString> character_variant) {
  character_variant_ = std::move(character_variant);
}

unsigned FontVariantAlternates::GetHash() const {
  unsigned computed_hash = 0;
  WTF::AddIntToHash(computed_hash, stylistic_.has_value()
                                       ? AtomicStringHash::GetHash(*stylistic_)
                                       : -1);
  WTF::AddIntToHash(computed_hash, historical_forms_);
  WTF::AddIntToHash(computed_hash, swash_.has_value()
                                       ? AtomicStringHash::GetHash(*swash_)
                                       : -1);
  WTF::AddIntToHash(computed_hash, ornaments_.has_value()
                                       ? AtomicStringHash::GetHash(*ornaments_)
                                       : -1);
  WTF::AddIntToHash(computed_hash, annotation_.has_value()
                                       ? AtomicStringHash::GetHash(*annotation_)
                                       : -1);
  if (!styleset_.empty()) {
    for (const AtomicString& styleset_alias : styleset_) {
      WTF::AddIntToHash(computed_hash,
                        AtomicStringHash::GetHash(styleset_alias));
    }
  }
  if (!character_variant_.empty()) {
    for (const AtomicString& character_variant_alias : character_variant_) {
      WTF::AddIntToHash(computed_hash,
                        AtomicStringHash::GetHash(character_variant_alias));
    }
  }
  return computed_hash;
}

bool FontVariantAlternates::operator==(
    const FontVariantAlternates& other) const {
  return stylistic_ == other.stylistic_ &&
         historical_forms_ == other.historical_forms_ &&
         styleset_ == other.styleset_ &&
         character_variant_ == other.character_variant_ &&
         swash_ == other.swash_ && ornaments_ == other.ornaments_ &&
         annotation_ == other.annotation_;
}

}  // namespace blink
