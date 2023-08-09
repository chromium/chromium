// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/image_decoder_impl.h"

#include <string.h>

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/codec/png_codec.h"
#endif

namespace data_decoder {

namespace {

int64_t kPadding = 64;

void ResizeImage(SkBitmap* decoded_image,
                 bool shrink_to_fit,
                 int64_t max_size_in_bytes) {
  // When serialized, the space taken up by a skia::mojom::BitmapN32 excluding
  // the pixel data payload should be:
  //   sizeof(skia::mojom::BitmapN32::Data_) +
  //       pixel data array header (8 bytes)
  // Use a bigger number in case we need padding at the end.
  int64_t struct_size = sizeof(skia::mojom::BitmapN32::Data_) + kPadding;
  int64_t image_size = decoded_image->computeByteSize();
  int halves = 0;
  while (struct_size + (image_size >> 2 * halves) > max_size_in_bytes)
    halves++;
  if (halves) {
    // If the decoded image is too large, either discard it or shrink it.
    //
    // TODO(rockot): Also support exposing the bytes via shared memory for
    // larger images. https://crbug.com/416916.
    if (shrink_to_fit) {
      // Shrinking by halves prevents quality loss and should never
      // overshrink on displays smaller than 3600x2400.
      *decoded_image = skia::ImageOperations::Resize(
          *decoded_image, skia::ImageOperations::RESIZE_LANCZOS3,
          decoded_image->width() >> halves, decoded_image->height() >> halves);
    } else {
      decoded_image->reset();
    }
  }
}

}  // namespace

ImageDecoderImpl::ImageDecoderImpl() = default;

ImageDecoderImpl::~ImageDecoderImpl() = default;

void ImageDecoderImpl::DecodeImage(mojo_base::BigBuffer encoded_data,
                                   mojom::ImageCodec codec,
                                   bool shrink_to_fit,
                                   int64_t max_size_in_bytes,
                                   const gfx::Size& desired_image_frame_size,
                                   DecodeImageCallback callback) {
  TRACE_EVENT0("ui", "ImageDecoderImpl::DecodeImage");
  base::ElapsedTimer timer;

  if (encoded_data.size() == 0) {
    std::move(callback).Run(timer.Elapsed(), SkBitmap());
    return;
  }

  SkBitmap decoded_image;
#if BUILDFLAG(IS_CHROMEOS)
  if (codec == mojom::ImageCodec::kPng) {
    // Our PNG decoding is using libpng.
    if (encoded_data.size()) {
      SkBitmap decoded_png;
      if (gfx::PNGCodec::Decode(encoded_data.data(), encoded_data.size(),
                                &decoded_png)) {
        decoded_image = decoded_png;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (codec == mojom::ImageCodec::kDefault) {
    decoded_image = blink::WebImage::FromData(
        blink::WebData(reinterpret_cast<const char*>(encoded_data.data()),
                       encoded_data.size()),
        desired_image_frame_size);
  }

  if (!decoded_image.isNull())
    ResizeImage(&decoded_image, shrink_to_fit, max_size_in_bytes);

  std::move(callback).Run(timer.Elapsed(), decoded_image);
}

void ImageDecoderImpl::DecodeAnimation(mojo_base::BigBuffer encoded_data,
                                       bool shrink_to_fit,
                                       int64_t max_size_in_bytes,
                                       DecodeAnimationCallback callback) {
  TRACE_EVENT0("ui", "ImageDecoderImpl::DecodeAnimation");
  if (encoded_data.size() == 0) {
    std::move(callback).Run(std::vector<mojom::AnimationFramePtr>());
    return;
  }

  auto frames = blink::WebImage::AnimationFromData(blink::WebData(
      reinterpret_cast<const char*>(encoded_data.data()), encoded_data.size()));
  if (frames.size() == 0) {
    std::move(callback).Run({});
    return;
  }

  int64_t max_frame_size_in_bytes = max_size_in_bytes / frames.size();
  std::vector<mojom::AnimationFramePtr> decoded_images;

  for (const blink::WebImage::AnimationFrame& frame : frames) {
    auto image_frame = mojom::AnimationFrame::New();
    image_frame->bitmap = frame.bitmap;
    image_frame->duration = frame.duration;

    ResizeImage(&image_frame->bitmap, shrink_to_fit, max_frame_size_in_bytes);
    // Resizing reset the frame because it was too large. Clear out any
    // previously decoded frames so we do not return a partially decoded image.
    if (image_frame->bitmap.isNull()) {
      decoded_images.clear();
      break;
    }

    decoded_images.push_back(std::move(image_frame));
  }

  std::move(callback).Run(std::move(decoded_images));
}

}  // namespace data_decoder
