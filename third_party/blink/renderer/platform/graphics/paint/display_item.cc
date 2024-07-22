// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"

#include <cinttypes>

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsDisplayItem {
  uintptr_t pointer_as_id;
  gfx::Rect rect;
  uint32_t i1;
  uint32_t i2;
};
ASSERT_SIZE(DisplayItem, SameSizeAsDisplayItem);

void DisplayItem::Destruct() {
  if (IsTombstone())
    return;
  if (auto* drawing = DynamicTo<DrawingDisplayItem>(this)) {
    drawing->~DrawingDisplayItem();
  } else if (auto* foreign_layer = DynamicTo<ForeignLayerDisplayItem>(this)) {
    foreign_layer->~ForeignLayerDisplayItem();
  } else {
    To<ScrollbarDisplayItem>(this)->~ScrollbarDisplayItem();
  }
}

bool DisplayItem::EqualsForUnderInvalidation(const DisplayItem& other) const {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  SECURITY_CHECK(!IsTombstone());
  if (client_id_ != other.client_id_ || type_ != other.type_ ||
      fragment_ != other.fragment_ ||
      raster_effect_outset_ != other.raster_effect_outset_ ||
      draws_content_ != other.draws_content_)
    return false;

  if (visual_rect_ != other.visual_rect_ &&
      // Change of empty visual rect doesn't matter.
      (visual_rect_.IsEmpty() && other.visual_rect_.IsEmpty()) &&
      // Visual rect of a DrawingDisplayItem not drawing content doesn't matter.
      (!IsDrawing() || draws_content_))
    return false;

  if (auto* drawing = DynamicTo<DrawingDisplayItem>(this)) {
    return drawing->EqualsForUnderInvalidationImpl(
        To<DrawingDisplayItem>(other));
  }
  if (auto* foreign_layer = DynamicTo<ForeignLayerDisplayItem>(this)) {
    return foreign_layer->EqualsForUnderInvalidationImpl(
        To<ForeignLayerDisplayItem>(other));
  }
  return To<ScrollbarDisplayItem>(this)->EqualsForUnderInvalidationImpl(
      To<ScrollbarDisplayItem>(other));
}

#if DCHECK_IS_ON()

static WTF::String PaintPhaseAsDebugString(int paint_phase) {
  // Must be kept in sync with PaintPhase.
  switch (paint_phase) {
    case 0:
      return "PaintPhaseBlockBackground";
    case 1:
      return "PaintPhaseSelfBlockBackgroundOnly";
    case 2:
      return "PaintPhaseDescendantBlockBackgroundsOnly";
    case 3:
      return "PaintPhaseForcedColorsModeBackplate";
    case 4:
      return "PaintPhaseFloat";
    case 5:
      return "PaintPhaseForeground";
    case 6:
      return "PaintPhaseOutline";
    case 7:
      return "PaintPhaseSelfOutlineOnly";
    case 8:
      return "PaintPhaseDescendantOutlinesOnly";
    case 9:
      return "PaintPhaseOverlayOverflowControls";
    case 10:
      return "PaintPhaseSelection";
    case 11:
      return "PaintPhaseTextClip";
    case DisplayItem::kPaintPhaseMax:
      return "PaintPhaseMask";
    default:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";
  }
}

