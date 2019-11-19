// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/image_loader.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
Image::ImageDecodingMode ImageElementBase::ParseImageDecodingMode(
    const AtomicString& async_attr_value) {
  if (async_attr_value.IsNull())
    return Image::kUnspecifiedDecode;

  const auto& value = async_attr_value.LowerASCII();
  if (value == "async")
    return Image::kAsyncDecode;
  if (value == "sync")
    return Image::kSyncDecode;
  return Image::kUnspecifiedDecode;
}

ImageResourceContent* ImageElementBase::CachedImage() const {
  return GetImageLoader().GetContent();
}

const Element& ImageElementBase::GetElement() const {
  return *GetImageLoader().GetElement();
}

bool ImageElementBase::IsSVGSource() const {
  return CachedImage() && CachedImage()->GetImage()->IsSVGImage();
}

bool ImageElementBase::IsImageElement() const {
  return CachedImage() && !CachedImage()->GetImage()->IsSVGImage();
}

scoped_refptr<Image> ImageElementBase::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    const FloatSize& default_object_size) {
  ImageResourceContent* image_content = CachedImage();
  if (!GetImageLoader().ImageComplete() || !image_content) {
    *status = kIncompleteSourceImageStatus;
    return nullptr;
  }

  if (image_content->ErrorOccurred()) {
    *status = kUndecodableSourceImageStatus;
    return nullptr;
  }

  scoped_refptr<Image> source_image = image_content->GetImage();
  if (source_image->IsSVGImage()) {
    UseCounter::Count(GetElement().GetDocument(), WebFeature::kSVGInCanvas2D);
    SVGImage* svg_image = ToSVGImage(source_image.get());
    FloatSize image_size = svg_image->ConcreteObjectSize(default_object_size);
    source_image = SVGImageForContainer::Create(
        svg_image, image_size, 1,
        GetElement().GetDocument().CompleteURL(GetElement().ImageSourceURL()));
  }

  *status = kNormalSourceImageStatus;
  return source_image->ImageForDefaultFrame();
}

bool ImageElementBase::WouldTaintOrigin() const {
  return CachedImage() && !CachedImage()->IsAccessAllowed();
}

FloatSize ImageElementBase::ElementSize(
    const FloatSize& default_object_size) const {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content)
    return FloatSize();

  Image* image = image_content->GetImage();
  if (image->IsSVGImage())
    return ToSVGImage(image)->ConcreteObjectSize(default_object_size);

  return FloatSize(
      image_content->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(
          GetElement().GetLayoutObject())));
}

FloatSize ImageElementBase::DefaultDestinationSize(
    const FloatSize& default_object_size) const {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content)
    return FloatSize();

  Image* image = image_content->GetImage();
  if (image->IsSVGImage())
    return ToSVGImage(image)->ConcreteObjectSize(default_object_size);

  return FloatSize(
      image_content->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(
          GetElement().GetLayoutObject())));
}

bool ImageElementBase::IsAccelerated() const {
  return false;
}

const KURL& ImageElementBase::SourceURL() const {
  return CachedImage()->GetResponse().CurrentRequestUrl();
}

bool ImageElementBase::IsOpaque() const {
  ImageResourceContent* image_content = CachedImage();
  if (!GetImageLoader().ImageComplete() || !image_content)
    return false;
  Image* image = image_content->GetImage();
  return image->CurrentFrameKnownToBeOpaque();
}

IntSize ImageElementBase::BitmapSourceSize() const {
  ImageResourceContent* image = CachedImage();
  if (!image)
    return IntSize();
  return image->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(
      GetElement().GetLayoutObject()));
}

ScriptPromise ImageElementBase::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions* options) {
  DCHECK(event_target.ToLocalDOMWindow());

  ImageResourceContent* image_content = CachedImage();
  if (!image_content) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "No image can be retrieved from the provided element."));
  }
  Image* image = image_content->GetImage();
  if (image->IsSVGImage()) {
    if (!ToSVGImage(image)->HasIntrinsicDimensions() &&
        (!crop_rect &&
         (!options->hasResizeWidth() || !options->hasResizeHeight()))) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kInvalidStateError,
              "The image element contains an SVG image without intrinsic "
              "dimensions, and no resize options or crop region are "
              "specified."));
    }
    return ImageBitmap::CreateAsync(this, crop_rect,
                                    event_target.ToLocalDOMWindow()->document(),
                                    script_state, options);
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(
                        this, crop_rect,
                        event_target.ToLocalDOMWindow()->document(), options));
}

Image::ImageDecodingMode ImageElementBase::GetDecodingModeForPainting(
    PaintImage::Id new_id) {
  const bool content_transitioned =
      last_painted_image_id_ != PaintImage::kInvalidId &&
      new_id != PaintImage::kInvalidId && last_painted_image_id_ != new_id;
  last_painted_image_id_ = new_id;

  // If the image for the element was transitioned, and no preference has been
  // specified by the author, prefer sync decoding to avoid flickering the
  // element. Async decoding of this image would cause us to display
  // intermediate frames with no image while the decode is in progress which
  // creates a visual flicker in the transition.
  if (content_transitioned &&
      decoding_mode_ == Image::ImageDecodingMode::kUnspecifiedDecode)
    return Image::ImageDecodingMode::kSyncDecode;
  return decoding_mode_;
}

}  // namespace blink
