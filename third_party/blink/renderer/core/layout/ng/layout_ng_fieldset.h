// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

class CORE_EXPORT LayoutNGFieldset final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGFieldset(Element*);

  const char* GetName() const override { return "LayoutNGFieldset"; }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  bool CreatesNewFormattingContext() const final { return true; }

  void Paint(const PaintInfo&) const final;

 protected:
  bool IsOfType(LayoutObjectType) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGFieldset, IsLayoutNGFieldset());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_FIELDSET_H_
