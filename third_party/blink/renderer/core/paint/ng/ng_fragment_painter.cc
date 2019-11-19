// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_fragment_painter.h"

#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void NGFragmentPainter::PaintOutline(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  if (!NGOutlineUtils::HasPaintedOutline(fragment.Style(), fragment.GetNode()))
    return;

  Vector<PhysicalRect> outline_rects;
  fragment.AddSelfOutlineRects(
      paint_offset,
      fragment.GetLayoutObject()
          ->OutlineRectsShouldIncludeBlockVisualOverflow(),
      &outline_rects);

  if (outline_rects.IsEmpty())
    return;

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client, paint_info.phase))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           paint_info.phase);
  PaintOutlineRects(paint_info, outline_rects, fragment.Style());
}

void NGFragmentPainter::AddURLRectIfNeeded(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.ShouldAddUrlMetadata());

  // TODO(layout-dev): Should use break token when NG has its own tree building.
  const NGPhysicalBoxFragment& fragment = PhysicalFragment();
  if (fragment.GetLayoutObject()->IsElementContinuation() ||
      fragment.Style().Visibility() != EVisibility::kVisible)
    return;

  Node* node = fragment.GetNode();
  if (!node || !node->IsLink())
    return;

  KURL url = To<Element>(node)->HrefURL();
  if (!url.IsValid())
    return;

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  IntRect rect = display_item_client.VisualRect();
  if (rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kPrintedContentPDFURLRect))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kPrintedContentPDFURLRect);

  Document& document = fragment.GetLayoutObject()->GetDocument();
  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url, document.BaseURL())) {
    String fragment_name = url.FragmentIdentifier();
    if (document.FindAnchor(fragment_name))
      paint_info.context.SetURLFragmentForRect(fragment_name, rect);
    return;
  }
  paint_info.context.SetURLForRect(url, rect);
}

bool NGFragmentPainter::ShouldRecordHitTestData(
    const PaintInfo& paint_info,
    const NGPhysicalBoxFragment& fragment) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return false;

  // If an object is not visible, it does not participate in hit testing.
  if (fragment.Style().Visibility() != EVisibility::kVisible)
    return false;

  auto touch_action = fragment.EffectiveAllowedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return false;

  return true;
}

}  // namespace blink
