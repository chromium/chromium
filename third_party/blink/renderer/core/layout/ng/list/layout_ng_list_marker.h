// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

class Document;

// A LayoutObject subclass for outside-positioned list markers in LayoutNG.
class CORE_EXPORT LayoutNGListMarker final
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGListMarker(Element*);
  static LayoutNGListMarker* CreateAnonymous(Document*);

  void WillCollectInlines() override;

  bool IsContentImage() const;

  LayoutObject* SymbolMarkerLayoutText() const;

  const char* GetName() const override { return "LayoutNGListMarker"; }

  bool NeedsOccupyWholeLine() const;

 private:
  bool IsOfType(LayoutObjectType) const override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarker, IsLayoutNGListMarker());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_
