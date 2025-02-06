// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/layout_list_marker_image.h"

#include "third_party/blink/renderer/core/layout/natural_sizing_info.h"

namespace blink {

LayoutListMarkerImage::LayoutListMarkerImage(Element* element)
    : LayoutImage(element) {}

LayoutListMarkerImage* LayoutListMarkerImage::CreateAnonymous(
    Document* document) {
  LayoutListMarkerImage* object =
      MakeGarbageCollected<LayoutListMarkerImage>(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

PhysicalSize LayoutListMarkerImage::DefaultSize() const {
  NOT_DESTROYED();
  const SimpleFontData* font_data = StyleRef().GetFont()->PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return PhysicalSize(LayoutUnit(kDefaultWidth), LayoutUnit(kDefaultHeight));
  }
  const LayoutUnit bullet_width =
      LayoutUnit(font_data->GetFontMetrics().Ascent()) / 2;
  return PhysicalSize(bullet_width, bullet_width);
}

PhysicalNaturalSizingInfo LayoutListMarkerImage::GetNaturalDimensions() const {
  NOT_DESTROYED();
  PhysicalNaturalSizingInfo sizing_info = LayoutImage::GetNaturalDimensions();

  // If this is an image without natural width and height, compute the concrete
  // object size by using a default object size of (ascent/2 x ascent/2).
  if (sizing_info.size.IsEmpty()) {
    const auto natural_dimensions = PhysicalNaturalSizingInfo::FromSizingInfo(
        ImageResource()->GetNaturalDimensions(StyleRef().EffectiveZoom()));
    sizing_info = PhysicalNaturalSizingInfo::MakeFixed(
        ConcreteObjectSize(natural_dimensions, DefaultSize()));
  }
  return sizing_info;
}

}  // namespace blink
