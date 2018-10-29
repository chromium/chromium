// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TABLE_CAPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TABLE_CAPTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGTableCaption final
    : public LayoutNGMixin<LayoutTableCaption> {
 public:
  explicit LayoutNGTableCaption(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGTableCaption"; }

 private:
  void CalculateAndSetMargins(const NGConstraintSpace&,
                              const NGPhysicalFragment&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_TABLE_CAPTION_H_
