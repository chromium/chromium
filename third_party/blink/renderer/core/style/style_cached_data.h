// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CACHED_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CACHED_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;

// Key for the pseudo-style cache (which lives on each ComputedStyle).
// Note that pseudo_type is also duplicated in the pseudo element's
// ComputedStyle (the StyleType field); we have DCHECKs to verify
// that we don't insert an element with the type out of sync.
struct PseudoElementStyleCacheKey {
  PseudoId pseudo_type;
  AtomicString pseudo_argument;

  bool operator==(const PseudoElementStyleCacheKey& other) const {
    return pseudo_type == other.pseudo_type &&
           pseudo_argument == other.pseudo_argument;
  }
};

// NOTE: We use pseudo_argument to store DeletedValue.
template <>
struct HashTraits<PseudoElementStyleCacheKey>
    : SimpleClassHashTraits<PseudoElementStyleCacheKey> {
  static unsigned GetHash(const PseudoElementStyleCacheKey& key) {
    return HashInts(key.pseudo_type, key.pseudo_argument.IsNull()
                                         ? 0
                                         : key.pseudo_argument.Hash());
  }

  static const bool kEmptyValueIsZero = false;
  static void ConstructDeletedValue(PseudoElementStyleCacheKey& slot) {
    AtomicString pseudo_argument;
    PseudoElementStyleCacheKey* value = new (base::NotNullTag::kNotNull, &slot)
        PseudoElementStyleCacheKey{PseudoId::kPseudoIdNone, g_null_atom};
    HashTraits<AtomicString>::ConstructDeletedValue(value->pseudo_argument);
  }
  static bool IsDeletedValue(const PseudoElementStyleCacheKey& value) {
    return HashTraits<AtomicString>::IsDeletedValue(value.pseudo_argument);
  }
  static bool IsEmptyValue(const PseudoElementStyleCacheKey& value) {
    return value.pseudo_type == PseudoId::kPseudoIdNone &&
           value.pseudo_argument.IsNull();
  }
  static PseudoElementStyleCacheKey EmptyValue() {
    return PseudoElementStyleCacheKey{PseudoId::kPseudoIdNone, g_null_atom};
  }
};

using PseudoElementStyleCache =
    GCedHeapHashMap<PseudoElementStyleCacheKey, Member<const ComputedStyle>>;

// Cached (and sort-of cached) data that lives on each ComputedStyle.
class CORE_EXPORT StyleCachedData final
    : public GarbageCollected<StyleCachedData> {
 public:
  void Trace(Visitor* visitor) const {
    visitor->Trace(pseudo_element_styles_);
    visitor->Trace(applied_text_decorations_);
  }

 private:
  friend class ComputedStyle;
  friend class ComputedStyleBuilder;

  // This cache stores ComputedStyles for pseudo-elements originating from this
  // ComputedStyle's element. Pseudo-elements which are represented by
  // PseudoElement in DOM store the ComputedStyle on those elements, so this
  // cache is for:
  //
  // 1. Pseudo-elements which do not generate a PseudoElement internally like
  //    ::first-line and ::selection.
  //
  // 2. Pseudo-element style requested from getComputedStyle() where the element
  //    currently doesn't generate a PseudoElement. E.g.:
  //
  //    <style>
  //      #div::before { color: green /* no content property! */}
  //    </style>
  //    <div id=div></div>
  //    <script>
  //      getComputedStyle(div, "::before").color // still green.
  //    </script>
  Member<PseudoElementStyleCache> pseudo_element_styles_;

  // Stores the names of of all custom properties on a given ComputedStyle.
  std::unique_ptr<Vector<AtomicString>> variable_names_;

  // If this style is a "decorating box" stores the list of applied text
  // decorations (with the most recent decoration from this box being at the
  // end of the Vector).
  Member<AppliedTextDecorationVector> applied_text_decorations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CACHED_DATA_H_
