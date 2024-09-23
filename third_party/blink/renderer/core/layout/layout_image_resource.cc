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

#include "third_party/blink/renderer/core/layout/layout_image_resource.h"

#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace blink {

namespace {

gfx::SizeF ApplyClampedZoom(gfx::SizeF size, float multiplier) {
  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  gfx::SizeF minimum_size(size.width() > 0 ? 1 : 0, size.height() > 0 ? 1 : 0);
  size.Scale(multiplier);
  if (size.width() < minimum_size.width()) {
    size.set_width(minimum_size.width());
  }
  if (size.height() < minimum_size.height()) {
    size.set_height(minimum_size.height());
  }
  return size;
}

}  // namespace

LayoutImageResource::LayoutImageResource()
    : layout_object_(nullptr), cached_image_(nullptr) {}

LayoutImageResource::~LayoutImageResource() = default;

void LayoutImageResource::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  visitor->Trace(cached_image_);
}

void LayoutImageResource::Initialize(LayoutObject* layout_object) {
  DCHECK(!layout_object_);
  DCHECK(layout_object);
  layout_object_ = layout_object;
}

void LayoutImageResource::Shutdown() {
  DCHECK(layout_object_);

  if (!cached_image_)
    return;
  cached_image_->RemoveObserver(layout_object_);
}

void LayoutImageResource::SetImageResource(ImageResourceContent* new_image) {
  DCHECK(layout_object_);

  if (cached_image_ == new_image)
    return;

  if (cached_image_) {
    cached_image_->RemoveObserver(layout_object_);
  }
  cached_image_ = new_image;
  if (cached_image_) {
    cached_image_->AddObserver(layout_object_);
    if (cached_image_->ErrorOccurred()) {
      layout_object_->ImageChanged(
          cached_image_.Get(),
          ImageResourceObserver::CanDeferInvalidation::kNo);
    }
  } else {
    layout_object_->ImageChanged(
        cached_image_.Get(), ImageResourceObserver::CanDeferInvalidation::kNo);
  }
}

ResourcePriority LayoutImageResource::ComputeResourcePriority() const {
  if (!layout_object_)
    return ResourcePriority();
  return layout_object_->ComputeResourcePriority();
}

void LayoutImageResource::ResetAnimation() {
  DCHECK(layout_object_);

  if (!cached_image_)
    return;

  cached_image_->GetImage()->ResetAnimation();

  layout_object_->SetShouldDoFullPaintInvalidation();
}

bool LayoutImageResource::HasIntrinsicSize() const {
  return !cached_image_ || cached_image_->GetImage()->HasIntrinsicSize();
}

RespectImageOrientationEnum LayoutImageResource::ImageOrientation() const {
  DCHECK(cached_image_);
  // Always respect the orientation of opaque origin images to avoid leaking
  // image data. Otherwise pull orientation from the layout object's style.
  return cached_image_->ForceOrientationIfNecessary(
      layout_object_->StyleRef().ImageOrientation());
}

IntrinsicSizingInfo LayoutImageResource::GetNaturalDimensions(
    float multiplier) const {
  if (!cached_image_ || !cached_image_->IsSizeAvailable() ||
      !cached_image_->HasImage()) {
    return IntrinsicSizingInfo::None();
  }
  IntrinsicSizingInfo sizing_info;
  Image& image = *cached_image_->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    const SVGImageViewInfo* view_info = SVGImageForContainer::CreateViewInfo(
        *svg_image, layout_object_->GetNode());
    if (!SVGImageForContainer::GetNaturalDimensions(*svg_image, view_info,
                                                    sizing_info)) {
      sizing_info = IntrinsicSizingInfo::None();
    }
  } else {
    sizing_info.size = gfx::SizeF(image.Size(ImageOrientation()));
    sizing_info.aspect_ratio = sizing_info.size;
  }
  if (multiplier != 1 && HasIntrinsicSize()) {
    sizing_info.size = ApplyClampedZoom(sizing_info.size, multiplier);
  }
  if (auto* layout_image = DynamicTo<LayoutImage>(*layout_object_)) {
    sizing_info.size.Scale(layout_image->ImageDevicePixelRatio());
  }
  return sizing_info;
}

gfx::SizeF LayoutImageResource::ImageSize(float multiplier) const {
  if (!cached_image_)
    return gfx::SizeF();
  gfx::SizeF size(cached_image_->IntrinsicSize(
      layout_object_->StyleRef().ImageOrientation()));
  if (multiplier != 1 && HasIntrinsicSize()) {
    size = ApplyClampedZoom(size, multiplier);
  }
  if (auto* layout_image = DynamicTo<LayoutImage>(*layout_object_)) {
    size.Scale(layout_image->ImageDevicePixelRatio());
  }
  return size;
}

gfx::SizeF LayoutImageResource::ConcreteObjectSize(
    float multiplier,
    const gfx::SizeF& default_object_size) const {
  IntrinsicSizingInfo sizing_info = GetNaturalDimensions(multiplier);
  return blink::ConcreteObjectSize(sizing_info, default_object_size);
}

Image* LayoutImageResource::BrokenImage(double device_pixel_ratio) {
  // TODO(rendering-core): Replace static resources with dynamically
  // generated ones, to support a wider range of device scale factors.
  if (device_pixel_ratio >= 2) {
    DEFINE_STATIC_REF(
        Image, broken_image_hi_res,
        (Image::LoadPlatformResource(IDR_BROKENIMAGE, ui::k200Percent)));
    return broken_image_hi_res;
  }

  DEFINE_STATIC_REF(Image, broken_image_lo_res,
                    (Image::LoadPlatformResource(IDR_BROKENIMAGE)));
  return broken_image_lo_res;
}

double LayoutImageResource::DevicePixelRatio() const {
  if (!layout_object_)
    return 1.0;
  return layout_object_->GetDocument().DevicePixelRatio();
}

void LayoutImageResource::UseBrokenImage() {
  auto* broken_image =
      ImageResourceContent::CreateLoaded(BrokenImage(DevicePixelRatio()));
  broken_image->SetIsBroken();
  SetImageResource(broken_image);
}

scoped_refptr<Image> LayoutImageResource::GetImage(
    const gfx::Size& container_size) const {
  return GetImage(gfx::SizeF(container_size));
}

scoped_refptr<Image> LayoutImageResource::GetImage(
    const gfx::SizeF& container_size) const {
  if (!cached_image_)
    return Image::NullImage();

  if (cached_image_->ErrorOccurred())
    return BrokenImage(DevicePixelRatio());

  if (!cached_image_->HasImage())
    return Image::NullImage();

  Image* image = cached_image_->GetImage();

  auto* svg_image = DynamicTo<SVGImage>(image);
  if (!svg_image)
    return image;

  const ComputedStyle& style = layout_object_->StyleRef();
  auto preferred_color_scheme = layout_object_->GetDocument()
                                    .GetStyleEngine()
                                    .ResolveColorSchemeForEmbedding(&style);
  const SVGImageViewInfo* view_info = SVGImageForContainer::CreateViewInfo(
      *svg_image, layout_object_->GetNode());
  return SVGImageForContainer::Create(*svg_image, container_size,
                                      style.EffectiveZoom(), view_info,
                                      preferred_color_scheme);
}

bool LayoutImageResource::MaybeAnimated() const {
  Image* image = cached_image_ ? cached_image_->GetImage() : Image::NullImage();
  return image->MaybeAnimated();
}

}  // namespace blink
