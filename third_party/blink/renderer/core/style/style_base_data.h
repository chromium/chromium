// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BASE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BASE_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ComputedStyle;

class CORE_EXPORT StyleBaseData : public RefCounted<StyleBaseData> {
  USING_FAST_MALLOC(StyleBaseData);

 public:
  static scoped_refptr<StyleBaseData> Create(
      scoped_refptr<const ComputedStyle> style,
      std::unique_ptr<CSSBitset> important_set) {
    return base::AdoptRef(new StyleBaseData(style, std::move(important_set)));
  }

  const ComputedStyle* GetBaseComputedStyle() const {
    return computed_style_.get();
  }
  const CSSBitset* GetBaseImportantSet() const { return important_set_.get(); }

 private:
  StyleBaseData(scoped_refptr<const ComputedStyle>, std::unique_ptr<CSSBitset>);

  scoped_refptr<const ComputedStyle> computed_style_;

  // Keeps track of the !important declarations used to build the base
  // computed style. These declarations must not be overwritten by animation
  // effects, hence we have to disable the base computed style optimization when
  // !important declarations conflict with active animations.
  //
  // If there were no !important declarations in the base style, this field
  // will be nullptr.
  //
  // TODO(andruud): We should be able to simply skip applying the animation
  // for properties in this set instead of disabling the optimization.
  // However, we currently need the cascade to handle the case where
  // an !important declaration appears in a :visited selector.
  // See https://crbug.com/1062217.
  std::unique_ptr<CSSBitset> important_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BASE_DATA_H_
