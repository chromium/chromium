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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"
#include "ui/base/resource/scale_factor.h"

namespace blink {

LayoutImageResource::LayoutImageResource()
    : layout_object_(nullptr), cached_image_(nullptr) {}

LayoutImageResource::~LayoutImageResource() = default;

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

FloatSize LayoutImageResource::ImageSize(float multiplier) const {
  if (!cached_image_)
    return FloatSize();
  FloatSize size(cached_image_->IntrinsicSize(
      LayoutObject::ShouldRespectImageOrientation(layout_object_)));
  if (multiplier != 1 && HasIntrinsicSize()) {
    // Don't let images that have a width/height >= 1 shrink below 1 when
    // zoomed.
    FloatSize minimum_size(size.Width() > 0 ? 1 : 0, size.Height() > 0 ? 1 : 0);
    size.Scale(multiplier);
    if (size.Width() < minimum_size.Width())
      size.SetWidth(minimum_size.Width());
    if (size.Height() < minimum_size.Height())
      size.SetHeight(minimum_size.Height());
  }
  if (layout_object_ && layout_object_->IsLayoutImage() && size.Width() &&
      size.Height())
    size.Scale(ToLayoutImage(layout_object_)->ImageDevicePixelRatio());
  return size;
}

FloatSize LayoutImageResource::ImageSizeWithDefaultSize(
    float multiplier,
    const LayoutSize&) const {
  return ImageSize(multiplier);
}

float LayoutImageResource::DeviceScaleFactor() const {
  return DeviceScaleFactorDeprecated(layout_object_->GetFrame());
}

Image* LayoutImageResource::BrokenImage(float device_scale_factor) {
  // TODO(schenney): Replace static resources with dynamically
  // generated ones, to support a wider range of device scale factors.
  if (device_scale_factor >= 2) {
    DEFINE_STATIC_REF(
        Image, broken_image_hi_res,
        (Image::LoadPlatformResource(IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P)));
    return broken_image_hi_res;
  }

  DEFINE_STATIC_REF(Image, broken_image_lo_res,
                    (Image::LoadPlatformResource(IDR_BROKENIMAGE)));
  return broken_image_lo_res;
}

void LayoutImageResource::UseBrokenImage() {
  SetImageResource(
      ImageResourceContent::CreateLoaded(BrokenImage(DeviceScaleFactor())));
}

scoped_refptr<Image> LayoutImageResource::GetImage(
    const IntSize& container_size) const {
  return GetImage(FloatSize(container_size));
}

scoped_refptr<Image> LayoutImageResource::GetImage(
    const FloatSize& container_size) const {
  if (!cached_image_)
    return Image::NullImage();

  if (cached_image_->ErrorOccurred())
    return BrokenImage(DeviceScaleFactor());

  if (!cached_image_->HasImage())
    return Image::NullImage();

  Image* image = cached_image_->GetImage();
  if (image->IsPlaceholderImage()) {
    static_cast<PlaceholderImage*>(image)->SetIconAndTextScaleFactor(
        layout_object_->StyleRef().EffectiveZoom());
  }

  if (!image->IsSVGImage())
    return image;

  KURL url;
  if (auto* element = DynamicTo<Element>(layout_object_->GetNode())) {
    const AtomicString& url_string = element->ImageSourceURL();
    url = element->GetDocument().CompleteURL(url_string);
  }
  return SVGImageForContainer::Create(
      ToSVGImage(image), container_size,
      layout_object_->StyleRef().EffectiveZoom(), url);
}

bool LayoutImageResource::MaybeAnimated() const {
  Image* image = cached_image_ ? cached_image_->GetImage() : Image::NullImage();
  return image->MaybeAnimated();
}

}  // namespace blink
