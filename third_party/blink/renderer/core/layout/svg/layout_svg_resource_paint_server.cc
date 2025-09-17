// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"

namespace blink {

void LayoutSVGResourcePaintServer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::StyleDidChange(diff, old_style,
                                             style_change_context);
  if (diff.TransformChanged()) {
    RemoveAllClientsFromCache();
  }
}

}  // namespace blink
