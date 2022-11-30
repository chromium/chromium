// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;

// A LayoutObject subclass for outside-positioned list markers in LayoutNG.
class CORE_EXPORT LayoutNGOutsideListMarker final
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGOutsideListMarker(Element*);

  void WillCollectInlines() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGOutsideListMarker";
  }

  bool NeedsOccupyWholeLine() const;

  const ListMarker& Marker() const {
    NOT_DESTROYED();
    return list_marker_;
  }
  ListMarker& Marker() {
    NOT_DESTROYED();
    return list_marker_;
  }

  PaginationBreakability GetPaginationBreakability(
      FragmentationEngine engine) const final;

 private:
  bool IsOfType(LayoutObjectType) const override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  ListMarker list_marker_;
};

template <>
struct DowncastTraits<LayoutNGOutsideListMarker> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGOutsideListMarker();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_
