// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_list_marker_image.h"

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

namespace blink {

LayoutListMarkerImage::LayoutListMarkerImage(Element* element)
    : LayoutImage(element) {}

LayoutListMarkerImage* LayoutListMarkerImage::CreateAnonymous(
    Document* document) {
  LayoutListMarkerImage* object = new LayoutListMarkerImage(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutListMarkerImage::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectListMarkerImage || LayoutImage::IsOfType(type);
}

LayoutSize LayoutListMarkerImage::DefaultSize() const {
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutSize(kDefaultWidth, kDefaultHeight);
  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  return LayoutSize(bullet_width, bullet_width);
}

// Because ImageResource() is always LayoutImageResourceStyleImage. So we could
// use StyleImage::ImageSize to determine the concrete object size with
// default object size(ascent/2 x ascent/2).
void LayoutListMarkerImage::ComputeIntrinsicSizingInfoByDefaultSize(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  FloatSize concrete_size = ImageResource()->ImageSizeWithDefaultSize(
      Style()->EffectiveZoom(), FloatSize(DefaultSize()));
  concrete_size.Scale(ImageDevicePixelRatio());
  LayoutSize image_size(RoundedLayoutSize(concrete_size));

  intrinsic_sizing_info.size.SetWidth(image_size.Width());
  intrinsic_sizing_info.size.SetHeight(image_size.Height());
  intrinsic_sizing_info.has_width = true;
  intrinsic_sizing_info.has_height = true;
}

void LayoutListMarkerImage::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  LayoutImage::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

  // If this is an image without intrinsic width and height, compute the
  // concrete object size by using the specified default object size.
  if (intrinsic_sizing_info.size.IsEmpty() && ImageResource())
    ComputeIntrinsicSizingInfoByDefaultSize(intrinsic_sizing_info);
}

}  // namespace blink
