/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTING_INFO_H_

#include "base/logging.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#endif

namespace blink {

class PaintLayer;

enum PaintLayerFlag {
  kPaintLayerNoFlag = 0,
  kPaintLayerHaveTransparency = 1,
  kPaintLayerPaintingOverlayOverflowControls = 1 << 3,
  kPaintLayerPaintingCompositingBackgroundPhase = 1 << 4,
  kPaintLayerPaintingCompositingForegroundPhase = 1 << 5,
  kPaintLayerPaintingCompositingMaskPhase = 1 << 6,
  kPaintLayerPaintingCompositingScrollingPhase = 1 << 7,
  kPaintLayerPaintingOverflowContents = 1 << 8,
  kPaintLayerPaintingSkipRootBackground = 1 << 10,
  kPaintLayerPaintingRenderingClipPathAsMask = 1 << 13,
  kPaintLayerPaintingCompositingDecorationPhase = 1 << 14,
  kPaintLayerPaintingRenderingResourceSubtree = 1 << 15,
  kPaintLayerPaintingCompositingAllPhases =
      (kPaintLayerPaintingCompositingBackgroundPhase |
       kPaintLayerPaintingCompositingForegroundPhase |
       kPaintLayerPaintingCompositingMaskPhase |
       kPaintLayerPaintingCompositingDecorationPhase)
};

typedef unsigned PaintLayerFlags;

struct PaintLayerPaintingInfo {
  STACK_ALLOCATED();

 public:
  PaintLayerPaintingInfo(PaintLayer* root_layer,
                         const CullRect& cull_rect,
                         GlobalPaintFlags global_paint_flags,
                         const PhysicalOffset& sub_pixel_accumulation)
      : root_layer(root_layer),
        cull_rect(cull_rect),
        sub_pixel_accumulation(sub_pixel_accumulation),
        global_paint_flags_(global_paint_flags) {}

  GlobalPaintFlags GetGlobalPaintFlags() const { return global_paint_flags_; }

  // TODO(jchaffraix): We should encapsulate all these fields.
  const PaintLayer* root_layer;
  CullRect cull_rect;  // relative to rootLayer;
  PhysicalOffset sub_pixel_accumulation;

 private:
  const GlobalPaintFlags global_paint_flags_;
};

#if DCHECK_IS_ON()
inline String PaintLayerFlagsToDebugString(PaintLayerFlags flags) {
  if (flags == 0)
    return "(kPaintLayerNoFlag)";

  StringBuilder builder;
  builder.Append("(");
  bool need_separator = false;
  auto append = [&builder, &need_separator](const char* str) {
    if (need_separator)
      builder.Append("|");
    builder.Append(str);
    need_separator = true;
  };

  if (flags & kPaintLayerPaintingCompositingAllPhases) {
    append("kPaintLayerPaintingCompositingAllPhases");
  } else {
    if (flags & kPaintLayerPaintingCompositingBackgroundPhase)
      append("kPaintLayerPaintingCompositingBackgroundPhase");
    if (flags & kPaintLayerPaintingCompositingForegroundPhase)
      append("kPaintLayerPaintingCompositingForegroundPhase");
    if (flags & kPaintLayerPaintingCompositingMaskPhase)
      append("kPaintLayerPaintingCompositingMaskPhase");
    if (flags & kPaintLayerPaintingCompositingDecorationPhase)
      append("kPaintLayerPaintingCompositingDecorationPhase");
  }

  if (flags & kPaintLayerHaveTransparency)
    append("kPaintLayerHaveTransparency");
  if (flags & kPaintLayerPaintingOverlayOverflowControls)
    append("kPaintLayerPaintingOverlayOverflowControls");
  if (flags & kPaintLayerPaintingCompositingScrollingPhase)
    append("kPaintLayerPaintingCompositingScrollingPhase");
  if (flags & kPaintLayerPaintingOverflowContents)
    append("kPaintLayerPaintingOverflowContents");
  if (flags & kPaintLayerPaintingSkipRootBackground)
    append("kPaintLayerPaintingSkipRootBackground");
  if (flags & kPaintLayerPaintingRenderingClipPathAsMask)
    append("kPaintLayerPaintingRenderingClipPathAsMask");
  if (flags & kPaintLayerPaintingRenderingResourceSubtree)
    append("kPaintLayerPaintingRenderingResourceSubtree");

  builder.Append(")");
  return builder.ToString();
}
#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_PAINTING_INFO_H_
