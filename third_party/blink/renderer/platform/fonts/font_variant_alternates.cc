// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_variant_alternates.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

#include <hb.h>

namespace blink {

FontVariantAlternates::FontVariantAlternates() = default;

namespace {
constexpr uint32_t kSwshTag = HB_TAG('s', 'w', 's', 'h');
constexpr uint32_t kCswhTag = HB_TAG('c', 's', 'w', 'h');
constexpr uint32_t kHistTag = HB_TAG('h', 'i', 's', 't');
constexpr uint32_t kSaltTag = HB_TAG('s', 'a', 'l', 't');
constexpr uint32_t kNaltTag = HB_TAG('n', 'a', 'l', 't');
constexpr uint32_t kOrnmTag = HB_TAG('o', 'r', 'n', 'm');

constexpr uint32_t kMaxTag = 99;

uint32_t NumberedTag(uint32_t base_tag, uint32_t number) {
  if (number > kMaxTag)
    return base_tag;
  base_tag |= (number / 10 + 48) << 8;
  base_tag |= (number % 10 + 48);
  return base_tag;
}

uint32_t ssTag(uint32_t number) {
  uint32_t base_tag = HB_TAG('s', 's', 0, 0);
  return NumberedTag(base_tag, number);
}

uint32_t cvTag(uint32_t number) {
  uint32_t base_tag = HB_TAG('c', 'v', 0, 0);
  return NumberedTag(base_tag, number);
}
}  // namespace

const ResolvedFontFeatures& FontVariantAlternates::GetResolvedFontFeatures()
    const {
#if DCHECK_IS_ON()
  DCHECK(is_resolved_);
#endif
  return resolved_features_;
}

scoped_refptr<FontVariantAlternates> FontVariantAlternates::Clone(
    const FontVariantAlternates& other) {
  auto new_object = base::AdoptRef(new FontVariantAlternates());
  new_object->stylistic_ = other.stylistic_;
  new_object->historical_forms_ = other.historical_forms_;
  new_object->styleset_ = other.styleset_;
  new_object->character_variant_ = other.character_variant_;
  new_object->swash_ = other.swash_;
  new_object->ornaments_ = other.ornaments_;
  new_object->annotation_ = other.annotation_;
  new_object->resolved_features_ = other.resolved_features_;
  return new_object;
}

bool FontVariantAlternates::IsNormal() const {
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

scoped_refptr<FontVariantAlternates> FontVariantAlternates::Resolve(
    ResolverFunction resolve_stylistic,
    ResolverFunction resolve_styleset,
    ResolverFunction resolve_character_variant,
    ResolverFunction resolve_swash,
    ResolverFunction resolve_ornaments,
    ResolverFunction resolve_annotation) const {
  scoped_refptr<FontVariantAlternates> clone = Clone(*this);
  // https://drafts.csswg.org/css-fonts-4/#multi-value-features

  // "Most font specific functional values of the
  // font-variant-alternates property take a single value
  // (e.g. swash()). The character-variant() property value allows two
  // values and styleset() allows an unlimited number.
  // For the styleset property value, multiple values indicate the style
  // sets to be enabled. Values between 1 and 99 enable OpenType
  // features ss01 through ss99. [...]"

  if (swash_) {
    Vector<uint32_t> swash_resolved = resolve_swash(*swash_);
    if (!swash_resolved.empty()) {
      CHECK_EQ(swash_resolved.size(), 1u);
      auto pair = std::make_pair(kSwshTag, swash_resolved[0]);
      clone->resolved_features_.push_back(pair);
      pair = std::make_pair(kCswhTag, swash_resolved[0]);
      clone->resolved_features_.push_back(pair);
    }
  }

  if (ornaments_) {
    Vector<uint32_t> ornaments_resolved = resolve_ornaments(*ornaments_);
    if (!ornaments_resolved.empty()) {
      CHECK_EQ(ornaments_resolved.size(), 1u);
      auto pair = std::make_pair(kOrnmTag, ornaments_resolved[0]);
      clone->resolved_features_.push_back(pair);
    }
  }

  if (annotation_) {
    Vector<uint32_t> annotation_resolved = resolve_annotation(*annotation_);
    if (!annotation_resolved.empty()) {
      CHECK_EQ(annotation_resolved.size(), 1u);
      auto pair = std::make_pair(kNaltTag, annotation_resolved[0]);
      clone->resolved_features_.push_back(pair);
    }
  }

  if (stylistic_) {
    Vector<uint32_t> stylistic_resolved = resolve_stylistic(*stylistic_);
    if (!stylistic_resolved.empty()) {
      CHECK_EQ(stylistic_resolved.size(), 1u);
      auto pair = std::make_pair(kSaltTag, stylistic_resolved[0]);
      clone->resolved_features_.push_back(pair);
    }
  }

  if (!styleset_.empty()) {
    for (const AtomicString& styleset_alias : styleset_) {
      Vector<uint32_t> styleset_resolved = resolve_styleset(styleset_alias);
      if (!styleset_resolved.empty()) {
        for (auto styleset_entry : styleset_resolved) {
          if (styleset_entry <= kMaxTag) {
            auto pair = std::make_pair(ssTag(styleset_entry), 1u);
            clone->resolved_features_.push_back(pair);
          }
        }
      }
    }
  }

  if (!character_variant_.empty()) {
    for (const AtomicString& character_variant_alias : character_variant_) {
      Vector<uint32_t> character_variant_resolved =
          resolve_character_variant(character_variant_alias);
      if (!character_variant_resolved.empty() &&
          character_variant_resolved.size() <= 2) {
        uint32_t feature_value = 1;
        if (character_variant_resolved.size() == 2) {
          feature_value = character_variant_resolved[1];
        }
        if (character_variant_resolved[0] <= kMaxTag) {
          auto pair = std::make_pair(cvTag(character_variant_resolved[0]),
                                     feature_value);
          clone->resolved_features_.push_back(pair);
        }
      }
    }
  }

  if (historical_forms_) {
    auto pair = std::make_pair(kHistTag, 1u);
    clone->resolved_features_.push_back(pair);
  }

#if DCHECK_IS_ON()
  clone->is_resolved_ = true;
#endif

  return clone;
}

unsigned FontVariantAlternates::GetHash() const {
  unsigned computed_hash = 0;
  WTF::AddIntToHash(computed_hash,
                    stylistic_.has_value() ? WTF::GetHash(*stylistic_) : -1);
  WTF::AddIntToHash(computed_hash, historical_forms_);
  WTF::AddIntToHash(computed_hash,
                    swash_.has_value() ? WTF::GetHash(*swash_) : -1);
  WTF::AddIntToHash(computed_hash,
                    ornaments_.has_value() ? WTF::GetHash(*ornaments_) : -1);
  WTF::AddIntToHash(computed_hash,
                    annotation_.has_value() ? WTF::GetHash(*annotation_) : -1);
  if (!styleset_.empty()) {
    for (const AtomicString& styleset_alias : styleset_) {
      WTF::AddIntToHash(computed_hash, WTF::GetHash(styleset_alias));
    }
  }
  if (!character_variant_.empty()) {
    for (const AtomicString& character_variant_alias : character_variant_) {
      WTF::AddIntToHash(computed_hash, WTF::GetHash(character_variant_alias));
    }
  }
  WTF::AddIntToHash(computed_hash, resolved_features_.size());
  return computed_hash;
}

bool FontVariantAlternates::operator==(
    const FontVariantAlternates& other) const {
  return stylistic_ == other.stylistic_ &&
         historical_forms_ == other.historical_forms_ &&
         styleset_ == other.styleset_ &&
         character_variant_ == other.character_variant_ &&
         swash_ == other.swash_ && ornaments_ == other.ornaments_ &&
         annotation_ == other.annotation_ &&
         resolved_features_ == other.resolved_features_;
}

}  // namespace blink
