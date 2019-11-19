/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc.
 *               All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_IMAGE_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_IMAGE_RESOURCE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_image.h"

namespace blink {

class LayoutObject;

class CORE_EXPORT LayoutImageResource
    : public GarbageCollected<LayoutImageResource> {
 public:
  LayoutImageResource();
  virtual ~LayoutImageResource();

  virtual void Initialize(LayoutObject*);
  virtual void Shutdown();

  void SetImageResource(ImageResourceContent*);
  ImageResourceContent* CachedImage() const { return cached_image_.Get(); }
  virtual bool HasImage() const { return cached_image_; }

  void ResetAnimation();
  bool MaybeAnimated() const;

  virtual scoped_refptr<Image> GetImage(const FloatSize&) const;
  scoped_refptr<Image> GetImage(const IntSize&) const;
  virtual bool ErrorOccurred() const {
    return cached_image_ && cached_image_->ErrorOccurred();
  }

  // Replace the resource this object references with a reference to
  // the "broken image".
  void UseBrokenImage();

  virtual bool HasIntrinsicSize() const;

  virtual FloatSize ImageSize(float multiplier) const;
  // Default size is effective when this is LayoutImageResourceStyleImage.
  virtual FloatSize ImageSizeWithDefaultSize(float multiplier,
                                             const LayoutSize&) const;
  virtual WrappedImagePtr ImagePtr() const { return cached_image_.Get(); }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(cached_image_); }

 protected:
  // Device scale factor for the associated LayoutObject.
  float DeviceScaleFactor() const;
  // Returns an image based on the passed device scale factor.
  static Image* BrokenImage(float device_scale_factor);

  LayoutObject* layout_object_;
  Member<ImageResourceContent> cached_image_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutImageResource);
};

}  // namespace blink

#endif  // LayoutImage_h
