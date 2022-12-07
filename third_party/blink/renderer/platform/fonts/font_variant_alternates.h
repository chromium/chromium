// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_ALTERNATES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_ALTERNATES_H_

#include "base/dcheck_is_on.h"
#include "base/functional/function_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/fonts/resolved_font_features.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class PLATFORM_EXPORT FontVariantAlternates
    : public RefCounted<FontVariantAlternates> {
 public:
  static scoped_refptr<FontVariantAlternates> Create() {
    return base::AdoptRef(new FontVariantAlternates());
  }

  void SetStylistic(AtomicString);
  void SetHistoricalForms();
  void SetSwash(AtomicString);
  void SetOrnaments(AtomicString);
  void SetAnnotation(AtomicString);

  void SetStyleset(Vector<AtomicString>);
  void SetCharacterVariant(Vector<AtomicString>);

  AtomicString* Stylistic() { return stylistic_ ? &(*stylistic_) : nullptr; }
  bool HistoricalForms() { return historical_forms_; }
  AtomicString* Swash() { return swash_ ? &(*swash_) : nullptr; }
  AtomicString* Ornaments() { return ornaments_ ? &(*ornaments_) : nullptr; }
  AtomicString* Annotation() { return annotation_ ? &(*annotation_) : nullptr; }

  const Vector<AtomicString>& Styleset() { return styleset_; }
  const Vector<AtomicString>& CharacterVariant() { return character_variant_; }

  using ResolverFunction = base::FunctionRef<Vector<uint32_t>(AtomicString)>;
  /* Resolves the internal feature configuration with aliases against resolution
   * functions to get the actual OpenType feature indices for each alias.
   * Produces a resolved copy on which it is possible to call
   * GetResolvedFontFeatures(). Can be called with empty resolution functions
   * for converting just the historical-forms flag to a resolved OpenType
   * feature. */
  scoped_refptr<FontVariantAlternates> Resolve(
      ResolverFunction resolve_stylistic,
      ResolverFunction resolve_styleset_,
      ResolverFunction resolve_character_variant,
      ResolverFunction resolve_swash,
      ResolverFunction resolve_ornaments,
      ResolverFunction resolve_annotation) const;

  const ResolvedFontFeatures& GetResolvedFontFeatures() const;

  unsigned GetHash() const;

  bool IsNormal();

  bool operator==(const FontVariantAlternates& other) const;
  bool operator!=(const FontVariantAlternates& other) const {
    return !(*this == other);
  }

 private:
  FontVariantAlternates();

  // RefCounted<> has a deleted copy constructor. Since we're inheriting
  // from it, we can't re-enable it and have to manually implement a
  // Clone() method for the case of making a changed copy in
  // CSSFontSelector.
  static scoped_refptr<FontVariantAlternates> Clone(
      const FontVariantAlternates& other);

  absl::optional<AtomicString> stylistic_ = absl::nullopt;
  absl::optional<AtomicString> swash_ = absl::nullopt;
  absl::optional<AtomicString> ornaments_ = absl::nullopt;
  absl::optional<AtomicString> annotation_ = absl::nullopt;

  Vector<AtomicString> styleset_;
  Vector<AtomicString> character_variant_;
  bool historical_forms_ = false;

  ResolvedFontFeatures resolved_features_;

#if DCHECK_IS_ON()
  bool is_resolved_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_ALTERNATES_H_
