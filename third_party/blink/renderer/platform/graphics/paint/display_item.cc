// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"

#include "cc/paint/display_item_list.h"

namespace blink {

struct SameSizeAsDisplayItem {
  virtual ~SameSizeAsDisplayItem() = default;  // Allocate vtable pointer.
  void* pointer;
  IntRect rect;
  float outset;
  int i;
};
static_assert(sizeof(DisplayItem) == sizeof(SameSizeAsDisplayItem),
              "DisplayItem should stay small");

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
      return "PaintPhaseOverlayScrollbars";
    case 10:
      return "PaintPhaseSelection";
    case 11:
      return "PaintPhaseTextClip";
    case DisplayItem::kPaintPhaseMax:
      return "PaintPhaseMask";
    default:
      NOTREACHED();
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

#define DEFAULT_CASE \
  default:           \
    NOTREACHED();    \
    return "Unknown"

static WTF::String SpecialDrawingTypeAsDebugString(DisplayItem::Type type) {
  switch (type) {
    DEBUG_STRING_CASE(BoxDecorationBackground);
    DEBUG_STRING_CASE(Caret);
    DEBUG_STRING_CASE(CapsLockIndicator);
    DEBUG_STRING_CASE(ClippingMask);
    DEBUG_STRING_CASE(ColumnRules);
    DEBUG_STRING_CASE(DebugDrawing);
    DEBUG_STRING_CASE(DocumentBackground);
    DEBUG_STRING_CASE(DragImage);
    DEBUG_STRING_CASE(DragCaret);
    DEBUG_STRING_CASE(EmptyContentForFilters);
    DEBUG_STRING_CASE(ForcedColorsModeBackplate);
    DEBUG_STRING_CASE(SVGImage);
    DEBUG_STRING_CASE(LinkHighlight);
    DEBUG_STRING_CASE(ImageAreaFocusRing);
    DEBUG_STRING_CASE(OverflowControls);
    DEBUG_STRING_CASE(FrameOverlay);
    DEBUG_STRING_CASE(PopupContainerBorder);
    DEBUG_STRING_CASE(PopupListBoxBackground);
    DEBUG_STRING_CASE(PopupListBoxRow);
    DEBUG_STRING_CASE(PrintedContentDestinationLocations);
    DEBUG_STRING_CASE(PrintedContentPDFURLRect);
    DEBUG_STRING_CASE(ReflectionMask);
    DEBUG_STRING_CASE(Resizer);
    DEBUG_STRING_CASE(SVGClip);
    DEBUG_STRING_CASE(SVGFilter);
    DEBUG_STRING_CASE(SVGMask);
    DEBUG_STRING_CASE(ScrollbarThumb);
    DEBUG_STRING_CASE(ScrollbarTickmarks);
    DEBUG_STRING_CASE(ScrollbarTrackAndButtons);
    DEBUG_STRING_CASE(ScrollCorner);
    DEBUG_STRING_CASE(SelectionTint);
    DEBUG_STRING_CASE(TableCollapsedBorders);
    DEBUG_STRING_CASE(VideoBitmap);
    DEBUG_STRING_CASE(WebFont);
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
    DEBUG_STRING_CASE(ForeignLayerWrapper);
    DEBUG_STRING_CASE(ForeignLayerContentsWrapper);
    DEBUG_STRING_CASE(ForeignLayerLinkHighlight);
    DEBUG_STRING_CASE(ForeignLayerViewportScroll);
    DEBUG_STRING_CASE(ForeignLayerViewportScrollbar);
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
    DEBUG_STRING_CASE(ScrollHitTest);
    DEBUG_STRING_CASE(ResizerScrollHitTest);
    DEBUG_STRING_CASE(PluginScrollHitTest);
    DEBUG_STRING_CASE(LayerChunkBackground);
    DEBUG_STRING_CASE(LayerChunkNegativeZOrderChildren);
    DEBUG_STRING_CASE(LayerChunkDescendantBackgrounds);
    DEBUG_STRING_CASE(LayerChunkFloat);
    DEBUG_STRING_CASE(LayerChunkForeground);
    DEBUG_STRING_CASE(LayerChunkNormalFlowAndPositiveZOrderChildren);
    DEBUG_STRING_CASE(ScrollbarHorizontal);
    DEBUG_STRING_CASE(ScrollbarVertical);
    DEBUG_STRING_CASE(UninitializedType);
    DEFAULT_CASE;
  }
}

WTF::String DisplayItem::AsDebugString() const {
  auto json = std::make_unique<JSONObject>();
  PropertiesAsJSON(*json);
  return json->ToPrettyJSONString();
}

void DisplayItem::PropertiesAsJSON(JSONObject& json) const {
  if (IsTombstone())
    json.SetBoolean("ISTOMBSTONE", true);

  json.SetString("id", GetId().ToString());
  json.SetString("visualRect", VisualRect().ToString());
  if (OutsetForRasterEffects())
    json.SetDouble("outset", OutsetForRasterEffects());
}

#endif  // DCHECK_IS_ON()

String DisplayItem::Id::ToString() const {
#if DCHECK_IS_ON()
  return String::Format("%s:%s:%d", client.ToString().Utf8().c_str(),
                        DisplayItem::TypeAsDebugString(type).Utf8().c_str(),
                        fragment);
#else
  return String::Format("%p:%d:%d", &client, static_cast<int>(type), fragment);
#endif
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
#if DCHECK_IS_ON()
  return os << item.AsDebugString().Utf8();
#else
  return os << "{\"id\": " << item.GetId() << "}";
#endif
}

}  // namespace blink
