// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/image_loader.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

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

mojom::blink::PreferredColorScheme ImageElementBase::PreferredColorScheme()
    const {
  const Element& element = GetElement();
  const ComputedStyle* style = element.GetComputedStyle();
  return element.GetDocument().GetStyleEngine().ResolveColorSchemeForEmbedding(
      style);
}

bool ImageElementBase::IsSVGSource() const {
  return CachedImage() && IsA<SVGImage>(CachedImage()->GetImage());
}

bool ImageElementBase::IsImageElement() const {
  return CachedImage() && !IsA<SVGImage>(CachedImage()->GetImage());
}

scoped_refptr<Image> ImageElementBase::GetSourceImageForCanvas(
    FlushReason,
    SourceImageStatus* status,
    const gfx::SizeF& default_object_size,
    const AlphaDisposition alpha_disposition) {
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

  if (auto* svg_image = DynamicTo<SVGImage>(source_image.get())) {
    UseCounter::Count(GetElement().GetDocument(), WebFeature::kSVGInCanvas2D);
    const SVGImageViewInfo* view_info =
        SVGImageForContainer::CreateViewInfo(*svg_image, GetElement());
    const gfx::SizeF image_size = SVGImageForContainer::ConcreteObjectSize(
        *svg_image, view_info, default_object_size);
    if (image_size.IsEmpty()) {
      *status = kZeroSizeImageSourceStatus;
      return nullptr;
    }
    source_image = SVGImageForContainer::Create(
        *svg_image, image_size, 1, view_info, PreferredColorScheme());
  }

  if (source_image->Size().IsEmpty()) {
    *status = kZeroSizeImageSourceStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return source_image->ImageForDefaultFrame();
}

bool ImageElementBase::WouldTaintOrigin() const {
  return CachedImage() && !CachedImage()->IsAccessAllowed();
}

gfx::SizeF ImageElementBase::ElementSize(
    const gfx::SizeF& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content || !image_content->HasImage())
    return gfx::SizeF();
  Image* image = image_content->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    const SVGImageViewInfo* view_info =
        SVGImageForContainer::CreateViewInfo(*svg_image, GetElement());
    return SVGImageForContainer::ConcreteObjectSize(*svg_image, view_info,
                                                    default_object_size);
  }
  return gfx::SizeF(image->Size(respect_orientation));
}

gfx::SizeF ImageElementBase::DefaultDestinationSize(
    const gfx::SizeF& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  return ElementSize(default_object_size, respect_orientation);
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

gfx::Size ImageElementBase::BitmapSourceSize() const {
  ImageResourceContent* image = CachedImage();
  if (!image)
    return gfx::Size();
  // This method is called by ImageBitmap when creating and cropping the image.
  // Return un-oriented size because the cropping must happen before
  // orienting.
  return image->IntrinsicSize(kDoNotRespectImageOrientation);
}

static bool HasDimensionsForImage(SVGImage& svg_image,
                                  std::optional<gfx::Rect> crop_rect,
                                  const ImageBitmapOptions* options) {
  if (crop_rect) {
    return true;
  }
  if (options->hasResizeWidth() && options->hasResizeHeight()) {
    return true;
  }
  if (!SVGImageForContainer::ConcreteObjectSize(svg_image, nullptr,
                                                gfx::SizeF())
           .IsEmpty()) {
    return true;
  }
  return false;
}

ScriptPromise<ImageBitmap> ImageElementBase::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No image can be retrieved from the provided element.");
    return EmptyPromise();
  }
  if (options->hasResizeWidth() && options->resizeWidth() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The resize width dimension is equal to 0.");
    return EmptyPromise();
  }
  if (options->hasResizeHeight() && options->resizeHeight() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The resize width dimension is equal to 0.");
    return EmptyPromise();
  }
  if (auto* svg_image = DynamicTo<SVGImage>(image_content->GetImage())) {
    if (!HasDimensionsForImage(*svg_image, crop_rect, options)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The image element contains an SVG image without intrinsic "
          "dimensions, and no resize options or crop region are "
          "specified.");
      return EmptyPromise();
    }
    // The following function only works on SVGImages (as checked above).
    return ImageBitmap::CreateAsync(
        this, crop_rect, script_state,
        GetElement().GetDocument().GetTaskRunner(TaskType::kInternalDefault),
        PreferredColorScheme(), exception_state, options);
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      options, exception_state);
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
