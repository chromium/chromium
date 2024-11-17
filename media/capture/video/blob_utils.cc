// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/blob_utils.h"

#include "media/base/video_frame.h"
#include "media/capture/video_capture_types.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

#include <vector>

namespace media {

namespace {

static const int kJpegQualityDefault = 90;

libyuv::RotationModeEnum TranslateIntegerRotationToLibyuvRotation(
    const int rotation) {
  switch (rotation) {
    case 0:
      return libyuv::kRotate0;
    case 90:
      return libyuv::kRotate90;
    case 180:
      return libyuv::kRotate180;
    case 270:
      return libyuv::kRotate270;
  }
  return libyuv::kRotate0;
}

mojom::BlobPtr ProduceJpegBlobFromMjpegFrame(const uint8_t* buffer,
                                             const uint32_t bytesused,
                                             const gfx::Size size,
                                             const int rotation) {
  // TODO(shenghao): The rotation handling logic here can be deleted once we
  // don't need to support HALv1 devices anymore.
  if (rotation != 0) {
    // If rotation is not 0, we need to decode the JPEG frame, rotate it
    // according to |rotation|, and then encode it back.
    int output_width = size.width();
    int output_height = size.height();
    if (rotation == 90 || rotation == 270) {
      std::swap(output_width, output_height);
    }
    const int bytes_per_pixel = 4;
    std::vector<uint8_t> bgra_buffer(output_width * output_height *
                                     bytes_per_pixel);
    libyuv::ConvertToARGB(
        buffer, static_cast<size_t>(bytesused), bgra_buffer.data(),
        output_width * bytes_per_pixel, /*crop_x=*/0,
        /*crop_y=*/0, size.width(), size.height(), size.width(), size.height(),
        TranslateIntegerRotationToLibyuvRotation(rotation),
        libyuv::FOURCC_MJPG);
    SkImageInfo info =
        SkImageInfo::Make(output_width, output_height, kBGRA_8888_SkColorType,
                          kOpaque_SkAlphaType);
    SkPixmap src(info, &bgra_buffer[0], output_width * bytes_per_pixel);

    std::optional<std::vector<uint8_t>> rotated_jpeg_data =
        gfx::JPEGCodec::Encode(src, kJpegQualityDefault);
    if (rotated_jpeg_data) {
      mojom::BlobPtr blob = mojom::Blob::New();
      blob->data = std::move(rotated_jpeg_data).value();
      blob->mime_type = "image/jpeg";
      return blob;
    }

    LOG(ERROR)
        << "Failed to encode frame to JPEG. Using non-rotated original frame.";
    // Fall through to the non-rotated case.
  }

  mojom::BlobPtr blob = mojom::Blob::New();
  blob->data.assign(buffer, buffer + bytesused);
  blob->mime_type = "image/jpeg";
  return blob;
}

}  // namespace

mojom::BlobPtr RotateAndBlobify(const uint8_t* buffer,
                                const uint32_t bytesused,
                                const VideoCaptureFormat& capture_format,
                                const int rotation) {
  DCHECK(buffer);
  DCHECK(bytesused);
  DCHECK(capture_format.IsValid());

  const VideoPixelFormat pixel_format = capture_format.pixel_format;
  if (pixel_format == VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
    return ProduceJpegBlobFromMjpegFrame(buffer, bytesused,
                                         capture_format.frame_size, rotation);
  }

  uint32_t src_format;
  switch (pixel_format) {
    case VideoPixelFormat::PIXEL_FORMAT_YUY2:
      src_format = libyuv::FOURCC_YUY2;
      break;
    case VideoPixelFormat::PIXEL_FORMAT_I420:
      src_format = libyuv::FOURCC_I420;
      break;
    case VideoPixelFormat::PIXEL_FORMAT_RGB24:
      src_format = libyuv::FOURCC_24BG;
      break;
    case VideoPixelFormat::PIXEL_FORMAT_ARGB:
      src_format = libyuv::FOURCC_ARGB;
      break;
    case VideoPixelFormat::PIXEL_FORMAT_NV12:
      src_format = libyuv::FOURCC_NV12;
      break;
    case VideoPixelFormat::PIXEL_FORMAT_UYVY:
      src_format = libyuv::FOURCC_UYVY;
      break;
    default:
      NOTREACHED() << "Unsupported pixel format passed to RotateAndBlobify";
  }

  const gfx::Size frame_size = capture_format.frame_size;
  // PNGCodec does not support YUV formats, convert to a temporary ARGB buffer.
  auto tmp_argb = std::make_unique<uint8_t[]>(
      VideoFrame::AllocationSize(PIXEL_FORMAT_ARGB, frame_size));
  if (ConvertToARGB(buffer, bytesused, tmp_argb.get(), frame_size.width() * 4,
                    0 /* crop_x_pos */, 0 /* crop_y_pos */, frame_size.width(),
                    frame_size.height(), frame_size.width(),
                    frame_size.height(), libyuv::RotationMode::kRotate0,
                    src_format) != 0) {
    return nullptr;
  }

  mojom::BlobPtr blob = mojom::Blob::New();
  const gfx::PNGCodec::ColorFormat codec_color_format =
#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
      gfx::PNGCodec::FORMAT_RGBA;
#else
      gfx::PNGCodec::FORMAT_BGRA;
#endif
  std::optional<std::vector<uint8_t>> data = gfx::PNGCodec::Encode(
      tmp_argb.get(), codec_color_format, frame_size, frame_size.width() * 4,
      /*discard_transparency=*/true, std::vector<gfx::PNGCodec::Comment>());

  blob->data = std::move(data.value());
  blob->mime_type = "image/png";
  return blob;
}

}  // namespace media
