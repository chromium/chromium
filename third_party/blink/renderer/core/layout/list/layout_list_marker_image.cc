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

gfx::SizeF LayoutListMarkerImage::DefaultSize() const {
  NOT_DESTROYED();
  const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return gfx::SizeF(kDefaultWidth, kDefaultHeight);
  float bullet_width = font_data->GetFontMetrics().Ascent() / 2.f;
  return gfx::SizeF(bullet_width, bullet_width);
}

PhysicalNaturalSizingInfo LayoutListMarkerImage::GetNaturalDimensions() const {
  NOT_DESTROYED();
  PhysicalNaturalSizingInfo sizing_info = LayoutImage::GetNaturalDimensions();

  // If this is an image without natural width and height, compute the concrete
  // object size by using the specified default object size.
  if (sizing_info.size.IsEmpty()) {
    // Because ImageResource() is always LayoutImageResourceStyleImage. So we
    // could use StyleImage::ImageSize to determine the concrete object size
    // with default object size(ascent/2 x ascent/2).
    gfx::SizeF concrete_size = ImageResource()->ConcreteObjectSize(
        StyleRef().EffectiveZoom(), DefaultSize());
    concrete_size.Scale(ImageDevicePixelRatio());

    sizing_info = PhysicalNaturalSizingInfo::MakeFixed(
        PhysicalSize::FromSizeFRound(concrete_size));
  }
  return sizing_info;
}

}  // namespace blink
