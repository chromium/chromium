// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/layout_list_marker_image.h"

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

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
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return gfx::SizeF(kDefaultWidth, kDefaultHeight);
  float bullet_width = font_data->GetFontMetrics().Ascent() / 2.f;
  return gfx::SizeF(bullet_width, bullet_width);
}

// Because ImageResource() is always LayoutImageResourceStyleImage. So we could
// use StyleImage::ImageSize to determine the concrete object size with
// default object size(ascent/2 x ascent/2).
void LayoutListMarkerImage::ComputeIntrinsicSizingInfoByDefaultSize(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  gfx::SizeF concrete_size = ImageResource()->ConcreteObjectSize(
      Style()->EffectiveZoom(), DefaultSize());
  concrete_size.Scale(ImageDevicePixelRatio());

  intrinsic_sizing_info.size = concrete_size;
  intrinsic_sizing_info.has_width = true;
  intrinsic_sizing_info.has_height = true;
}

void LayoutListMarkerImage::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  LayoutImage::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

  // If this is an image without intrinsic width and height, compute the
  // concrete object size by using the specified default object size.
  if (intrinsic_sizing_info.size.IsEmpty()) {
    ComputeIntrinsicSizingInfoByDefaultSize(intrinsic_sizing_info);
  }
}

}  // namespace blink
