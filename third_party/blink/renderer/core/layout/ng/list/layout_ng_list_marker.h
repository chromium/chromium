// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class Document;
class LayoutNGListItem;

// A LayoutObject subclass for list markers in LayoutNG.
class CORE_EXPORT LayoutNGListMarker final
    : public LayoutNGMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGListMarker(Element*);
  static LayoutNGListMarker* CreateAnonymous(Document*);

  // True if the LayoutObject is a list marker wrapper for block content.
  //
  // Because a list marker in LayoutNG is an inline block, and because CSS
  // defines all children of a box must be either inline level or block level,
  // when the content of an list item is block level, the list marker is wrapped
  // in an anonymous block box. This function determines such an anonymous box.
  static bool IsListMarkerWrapperForBlockContent(const LayoutObject&);

  void WillCollectInlines() override;

  bool IsContentImage() const;

  LayoutObject* SymbolMarkerLayoutText() const;

  // Marker text with suffix, e.g. "1. ", for use in accessibility.
  String TextAlternative() const;

  const char* GetName() const override { return "LayoutNGListMarker"; }

  LayoutNGListItem* ListItem() const;

  bool NeedsOccupyWholeLine() const;

 private:
  bool IsOfType(LayoutObjectType) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarker, IsLayoutNGListMarker());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_H_
