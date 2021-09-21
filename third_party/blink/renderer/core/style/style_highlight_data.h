// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_

#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ComputedStyle;

class CORE_EXPORT StyleHighlightData final
    : public RefCounted<StyleHighlightData> {
 public:
  using PassKey = base::PassKey<StyleHighlightData>;
  using PkComputedStyle = base::PassKey<ComputedStyle>;

  StyleHighlightData(StyleHighlightData&& other) = delete;
  StyleHighlightData& operator=(const StyleHighlightData& other) = delete;
  StyleHighlightData& operator=(StyleHighlightData&& other) = delete;

  explicit StyleHighlightData(PkComputedStyle);
  StyleHighlightData(PkComputedStyle, const StyleHighlightData& other);
  static scoped_refptr<StyleHighlightData> Create(PkComputedStyle);
  scoped_refptr<StyleHighlightData> Copy(PkComputedStyle) const;

  bool operator==(const StyleHighlightData&) const;

  // TODO(crbug.com/1024156): add methods for ::highlight()
  const scoped_refptr<ComputedStyle>& Selection() const;
  const scoped_refptr<ComputedStyle>& TargetText() const;
  const scoped_refptr<ComputedStyle>& SpellingError() const;
  const scoped_refptr<ComputedStyle>& GrammarError() const;
  void SetSelection(scoped_refptr<ComputedStyle>&&);
  void SetTargetText(scoped_refptr<ComputedStyle>&&);
  void SetSpellingError(scoped_refptr<ComputedStyle>&&);
  void SetGrammarError(scoped_refptr<ComputedStyle>&&);

 private:
  StyleHighlightData();
  StyleHighlightData(const StyleHighlightData& other);

  // TODO(crbug.com/1024156): add field for ::highlight()
  scoped_refptr<ComputedStyle> selection_;
  scoped_refptr<ComputedStyle> target_text_;
  scoped_refptr<ComputedStyle> spelling_error_;
  scoped_refptr<ComputedStyle> grammar_error_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_HIGHLIGHT_DATA_H_
