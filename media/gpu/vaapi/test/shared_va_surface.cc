// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/shared_va_surface.h"

#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "media/base/video_types.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {
namespace vaapi_test {

namespace {

// Returns whether the image format |fourcc| is supported for MD5 hash checking.
// MD5 golden values are computed from vpxdec based on I420, and only certain
// format conversions are implemented.
bool IsSupportedFormat(uint32_t fourcc) {
  return fourcc == VA_FOURCC_I420 || fourcc == VA_FOURCC_NV12;
}

// Derives the VAImage metadata and image data from |surface_id| in |display|,
// returning true on success.
bool DeriveImage(VADisplay display,
                 VASurfaceID surface_id,
                 VAImage* image,
                 uint8_t** image_data) {
  VAStatus res = vaDeriveImage(display, surface_id, image);
  if (res != VA_STATUS_SUCCESS) {
    VLOG(2) << "vaDeriveImage failed, VA error: " << vaErrorStr(res);
    return false;
  }

  const uint32_t fourcc = image->format.fourcc;
  DCHECK_NE(fourcc, 0u);

  // TODO(jchinlee): Support derivation into 10-bit fourcc.
  if (!IsSupportedFormat(fourcc)) {
    VLOG(2) << "Test decoder binary does not support derived surface format "
            << "with fourcc " << media::FourccToString(fourcc);
    VA_LOG_ASSERT(vaDestroyImage(display, image->image_id), "vaDestroyImage");
    return false;
  }

  res = vaMapBuffer(display, image->buf, reinterpret_cast<void**>(image_data));
  VA_LOG_ASSERT(res, "vaMapBuffer");
  return true;
}

// Returns image format to fall back to given the surface's internal VA format.
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
      LOG(FATAL) << "Unknown VA format " << std::hex << va_rt_format;
  }
}

// Retrieves the image data from |surface_id| in |display| with given |size| by
// creating a VAImage with |format| to use with vaGetImage and setting |image|
// and |image_data| accordingly.
void GetSurfaceImage(VADisplay display,
                     VASurfaceID surface_id,
                     VAImageFormat format,
                     const gfx::Size size,
                     VAImage* image,
                     uint8_t** image_data) {
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

SharedVASurface::SharedVASurface(base::PassKey<SharedVASurface>,
                                 const VaapiDevice& va_device,
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
  return base::MakeRefCounted<SharedVASurface>(base::PassKey<SharedVASurface>(),
                                               va_device, surface_id, size,
                                               va_rt_format);
}

SharedVASurface::~SharedVASurface() {
  VAStatus res = vaDestroySurfaces(va_device_->display(),
                                   const_cast<VASurfaceID*>(&id_), 1u);
  VA_LOG_ASSERT(res, "vaDestroySurfaces");
  VLOG(1) << "destroyed surface " << id_;
}

void SharedVASurface::FetchData(FetchPolicy fetch_policy,
                                const VAImageFormat& format,
                                VAImage* image,
                                uint8_t** image_data) const {
  if (fetch_policy == FetchPolicy::kDeriveImage ||
      fetch_policy == FetchPolicy::kAny) {
    const bool res = DeriveImage(va_device_->display(), id_, image, image_data);
    if (fetch_policy != FetchPolicy::kAny)
      LOG_ASSERT(res) << "Failed to vaDeriveImage.";
    if (res)
      return;
  }

  GetSurfaceImage(va_device_->display(), id_, format, size_, image, image_data);
}

void SharedVASurface::SaveAsPNG(FetchPolicy fetch_policy,
                                const std::string& path) const {
  VAImage image;
  uint8_t* image_data;
  FetchData(fetch_policy, GetImageFormat(va_rt_format_), &image, &image_data);

  // Convert the image data to ARGB and write to |path|.
  const size_t argb_stride = image.width * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * image.height);
  int convert_res = -1;
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
  VAStatus res = vaUnmapBuffer(va_device_->display(), image.buf);
  VA_LOG_ASSERT(res, "vaUnmapBuffer");

  res = vaDestroyImage(va_device_->display(), image.image_id);
  VA_LOG_ASSERT(res, "vaDestroyImage");
}

std::string SharedVASurface::GetMD5Sum(FetchPolicy fetch_policy) const {
  VAImage image;
  uint8_t* image_data;
  constexpr VAImageFormat kImageFormatI420{.fourcc = VA_FOURCC_I420,
                                           .byte_order = VA_LSB_FIRST,
                                           .bits_per_pixel = 12};
  FetchData(fetch_policy, kImageFormatI420, &image, &image_data);

  // Golden values of MD5 sums are computed from vpxdec with packed I420 as the
  // format, so convert as needed.
  uint32_t luma_plane_size =
      base::checked_cast<uint32_t>(image.height * image.width);
  uint32_t chroma_plane_size = base::checked_cast<uint32_t>(
      ((image.height + 1) / 2) * ((image.width + 1) / 2));
  std::vector<uint8_t> i420_data(luma_plane_size + 2 * chroma_plane_size, 0u);
  int convert_res = -1;
  const uint32_t fourcc = image.format.fourcc;
  if (fourcc == VA_FOURCC_I420) {
    // I420 still needs to be packed.
    LOG_ASSERT(image.num_planes == 3u);
    convert_res = libyuv::I420Copy(
        image_data + image.offsets[0],
        base::checked_cast<int>(image.pitches[0]),
        image_data + image.offsets[1],
        base::checked_cast<int>(image.pitches[1]),
        image_data + image.offsets[2],
        base::checked_cast<int>(image.pitches[2]), i420_data.data(),
        base::strict_cast<int>(image.width), i420_data.data() + luma_plane_size,
        base::checked_cast<int>((image.width + 1) / 2),
        i420_data.data() + luma_plane_size + chroma_plane_size,
        base::checked_cast<int>((image.width + 1) / 2),
        base::strict_cast<int>(image.width),
        base::strict_cast<int>(image.height));
  } else if (fourcc == VA_FOURCC_NV12) {
    LOG_ASSERT(image.num_planes == 2u);
    convert_res = libyuv::NV12ToI420(
        image_data + image.offsets[0],
        base::checked_cast<int>(image.pitches[0]),
        image_data + image.offsets[1],
        base::checked_cast<int>(image.pitches[1]), i420_data.data(),
        base::strict_cast<int>(image.width), i420_data.data() + luma_plane_size,
        base::checked_cast<int>((image.width + 1) / 2),
        i420_data.data() + luma_plane_size + chroma_plane_size,
        base::checked_cast<int>((image.width + 1) / 2),
        base::strict_cast<int>(image.width),
        base::strict_cast<int>(image.height));
  }
  LOG_ASSERT(convert_res == 0)
      << "Failed to convert " << media::FourccToString(fourcc)
      << " to packed I420.";

  // Clean up VA handles.
  VAStatus res = vaUnmapBuffer(va_device_->display(), image.buf);
  VA_LOG_ASSERT(res, "vaUnmapBuffer");

  res = vaDestroyImage(va_device_->display(), image.image_id);
  VA_LOG_ASSERT(res, "vaDestroyImage");

  base::MD5Digest md5_digest;
  base::MD5Sum(i420_data, &md5_digest);
  return MD5DigestToBase16(md5_digest);
}

}  // namespace vaapi_test
}  // namespace media
