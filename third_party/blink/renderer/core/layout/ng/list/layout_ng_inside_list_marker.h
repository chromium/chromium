// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INSIDE_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INSIDE_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"

namespace blink {

class Document;

// A LayoutObject subclass for inside-positioned list markers in LayoutNG.
class CORE_EXPORT LayoutNGInsideListMarker final : public LayoutInline {
 public:
  explicit LayoutNGInsideListMarker(Element*);
  static LayoutNGInsideListMarker* CreateAnonymous(Document*);

  const char* GetName() const override { return "LayoutNGInsideListMarker"; }

#if DCHECK_IS_ON()
  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override {
    // List marker should have at most one child.
    DCHECK(!FirstChild());
    LayoutInline::AddChild(new_child, before_child);
  }
#endif

 private:
  bool IsOfType(LayoutObjectType) const override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGInsideListMarker,
                                IsLayoutNGInsideListMarker());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_INSIDE_LIST_MARKER_H_
