/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"

#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_marker_image.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"

namespace blink {

LayoutImageResourceStyleImage::LayoutImageResourceStyleImage(
    StyleImage* style_image)
    : style_image_(style_image) {
  DCHECK(style_image_);
}

LayoutImageResourceStyleImage::~LayoutImageResourceStyleImage() {
  DCHECK(!cached_image_);
}

void LayoutImageResourceStyleImage::Initialize(LayoutObject* layout_object) {
  LayoutImageResource::Initialize(layout_object);

  if (style_image_->IsImageResource())
    cached_image_ = To<StyleFetchedImage>(style_image_.Get())->CachedImage();

  style_image_->AddClient(layout_object_);
}

void LayoutImageResourceStyleImage::Shutdown() {
  DCHECK(layout_object_);
  style_image_->RemoveClient(layout_object_);
  cached_image_ = nullptr;
}

scoped_refptr<Image> LayoutImageResourceStyleImage::GetImage(
    const gfx::SizeF& size) const {
  // Generated content may trigger calls to image() while we're still pending,
  // don't assert but gracefully exit.
  if (style_image_->IsPendingImage())
    return nullptr;
  return style_image_->GetImage(*layout_object_, layout_object_->GetDocument(),
                                layout_object_->StyleRef(), size);
}

gfx::SizeF LayoutImageResourceStyleImage::ImageSize(float multiplier) const {
  // TODO(davve): Find out the correct default object size in this context.
  auto* list_marker = DynamicTo<LayoutListMarkerImage>(layout_object_.Get());
  gfx::SizeF default_size = list_marker
                                ? list_marker->DefaultSize()
                                : gfx::SizeF(LayoutReplaced::kDefaultWidth,
                                             LayoutReplaced::kDefaultHeight);
  return ConcreteObjectSize(multiplier, default_size);
}

gfx::SizeF LayoutImageResourceStyleImage::ConcreteObjectSize(
    float multiplier,
    const gfx::SizeF& default_object_size) const {
  return style_image_->ImageSize(multiplier, default_object_size,
                                 ImageOrientation());
}

IntrinsicSizingInfo LayoutImageResourceStyleImage::GetNaturalDimensions(
    float multiplier) const {
  // Always respect the orientation of opaque origin images to avoid leaking
  // image data. Otherwise pull orientation from the layout object's style.
  return style_image_->GetNaturalSizingInfo(multiplier, ImageOrientation());
}

RespectImageOrientationEnum LayoutImageResourceStyleImage::ImageOrientation()
    const {
  // Always respect the orientation of opaque origin images to avoid leaking
  // image data. Otherwise pull orientation from the layout object's style.
  RespectImageOrientationEnum respect_orientation =
      layout_object_->StyleRef().ImageOrientation();
  return style_image_->ForceOrientationIfNecessary(respect_orientation);
}

void LayoutImageResourceStyleImage::Trace(Visitor* visitor) const {
  visitor->Trace(style_image_);
  LayoutImageResource::Trace(visitor);
}

}  // namespace blink
