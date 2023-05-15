// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class NGPhysicalFragment;

class CORE_EXPORT LayoutNGTableCaption final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGTableCaption(Element*);

  void UpdateBlockLayout() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTableCaption";
  }

  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    return true;
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectTableCaption ||
           LayoutNGBlockFlow::IsOfType(type);
  }

 private:
  // Legacy-only API.
  void CalculateAndSetMargins(const NGConstraintSpace&,
                              const NGPhysicalFragment&);
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableCaption> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCaption();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_
