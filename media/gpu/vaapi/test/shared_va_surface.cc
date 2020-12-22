// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include "base/files/file_util.h"
#include "media/base/video_types.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {
namespace vaapi_test {

namespace {

// Derives the VAImage metadata and image data from |surface_id| in |display|,
// returning true on success.
bool DeriveImage(VADisplay display,
                 VASurfaceID surface_id,
                 VAImage* image,
                 uint8_t** image_data) {
  VAStatus res = vaDeriveImage(display, surface_id, image);
  VLOG_IF(2, (res != VA_STATUS_SUCCESS))
      << "vaDeriveImage failed, VA error: " << vaErrorStr(res);

  // TODO(jchinlee): Support derivation into 10-bit fourcc.
  if (image->format.fourcc != VA_FOURCC_NV12) {
    VLOG(2) << "Test decoder binary does not support derived surface format "
            << "with fourcc " << media::FourccToString(image->format.fourcc);
    res = vaDestroyImage(display, image->image_id);
    VA_LOG_ASSERT(res, "vaDestroyImage");
    return false;
  }

  res = vaMapBuffer(display, image->buf, reinterpret_cast<void**>(image_data));
  VA_LOG_ASSERT(res, "vaMapBuffer");
  return true;
}

// Returns image format to use given the surface's internal VA format.
VAImageFormat GetImageFormat(unsigned int va_rt_format) {
  constexpr VAImageFormat kImageFormatNV12{.fourcc = VA_FOURCC_NV12,
                                           .byte_order = VA_LSB_FIRST,
                                           .bits_per_pixel = 12};
  constexpr VAImageFormat kImageFormatP010{.fourcc = VA_FOURCC_P010,
                                           .byte_order = VA_LSB_FIRST,
                                           .bits_per_pixel = 16};
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return kImageFormatNV12;
    case VA_RT_FORMAT_YUV420_10:
      return kImageFormatP010;
    default:
      LOG_ASSERT(false) << "Unknown VA format " << std::hex << va_rt_format;
      return VAImageFormat{};
  }
}

// Maps the image data from |surface_id| in |display| with given |size| by
// attempting to derive into |image| and |image_data|, or creating a
// VAImage to use with vaGetImage as fallback and setting |image| and
// |image_data| accordingly.
void GetSurfaceImage(VADisplay display,
                     VASurfaceID surface_id,
                     unsigned int va_rt_format,
                     const gfx::Size size,
                     VAImage* image,
                     uint8_t** image_data) {
  // First attempt to derive the image from the surface.
  if (DeriveImage(display, surface_id, image, image_data))
    return;

  // Fall back to getting the image with manually passed format.
  VAImageFormat format = GetImageFormat(va_rt_format);
  VAStatus res =
      vaCreateImage(display, &format, size.width(), size.height(), image);
  VA_LOG_ASSERT(res, "vaCreateImage");

  res = vaGetImage(display, surface_id, 0, 0, size.width(), size.height(),
                   image->image_id);
  VA_LOG_ASSERT(res, "vaGetImage");

  res = vaMapBuffer(display, image->buf, reinterpret_cast<void**>(image_data));
  VA_LOG_ASSERT(res, "vaMapBuffer");
}

uint16_t JoinUint8(uint8_t first, uint8_t second) {
  // P010 uses the 10 most significant bits; H010 the 10 least.
  const uint16_t joined = ((second << 8u) | first);
  return joined >> 6u;
}

}  // namespace

SharedVASurface::SharedVASurface(const VaapiDevice& va_device,
                                 VASurfaceID id,
                                 const gfx::Size& size,
                                 unsigned int format)
    : va_device_(va_device), id_(id), size_(size), va_rt_format_(format) {}

// static
scoped_refptr<SharedVASurface> SharedVASurface::Create(
    const VaapiDevice& va_device,
    unsigned int va_rt_format,
    const gfx::Size& size,
    VASurfaceAttrib attrib) {
  VASurfaceID surface_id;
  VAStatus res =
      vaCreateSurfaces(va_device.display(), va_rt_format,
                       base::checked_cast<unsigned int>(size.width()),
                       base::checked_cast<unsigned int>(size.height()),
                       &surface_id, 1u, &attrib, 1u);
  VA_LOG_ASSERT(res, "vaCreateSurfaces");
  VLOG(1) << "created surface: " << surface_id;
  return base::WrapRefCounted(
      new SharedVASurface(va_device, surface_id, size, va_rt_format));
}

