// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"

namespace blink {

// Devtools uses this info to highlight lines and items on its flexbox overlay.
// Devtools usually reads such info from the layout or fragment trees. But
// Layout doesn't store this flex line -> flex items hierarchy there, or
// anywhere, because neither paint nor ancestor layout needs it. So the NG flex
// layout algorithm will fill one of these in when devtools requests it.
struct DevtoolsFlexInfo {
  struct Item {
    PhysicalRect rect;
    LayoutUnit baseline;
  };
  struct Line {
    Vector<Item> items;
  };
  Vector<Line> lines;
};

class CORE_EXPORT LayoutNGFlexibleBox : public LayoutNGBlock {
 public:
  explicit LayoutNGFlexibleBox(Element*);

  bool HasTopOverflow() const override;
  bool HasLeftOverflow() const override;

  void UpdateBlockLayout(bool relayout_children) override;

  bool IsFlexibleBoxIncludingDeprecatedAndNG() const final {
    NOT_DESTROYED();
    return true;
  }
  bool IsFlexibleBoxIncludingNG() const final {
    NOT_DESTROYED();
    return true;
  }
  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGFlexibleBox";
  }

  DevtoolsFlexInfo LayoutForDevtools();

 protected:
  bool IsChildAllowed(LayoutObject* object,
                      const ComputedStyle& style) const override;
  void RemoveChild(LayoutObject*) override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectNGFlexibleBox ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

template <>
struct DowncastTraits<LayoutNGFlexibleBox> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsFlexibleBoxIncludingNG() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_
