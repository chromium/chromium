/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_image.h"

#include <algorithm>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

SkBitmap WebImage::FromData(const WebData& data,
                            const gfx::Size& desired_size) {
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      data, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::kIgnore,
      cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes()));
  if (!decoder || !decoder->IsSizeAvailable())
    return {};

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Pick the frame closest to |desiredSize|'s area without being smaller,
  // which has the highest bit depth.
  const wtf_size_t frame_count = decoder->FrameCount();
  wtf_size_t index = 0;  // Default to first frame if none are large enough.
  uint64_t frame_area_at_index = 0;
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    const gfx::Size frame_size = decoder->FrameSizeAtIndex(i);
    if (frame_size == desired_size) {
      index = i;
      break;  // Perfect match.
    }

    uint64_t frame_area = frame_size.Area64();
    if (frame_area < desired_size.Area64())
      break;  // No more frames that are large enough.

    if (!i || (frame_area < frame_area_at_index)) {
      index = i;  // Closer to desired area than previous best match.
      frame_area_at_index = frame_area;
    }
  }

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(index);
  if (!frame || decoder->Failed() || frame->Bitmap().drawsNothing()) {
    return {};
  }

  if (decoder->Orientation() == ImageOrientationEnum::kDefault) {
    return frame->Bitmap();
  }

  cc::PaintImage paint_image(Image::ResizeAndOrientImage(
      cc::PaintImage::CreateFromBitmap(frame->Bitmap()),
      decoder->Orientation()));

  SkBitmap bitmap;
  paint_image.GetSwSkImage()->asLegacyBitmap(&bitmap);
  return bitmap;
}

SkBitmap WebImage::DecodeSVG(const WebData& data,
                             const gfx::Size& desired_size) {
  scoped_refptr<SVGImage> svg_image = SVGImage::Create(nullptr);
  const bool data_complete = true;
  Image::SizeAvailability size_available =
      svg_image->SetData(data, data_complete);
  // If we're not able to determine a size after feeding all the data, we don't
  // have a valid SVG image, and return an empty SkBitmap.
  SkBitmap bitmap;
  if (size_available == Image::kSizeUnavailable)
    return bitmap;
  // If the desired size is non-empty, use it directly as the container
  // size. This is likely what most (all?) users of this function will
  // expect/want. If the desired size is empty, then use the intrinsic size of
  // image.
  gfx::SizeF container_size(desired_size);
  if (container_size.IsEmpty()) {
    container_size = SVGImageForContainer::ConcreteObjectSize(
        *svg_image, nullptr, gfx::SizeF());
  }
  // TODO(chrishtr): perhaps the downloaded image should be decoded in dark
  // mode if the preferred color scheme is dark.
  scoped_refptr<Image> svg_container =
      SVGImageForContainer::Create(*svg_image, container_size, 1, nullptr);
  if (PaintImage image = svg_container->PaintImageForCurrentFrame()) {
    image.GetSwSkImage()->asLegacyBitmap(&bitmap,
                                         SkImage::kRO_LegacyBitmapMode);
  }
  return bitmap;
}

WebVector<SkBitmap> WebImage::FramesFromData(const WebData& data) {
  // This is to protect from malicious images. It should be big enough that it's
  // never hit in practice.
  const wtf_size_t kMaxFrameCount = 8;

  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      data, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::kIgnore,
      cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes()));
  if (!decoder || !decoder->IsSizeAvailable())
    return {};

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Keep the first frame at every size, has the highest bit depth.
  const wtf_size_t frame_count = decoder->FrameCount();
  gfx::Size last_size;

  WebVector<SkBitmap> frames;
  for (wtf_size_t i = 0; i < std::min(frame_count, kMaxFrameCount); ++i) {
    const gfx::Size frame_size = decoder->FrameSizeAtIndex(i);
    if (frame_size == last_size)
      continue;
    last_size = frame_size;

    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    if (!frame)
      continue;

    SkBitmap bitmap = frame->Bitmap();
    if (!bitmap.isNull() && frame->GetStatus() == ImageFrame::kFrameComplete)
      frames.emplace_back(std::move(bitmap));
  }

  return frames;
}

WebVector<WebImage::AnimationFrame> WebImage::AnimationFromData(
    const WebData& data) {
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      data, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::kIgnore,
      cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes()));
  if (!decoder || !decoder->IsSizeAvailable() || decoder->FrameCount() == 0)
    return {};

  const wtf_size_t frame_count = decoder->FrameCount();
  gfx::Size last_size = decoder->FrameSizeAtIndex(0);

  WebVector<WebImage::AnimationFrame> frames;
  frames.reserve(frame_count);
  for (wtf_size_t i = 0; i < frame_count; ++i) {
    // If frame size changes, this is most likely not an animation and is
    // instead an image with multiple versions at different resolutions. If
    // that's the case, return only the first frame (or no frames if we failed
    // decoding the first one).
    if (last_size != decoder->FrameSizeAtIndex(i)) {
      frames.resize(frames.empty() ? 0 : 1);
      return frames;
    }
    last_size = decoder->FrameSizeAtIndex(i);

    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);

    SkBitmap bitmap = frame->Bitmap();
    if (bitmap.isNull() || frame->GetStatus() != ImageFrame::kFrameComplete)
      continue;

    // Make the bitmap a deep copy, otherwise the next loop iteration will
    // replace the contents of the previous frame. DecodeFrameBufferAtIndex
    // reuses the same underlying pixel buffer.
    bitmap.setImmutable();

    AnimationFrame output;
    output.bitmap = bitmap;
    output.duration = frame->Duration();
    frames.emplace_back(std::move(output));
  }

  return frames;
}

}  // namespace blink