#define PAINT_PHASE_BASED_DEBUG_STRINGS(Category)          \
  if (type >= DisplayItem::k##Category##PaintPhaseFirst && \
      type <= DisplayItem::k##Category##PaintPhaseLast)    \
    return #Category + PaintPhaseAsDebugString(            \
                           type - DisplayItem::k##Category##PaintPhaseFirst);

#define DEBUG_STRING_CASE(DisplayItemName) \
  case DisplayItem::k##DisplayItemName:    \
    return #DisplayItemName

#define DEFAULT_CASE           \
  default:                     \
    NOTREACHED_IN_MIGRATION(); \
    return "Unknown"

static WTF::String SpecialDrawingTypeAsDebugString(DisplayItem::Type type) {
  switch (type) {
    DEBUG_STRING_CASE(BoxDecorationBackground);
    DEBUG_STRING_CASE(FixedAttachmentBackground);
    DEBUG_STRING_CASE(Caret);
    DEBUG_STRING_CASE(CapsLockIndicator);
    DEBUG_STRING_CASE(ColumnRules);
    DEBUG_STRING_CASE(DocumentRootBackdrop);
    DEBUG_STRING_CASE(DocumentBackground);
    DEBUG_STRING_CASE(DragCaret);
    DEBUG_STRING_CASE(ForcedColorsModeBackplate);
    DEBUG_STRING_CASE(SVGImage);
    DEBUG_STRING_CASE(ImageAreaFocusRing);
    DEBUG_STRING_CASE(OverflowControls);
    DEBUG_STRING_CASE(FrameOverlay);
    DEBUG_STRING_CASE(PrintedContentDestinationLocations);
    DEBUG_STRING_CASE(PrintedContentPDFURLRect);
    DEBUG_STRING_CASE(ReflectionMask);
    DEBUG_STRING_CASE(Resizer);
    DEBUG_STRING_CASE(SVGClip);
    DEBUG_STRING_CASE(SVGMask);
    DEBUG_STRING_CASE(ScrollbarThumb);
    DEBUG_STRING_CASE(ScrollbarTickmarks);
    DEBUG_STRING_CASE(ScrollbarTrackAndButtons);
    DEBUG_STRING_CASE(ScrollCorner);
    DEBUG_STRING_CASE(SelectionTint);
    DEBUG_STRING_CASE(TableCollapsedBorders);
    DEBUG_STRING_CASE(WebPlugin);

    DEFAULT_CASE;
  }
}

static WTF::String DrawingTypeAsDebugString(DisplayItem::Type type) {
  PAINT_PHASE_BASED_DEBUG_STRINGS(Drawing);
  return "Drawing" + SpecialDrawingTypeAsDebugString(type);
}

static String ForeignLayerTypeAsDebugString(DisplayItem::Type type) {
  switch (type) {
    DEBUG_STRING_CASE(ForeignLayerCanvas);
    DEBUG_STRING_CASE(ForeignLayerDevToolsOverlay);
    DEBUG_STRING_CASE(ForeignLayerPlugin);
    DEBUG_STRING_CASE(ForeignLayerVideo);
    DEBUG_STRING_CASE(ForeignLayerRemoteFrame);
    DEBUG_STRING_CASE(ForeignLayerLinkHighlight);
    DEBUG_STRING_CASE(ForeignLayerViewportScroll);
    DEBUG_STRING_CASE(ForeignLayerViewportScrollbar);
    DEBUG_STRING_CASE(ForeignLayerViewTransitionContent);
    DEFAULT_CASE;
  }
}

WTF::String DisplayItem::TypeAsDebugString(Type type) {
  if (IsDrawingType(type))
    return DrawingTypeAsDebugString(type);

  if (IsForeignLayerType(type))
    return ForeignLayerTypeAsDebugString(type);

  PAINT_PHASE_BASED_DEBUG_STRINGS(Clip);
  PAINT_PHASE_BASED_DEBUG_STRINGS(Scroll);
  PAINT_PHASE_BASED_DEBUG_STRINGS(SVGTransform);
  PAINT_PHASE_BASED_DEBUG_STRINGS(SVGEffect);

  switch (type) {
    DEBUG_STRING_CASE(HitTest);
    DEBUG_STRING_CASE(WebPluginHitTest);
    DEBUG_STRING_CASE(RegionCapture);
    DEBUG_STRING_CASE(ScrollHitTest);
    DEBUG_STRING_CASE(ResizerScrollHitTest);
    DEBUG_STRING_CASE(ScrollbarHitTest);
    DEBUG_STRING_CASE(LayerChunk);
    DEBUG_STRING_CASE(LayerChunkForeground);
    DEBUG_STRING_CASE(ScrollbarHorizontal);
    DEBUG_STRING_CASE(ScrollbarVertical);
    DEBUG_STRING_CASE(UninitializedType);
    DEFAULT_CASE;
  }
}

String DisplayItem::AsDebugString(const PaintArtifact& paint_artifact) const {
  auto json = std::make_unique<JSONObject>();
  PropertiesAsJSON(*json, paint_artifact);
  return json->ToPrettyJSONString();
}

String DisplayItem::IdAsString(const PaintArtifact& paint_artifact) const {
  if (IsSubsequenceTombstone())
    return "SUBSEQUENCE TOMBSTONE";
  if (IsTombstone())
    return "TOMBSTONE " + paint_artifact.IdAsString(GetId());
  return paint_artifact.IdAsString(GetId());
}

void DisplayItem::PropertiesAsJSON(JSONObject& json,
                                   const PaintArtifact& paint_artifact) const {
  json.SetString("id", IdAsString(paint_artifact));
  if (IsSubsequenceTombstone()) {
    return;
  }
  json.SetString("invalidation",
                 PaintInvalidationReasonToString(GetPaintInvalidationReason()));
  json.SetString("visualRect", String(VisualRect().ToString()));
  if (GetRasterEffectOutset() != RasterEffectOutset::kNone) {
    json.SetDouble(
        "outset",
        GetRasterEffectOutset() == RasterEffectOutset::kHalfPixel ? 0.5 : 1);
  }

  if (IsTombstone())
    return;
  if (auto* drawing = DynamicTo<DrawingDisplayItem>(this)) {
    drawing->PropertiesAsJSONImpl(json);
  } else if (auto* foreign_layer = DynamicTo<ForeignLayerDisplayItem>(this)) {
    foreign_layer->PropertiesAsJSONImpl(json);
  } else {
    To<ScrollbarDisplayItem>(this)->PropertiesAsJSONImpl(json);
  }
}

#endif  // DCHECK_IS_ON()

String DisplayItem::Id::ToString() const {
#if DCHECK_IS_ON()
  return String::Format("%p:%s:%d", reinterpret_cast<void*>(client_id),
                        DisplayItem::TypeAsDebugString(type).Utf8().data(),
                        fragment);
#else
  return String::Format("%p:%d:%d", reinterpret_cast<void*>(client_id),
                        static_cast<int>(type), fragment);
#endif
}

String DisplayItem::Id::ToString(const PaintArtifact& paint_artifact) const {
  return paint_artifact.IdAsString(*this);
}

std::ostream& operator<<(std::ostream& os, DisplayItem::Type type) {
#if DCHECK_IS_ON()
  return os << DisplayItem::TypeAsDebugString(type).Utf8();
#else
  return os << static_cast<int>(type);
#endif
}

std::ostream& operator<<(std::ostream& os, const DisplayItem::Id& id) {
  return os << id.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& os, const DisplayItem& item) {
  return os << "{\"id\": " << item.GetId() << "}";
}

}  // namespace blink