SharedVASurface::~SharedVASurface() {
  VAStatus res = vaDestroySurfaces(va_device_.display(),
                                   const_cast<VASurfaceID*>(&id_), 1u);
  VA_LOG_ASSERT(res, "vaDestroySurfaces");
  VLOG(1) << "destroyed surface " << id_;
}

void SharedVASurface::SaveAsPNG(const std::string& path) {
  VAImage image;
  uint8_t* image_data;

  GetSurfaceImage(va_device_.display(), id_, va_rt_format_, size_, &image,
                  &image_data);

  // Convert the image data to ARGB and write to |path|.
  const size_t argb_stride = image.width * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * image.height);
  int convert_res = 0;
  const uint32_t fourcc = image.format.fourcc;
  DCHECK(fourcc == VA_FOURCC_NV12 || fourcc == VA_FOURCC_P010);

  if (fourcc == VA_FOURCC_NV12) {
    convert_res = libyuv::NV12ToARGB(image_data + image.offsets[0],
                                     base::checked_cast<int>(image.pitches[0]),
                                     image_data + image.offsets[1],
                                     base::checked_cast<int>(image.pitches[1]),
                                     argb_data.get(),
                                     base::checked_cast<int>(argb_stride),
                                     base::strict_cast<int>(image.width),
                                     base::strict_cast<int>(image.height));
  } else if (fourcc == VA_FOURCC_P010) {
    LOG_ASSERT(image.width * 2 <= image.pitches[0]);
    LOG_ASSERT(4 * ((image.width + 1) / 2) <= image.pitches[1]);

    uint8_t* y_8b = image_data + image.offsets[0];
    std::vector<uint16_t> y_plane;
    for (uint32_t row = 0u; row < image.height; row++) {
      for (uint32_t col = 0u; col < image.width * 2; col += 2) {
        y_plane.push_back(JoinUint8(y_8b[col], y_8b[col + 1]));
      }
      y_8b += image.pitches[0];
    }

    // Split the interleaved UV plane.
    uint8_t* uv_8b = image_data + image.offsets[1];
    std::vector<uint16_t> u_plane, v_plane;
    for (uint32_t row = 0u; row < (image.height + 1) / 2; row++) {
      for (uint32_t col = 0u; col < 4 * ((image.width + 1) / 2); col += 4) {
        u_plane.push_back(JoinUint8(uv_8b[col], uv_8b[col + 1]));
        v_plane.push_back(JoinUint8(uv_8b[col + 2], uv_8b[col + 3]));
      }
      uv_8b += image.pitches[1];
    }

    convert_res = libyuv::H010ToARGB(
        y_plane.data(), base::strict_cast<int>(image.width), u_plane.data(),
        base::checked_cast<int>((image.width + 1) / 2), v_plane.data(),
        base::checked_cast<int>((image.width + 1) / 2), argb_data.get(),
        base::checked_cast<int>(argb_stride),
        base::strict_cast<int>(image.width),
        base::strict_cast<int>(image.height));
  }
  LOG_ASSERT(convert_res == 0) << "Failed to convert to ARGB";

  std::vector<unsigned char> image_buffer;
  const bool result = gfx::PNGCodec::Encode(
      argb_data.get(), gfx::PNGCodec::FORMAT_BGRA, size_, argb_stride,
      true /* discard_transparency */, std::vector<gfx::PNGCodec::Comment>(),
      &image_buffer);
  LOG_ASSERT(result) << "Failed to encode to PNG";

  LOG_ASSERT(base::WriteFile(base::FilePath(path), image_buffer));

  // Clean up VA handles.
  VAStatus res = vaUnmapBuffer(va_device_.display(), image.buf);
  VA_LOG_ASSERT(res, "vaUnmapBuffer");

  res = vaDestroyImage(va_device_.display(), image.image_id);
  VA_LOG_ASSERT(res, "vaDestroyImage");
}

}  // namespace vaapi_test
}  // namespace media
