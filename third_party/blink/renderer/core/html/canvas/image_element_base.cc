// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
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

bool ImageElementBase::IsImageElement() const {
  return CachedImage() && !IsA<SVGImage>(CachedImage()->GetImage());
}

scoped_refptr<Image> ImageElementBase::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const gfx::SizeF& default_object_size) {
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
  return image->IsOpaque();
}

static bool HasDimensionsForImage(SVGImage& svg_image,
                                  const ImageBitmapOptions* options) {
  if (options->hasResizeWidth() && options->hasResizeHeight()) {
    return true;
  }
  std::optional<NaturalSizingInfo> sizing_info =
      SVGImageForContainer::GetNaturalDimensions(svg_image, nullptr);
  return sizing_info && sizing_info->has_width && sizing_info->has_height;
}

ImageBitmapSourceStatus ImageElementBase::CheckUsability() const {
  ImageResourceContent* image_content = CachedImage();
  if (!GetImageLoader().ImageComplete() || !image_content) {
    return base::unexpected(ImageBitmapSourceError::kIncomplete);
  }
  if (image_content->ErrorOccurred()) {
    return base::unexpected(ImageBitmapSourceError::kUndecodable);
  }
  // Check if the width/height are explicitly zero.
  bool width_is_zero = false;
  bool height_is_zero = false;
  Image& image = *image_content->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    std::optional<NaturalSizingInfo> sizing_info =
        SVGImageForContainer::GetNaturalDimensions(*svg_image, nullptr);
    if (!sizing_info) {
      return base::unexpected(ImageBitmapSourceError::kIncomplete);
    }
    width_is_zero = sizing_info->has_width && sizing_info->size.width() == 0;
    height_is_zero = sizing_info->has_height && sizing_info->size.height() == 0;
  } else {
    const gfx::Size image_size = image.Size();
    width_is_zero = image_size.width() == 0;
    height_is_zero = image_size.height() == 0;
  }
  if (width_is_zero || height_is_zero) {
    return base::unexpected(width_is_zero
                                ? ImageBitmapSourceError::kZeroWidth
                                : ImageBitmapSourceError::kZeroHeight);
  }
  return base::ok();
}

ScriptPromise<ImageBitmap> ImageElementBase::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  // This was checked by CheckUsability().
  CHECK(CachedImage());
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
  Image& image = *CachedImage()->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    if (!HasDimensionsForImage(*svg_image, options)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The image element contains an SVG image without natural "
          "dimensions, and no resize options are specified.");
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
