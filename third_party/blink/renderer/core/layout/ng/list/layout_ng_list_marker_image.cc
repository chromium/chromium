// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker_image.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

namespace blink {

LayoutNGListMarkerImage::LayoutNGListMarkerImage(Element* element)
    : LayoutImage(element) {}

LayoutNGListMarkerImage* LayoutNGListMarkerImage::CreateAnonymous(
    Document* document) {
  LayoutNGListMarkerImage* object = new LayoutNGListMarkerImage(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGListMarkerImage::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListMarkerImage || LayoutImage::IsOfType(type);
}

Node* LayoutNGListMarkerImage::NodeForHitTest() const {
  // In LayoutNG tree, image list marker is structured like this:
  // <li> (LayoutListItem)
  //   <anonymous block> (LayoutNGListMarker or LayoutNGInsideListMarker)
  //     <anonymous img> (LayoutNGListMarkerImage)
  // Hit testing should return the list-item node.
  DCHECK(!GetNode());
  for (const LayoutObject* parent = Parent(); parent;
       parent = parent->Parent()) {
    if (Node* node = parent->GetNode())
      return node;
  }
  return nullptr;
}

// Because ImageResource() is always LayoutImageResourceStyleImage. So we could
// use StyleImage::ImageSize to determine the concrete object size with
// default object size(ascent/2 x ascent/2).
void LayoutNGListMarkerImage::ComputeIntrinsicSizingInfoByDefaultSize(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  LayoutSize default_object_size(bullet_width, bullet_width);
  FloatSize concrete_size = ImageResource()->ImageSizeWithDefaultSize(
      Style()->EffectiveZoom(), default_object_size);
  concrete_size.Scale(ImageDevicePixelRatio());
  LayoutSize image_size(RoundedLayoutSize(concrete_size));

  intrinsic_sizing_info.size.SetWidth(image_size.Width());
  intrinsic_sizing_info.size.SetHeight(image_size.Height());
  intrinsic_sizing_info.has_width = true;
  intrinsic_sizing_info.has_height = true;
}

void LayoutNGListMarkerImage::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  LayoutImage::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

  // If this is an image without intrinsic width and height, compute the
  // concrete object size by using the specified default object size.
  if (intrinsic_sizing_info.size.IsEmpty() && ImageResource())
    ComputeIntrinsicSizingInfoByDefaultSize(intrinsic_sizing_info);
}

}  // namespace blink
