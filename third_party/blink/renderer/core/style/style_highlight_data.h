// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_

#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ComputedStyle;
using CustomHighlightsStyleMap =
    HashMap<AtomicString, scoped_refptr<const ComputedStyle>>;

class CORE_EXPORT StyleHighlightData final
    : public RefCounted<StyleHighlightData> {
 public:
  StyleHighlightData(StyleHighlightData&& other) = delete;
  StyleHighlightData& operator=(const StyleHighlightData& other) = delete;
  StyleHighlightData& operator=(StyleHighlightData&& other) = delete;

  static scoped_refptr<StyleHighlightData> Create();
  scoped_refptr<StyleHighlightData> Copy() const;

  bool operator==(const StyleHighlightData&) const;

  const ComputedStyle* Style(
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom) const;
  const ComputedStyle* Selection() const;
  const ComputedStyle* TargetText() const;
  const ComputedStyle* SpellingError() const;
  const ComputedStyle* GrammarError() const;
  const ComputedStyle* CustomHighlight(const AtomicString&) const;
  const CustomHighlightsStyleMap& CustomHighlights() const {
    return custom_highlights_;
  }
  void SetSelection(scoped_refptr<const ComputedStyle>&&);
  void SetTargetText(scoped_refptr<const ComputedStyle>&&);
  void SetSpellingError(scoped_refptr<const ComputedStyle>&&);
  void SetGrammarError(scoped_refptr<const ComputedStyle>&&);
  void SetCustomHighlight(const AtomicString&,
                          scoped_refptr<const ComputedStyle>&&);

 private:
  StyleHighlightData();
  StyleHighlightData(const StyleHighlightData& other);

  scoped_refptr<const ComputedStyle> selection_;
  scoped_refptr<const ComputedStyle> target_text_;
  scoped_refptr<const ComputedStyle> spelling_error_;
  scoped_refptr<const ComputedStyle> grammar_error_;
  CustomHighlightsStyleMap custom_highlights_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_
