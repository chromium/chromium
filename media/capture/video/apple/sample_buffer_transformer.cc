// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/sample_buffer_transformer.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace media {

namespace {

// NV12 a.k.a. 420v
constexpr OSType kPixelFormatNv12 =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
// I420 a.k.a. y420
constexpr OSType kPixelFormatI420 = kCVPixelFormatType_420YpCbCr8Planar;
// MJPEG a.k.a. dmb1
constexpr OSType kPixelFormatMjpeg = kCMVideoCodecType_JPEG_OpenDML;

std::pair<uint8_t*, size_t> GetSampleBufferBaseAddressAndSize(
    CMSampleBufferRef sample_buffer) {
  // Access source sample buffer bytes.
  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sample_buffer);
  DCHECK(block_buffer);
  char* data_base_address;
  size_t data_size;
  size_t length_at_offset;
  OSStatus status = CMBlockBufferGetDataPointer(
      block_buffer, 0, &length_at_offset, &data_size, &data_base_address);
  DCHECK_EQ(status, noErr);
  DCHECK(data_base_address);
  DCHECK(data_size);
  DCHECK_EQ(length_at_offset, data_size);  // Buffer must be contiguous.
  return std::make_pair(reinterpret_cast<uint8_t*>(data_base_address),
                        data_size);
}

struct I420Planes {
  size_t width;
  size_t height;
  raw_ptr<uint8_t, AllowPtrArithmetic> y_plane_data;
  raw_ptr<uint8_t, AllowPtrArithmetic> u_plane_data;
  raw_ptr<uint8_t, AllowPtrArithmetic> v_plane_data;
  size_t y_plane_stride;
  size_t u_plane_stride;
  size_t v_plane_stride;
};

size_t GetContiguousI420BufferSize(size_t width, size_t height) {
  gfx::Size dimensions(width, height);
  return VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kY,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kU,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kV,
                               dimensions)
             .GetArea();
}

I420Planes GetI420PlanesFromContiguousBuffer(uint8_t* data_base_address,
                                             size_t width,
                                             size_t height) {
  gfx::Size dimensions(width, height);
  gfx::Size y_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::Plane::kY, dimensions);
  gfx::Size u_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::Plane::kU, dimensions);
  gfx::Size v_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::Plane::kU, dimensions);
  I420Planes i420_planes;
  i420_planes.width = width;
  i420_planes.height = height;
  i420_planes.y_plane_data = data_base_address;
  i420_planes.u_plane_data = i420_planes.y_plane_data + y_plane_size.GetArea();
  i420_planes.v_plane_data = i420_planes.u_plane_data + u_plane_size.GetArea();
  i420_planes.y_plane_stride = y_plane_size.width();
  i420_planes.u_plane_stride = u_plane_size.width();
  i420_planes.v_plane_stride = v_plane_size.width();
  return i420_planes;
}

I420Planes EnsureI420BufferSizeAndGetPlanes(size_t width,
                                            size_t height,
                                            std::vector<uint8_t>* i420_buffer) {
  size_t required_size = GetContiguousI420BufferSize(width, height);
  if (i420_buffer->size() < required_size) {
    i420_buffer->resize(required_size);
  }
  return GetI420PlanesFromContiguousBuffer(&(*i420_buffer)[0], width, height);
}

I420Planes GetI420PlanesFromPixelBuffer(CVPixelBufferRef pixel_buffer) {
  DCHECK_EQ(CVPixelBufferGetPlaneCount(pixel_buffer), 3u);
  I420Planes i420_planes;
  i420_planes.width = CVPixelBufferGetWidth(pixel_buffer);
  i420_planes.height = CVPixelBufferGetHeight(pixel_buffer);
  i420_planes.y_plane_data = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  i420_planes.u_plane_data = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  i420_planes.v_plane_data = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 2));
  i420_planes.y_plane_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  i420_planes.u_plane_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
  i420_planes.v_plane_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 2);
  return i420_planes;
}

struct NV12Planes {
  size_t width;
  size_t height;
  raw_ptr<uint8_t, AllowPtrArithmetic> y_plane_data;
  raw_ptr<uint8_t, AllowPtrArithmetic> uv_plane_data;
  size_t y_plane_stride;
  size_t uv_plane_stride;
};

// TODO(eshr): Move this to libyuv.
void CopyNV12(const uint8_t* src_y,
              int src_y_stride,
              const uint8_t* src_uv,
              int src_uv_stride,
              uint8_t* dst_y,
              int dst_y_stride,
              uint8_t* dst_uv,
              int dst_uv_stride,
              int width,
              int height) {
  libyuv::CopyPlane(src_y, src_y_stride, dst_y, dst_y_stride, width, height);
  size_t half_width = (width + 1) >> 1;
  size_t half_height = (height + 1) >> 1;
  libyuv::CopyPlane(src_uv, src_uv_stride, dst_uv, dst_uv_stride,
                    half_width * 2, half_height);
}

size_t GetContiguousNV12BufferSize(size_t width, size_t height) {
  gfx::Size dimensions(width, height);
  return VideoFrame::PlaneSize(PIXEL_FORMAT_NV12, VideoFrame::Plane::kY,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV,
                               dimensions)
             .GetArea();
}

NV12Planes GetNV12PlanesFromContiguousBuffer(uint8_t* data_base_address,
                                             size_t width,
                                             size_t height) {
  gfx::Size dimensions(width, height);
  gfx::Size y_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kY, dimensions);
  gfx::Size uv_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV, dimensions);
  NV12Planes nv12_planes;
  nv12_planes.width = width;
  nv12_planes.height = height;
  nv12_planes.y_plane_data = data_base_address;
  nv12_planes.uv_plane_data = nv12_planes.y_plane_data + y_plane_size.GetArea();
  nv12_planes.y_plane_stride = y_plane_size.width();
  nv12_planes.uv_plane_stride = uv_plane_size.width();
  return nv12_planes;
}

NV12Planes EnsureNV12BufferSizeAndGetPlanes(size_t width,
                                            size_t height,
                                            std::vector<uint8_t>* nv12_buffer) {
  size_t required_size = GetContiguousNV12BufferSize(width, height);
  if (nv12_buffer->size() < required_size) {
    nv12_buffer->resize(required_size);
  }
  return GetNV12PlanesFromContiguousBuffer(&(*nv12_buffer)[0], width, height);
}

NV12Planes GetNV12PlanesFromPixelBuffer(CVPixelBufferRef pixel_buffer) {
  DCHECK_EQ(CVPixelBufferGetPlaneCount(pixel_buffer), 2u);
  NV12Planes nv12_planes;
  nv12_planes.width = CVPixelBufferGetWidth(pixel_buffer);
  nv12_planes.height = CVPixelBufferGetHeight(pixel_buffer);
  nv12_planes.y_plane_data = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  nv12_planes.uv_plane_data = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  nv12_planes.y_plane_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  nv12_planes.uv_plane_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
  return nv12_planes;
}

bool ConvertFromMjpegToI420(uint8_t* source_buffer_base_address,
                            size_t source_buffer_size,
                            const I420Planes& destination) {
  int result = libyuv::MJPGToI420(
      source_buffer_base_address, source_buffer_size, destination.y_plane_data,
      destination.y_plane_stride, destination.u_plane_data,
      destination.u_plane_stride, destination.v_plane_data,
      destination.v_plane_stride, destination.width, destination.height,
      destination.width, destination.height);
  return result == 0;
}

void ConvertFromAnyToNV12(CVPixelBufferRef source_pixel_buffer,
                          const NV12Planes& destination) {
  auto pixel_format = CVPixelBufferGetPixelFormatType(source_pixel_buffer);
  int ret;
  switch (pixel_format) {
    // UYVY a.k.a. 2vuy
    case kCVPixelFormatType_422YpCbCr8: {
      const uint8_t* src_uyvy = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_uyvy = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      ret = libyuv::UYVYToNV12(
          src_uyvy, src_stride_uyvy, destination.y_plane_data,
          destination.y_plane_stride, destination.uv_plane_data,
          destination.uv_plane_stride, destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    // YUY2 a.k.a. yuvs
    case kCMPixelFormat_422YpCbCr8_yuvs: {
      const uint8_t* src_yuy2 = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_yuy2 = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      ret = libyuv::YUY2ToNV12(
          src_yuy2, src_stride_yuy2, destination.y_plane_data,
          destination.y_plane_stride, destination.uv_plane_data,
          destination.uv_plane_stride, destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    // NV12 a.k.a. 420v
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
      DCHECK(CVPixelBufferIsPlanar(source_pixel_buffer));
      DCHECK_EQ(2u, CVPixelBufferGetPlaneCount(source_pixel_buffer));
      const uint8_t* src_y = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 0));
      size_t src_stride_y =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 0);
      const uint8_t* src_uv = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 1));
      size_t src_stride_uv =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 1);
      CopyNV12(src_y, src_stride_y, src_uv, src_stride_uv,
               destination.y_plane_data, destination.y_plane_stride,
               destination.uv_plane_data, destination.uv_plane_stride,
               destination.width, destination.height);
      return;
    }
    // I420 a.k.a. y420
    case kCVPixelFormatType_420YpCbCr8Planar: {
      DCHECK(CVPixelBufferIsPlanar(source_pixel_buffer));
      DCHECK_EQ(3u, CVPixelBufferGetPlaneCount(source_pixel_buffer));
      const uint8_t* src_y = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 0));
      size_t src_stride_y =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 0);
      const uint8_t* src_u = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 1));
      size_t src_stride_u =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 1);
      const uint8_t* src_v = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 2));
      size_t src_stride_v =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 2);
      ret = libyuv::I420ToNV12(
          src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
          destination.y_plane_data, destination.y_plane_stride,
          destination.uv_plane_data, destination.uv_plane_stride,
          destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    default:
      NOTREACHED_IN_MIGRATION()
          << "Pixel format " << pixel_format << " not supported.";
  }
}

void ConvertFromAnyToI420(CVPixelBufferRef source_pixel_buffer,
                          const I420Planes& destination) {
  auto pixel_format = CVPixelBufferGetPixelFormatType(source_pixel_buffer);
  int ret;
  switch (pixel_format) {
    // UYVY a.k.a. 2vuy
    case kCVPixelFormatType_422YpCbCr8: {
      const uint8_t* src_uyvy = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_uyvy = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      ret = libyuv::UYVYToI420(
          src_uyvy, src_stride_uyvy, destination.y_plane_data,
          destination.y_plane_stride, destination.u_plane_data,
          destination.u_plane_stride, destination.v_plane_data,
          destination.v_plane_stride, destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    // YUY2 a.k.a. yuvs
    case kCMPixelFormat_422YpCbCr8_yuvs: {
      const uint8_t* src_yuy2 = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_yuy2 = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      ret = libyuv::YUY2ToI420(
          src_yuy2, src_stride_yuy2, destination.y_plane_data,
          destination.y_plane_stride, destination.u_plane_data,
          destination.u_plane_stride, destination.v_plane_data,
          destination.v_plane_stride, destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    // NV12 a.k.a. 420v
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
      DCHECK(CVPixelBufferIsPlanar(source_pixel_buffer));
      DCHECK_EQ(2u, CVPixelBufferGetPlaneCount(source_pixel_buffer));
      const uint8_t* src_y = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 0));
      size_t src_stride_y =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 0);
      const uint8_t* src_uv = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 1));
      size_t src_stride_uv =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 1);
      ret = libyuv::NV12ToI420(
          src_y, src_stride_y, src_uv, src_stride_uv, destination.y_plane_data,
          destination.y_plane_stride, destination.u_plane_data,
          destination.u_plane_stride, destination.v_plane_data,
          destination.v_plane_stride, destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    // I420 a.k.a. y420
    case kCVPixelFormatType_420YpCbCr8Planar: {
      DCHECK(CVPixelBufferIsPlanar(source_pixel_buffer));
      DCHECK_EQ(3u, CVPixelBufferGetPlaneCount(source_pixel_buffer));
      const uint8_t* src_y = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 0));
      size_t src_stride_y =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 0);
      const uint8_t* src_u = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 1));
      size_t src_stride_u =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 1);
      const uint8_t* src_v = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(source_pixel_buffer, 2));
      size_t src_stride_v =
          CVPixelBufferGetBytesPerRowOfPlane(source_pixel_buffer, 2);
      ret = libyuv::I420Copy(
          src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
          destination.y_plane_data, destination.y_plane_stride,
          destination.u_plane_data, destination.u_plane_stride,
          destination.v_plane_data, destination.v_plane_stride,
          destination.width, destination.height);
      DCHECK_EQ(ret, 0);
      return;
    }
    default:
      NOTREACHED_IN_MIGRATION()
          << "Pixel format " << pixel_format << " not supported.";
  }
}

// Returns true on success. MJPEG frames produces by some webcams have been
// observed to be invalid in special circumstances (see
// https://crbug.com/1147867). To support a graceful failure path in this case,
// this function may return false.
bool ConvertFromMjpegToNV12(uint8_t* source_buffer_data_base_address,
                            size_t source_buffer_data_size,
                            const NV12Planes& destination) {
  // Despite libyuv::MJPGToNV12() taking both source and destination sizes as
  // arguments, this function is only successful if the sizes match. So here we
  // require the destination buffer's size to match the source's.
  int result = libyuv::MJPGToNV12(
      source_buffer_data_base_address, source_buffer_data_size,
      destination.y_plane_data, destination.y_plane_stride,
      destination.uv_plane_data, destination.uv_plane_stride, destination.width,
      destination.height, destination.width, destination.height);
  return result == 0;
}

void ScaleI420(const I420Planes& source, const I420Planes& destination) {
  int result = libyuv::I420Scale(
      source.y_plane_data, source.y_plane_stride, source.u_plane_data,
      source.u_plane_stride, source.v_plane_data, source.v_plane_stride,
      source.width, source.height, destination.y_plane_data,
      destination.y_plane_stride, destination.u_plane_data,
      destination.u_plane_stride, destination.v_plane_data,
      destination.v_plane_stride, destination.width, destination.height,
      libyuv::kFilterBilinear);
  DCHECK_EQ(result, 0);
}

void CopyI420(const I420Planes& source, const I420Planes& destination) {
  DCHECK_EQ(source.width, destination.width);
  DCHECK_EQ(source.height, destination.height);
  libyuv::I420Copy(source.y_plane_data, source.y_plane_stride,
                   source.u_plane_data, source.u_plane_stride,
                   source.v_plane_data, source.v_plane_stride,
                   destination.y_plane_data, destination.y_plane_stride,
                   destination.u_plane_data, destination.u_plane_stride,
                   destination.v_plane_data, destination.v_plane_stride,
                   source.width, source.height);
}

void ScaleNV12(const NV12Planes& source, const NV12Planes& destination) {
  int result = libyuv::NV12Scale(
      source.y_plane_data, source.y_plane_stride, source.uv_plane_data,
      source.uv_plane_stride, source.width, source.height,
      destination.y_plane_data, destination.y_plane_stride,
      destination.uv_plane_data, destination.uv_plane_stride, destination.width,
      destination.height, libyuv::kFilterBilinear);
  DCHECK_EQ(result, 0);
}

void CopyNV12(const NV12Planes& source, const NV12Planes& destination) {
  DCHECK_EQ(source.width, destination.width);
  DCHECK_EQ(source.height, destination.height);
  CopyNV12(source.y_plane_data, source.y_plane_stride, source.uv_plane_data,
           source.uv_plane_stride, destination.y_plane_data,
           destination.y_plane_stride, destination.uv_plane_data,
           destination.uv_plane_stride, source.width, source.height);
}

}  // namespace

// static
const SampleBufferTransformer::Transformer
    SampleBufferTransformer::kBestTransformerForPixelBufferToNv12Output =
        SampleBufferTransformer::Transformer::kPixelBufferTransfer;

// static
SampleBufferTransformer::Transformer
SampleBufferTransformer::GetBestTransformerForNv12Output(
    CMSampleBufferRef sample_buffer) {
  if (CMSampleBufferGetImageBuffer(sample_buffer)) {
    return kBestTransformerForPixelBufferToNv12Output;
  }
  // When we don't have a pixel buffer (e.g. it's MJPEG or we get a SW-backed
  // byte buffer) only libyuv is able to perform the transform.
  return Transformer::kLibyuv;
}

// static
std::unique_ptr<SampleBufferTransformer> SampleBufferTransformer::Create() {
  return std::make_unique<SampleBufferTransformer>();
}

SampleBufferTransformer::SampleBufferTransformer()
    : transformer_(Transformer::kNotConfigured),
      destination_pixel_format_(0x0) {}

SampleBufferTransformer::~SampleBufferTransformer() {}

SampleBufferTransformer::Transformer SampleBufferTransformer::transformer()
    const {
  return transformer_;
}

OSType SampleBufferTransformer::destination_pixel_format() const {
  return destination_pixel_format_;
}

const gfx::Size& SampleBufferTransformer::destination_size() const {
  return destination_size_;
}

void SampleBufferTransformer::Reconfigure(
    Transformer transformer,
    OSType destination_pixel_format,
    const gfx::Size& destination_size,
    int rotation_angle,
    std::optional<size_t> buffer_pool_size) {
  DCHECK(transformer != Transformer::kLibyuv ||
         destination_pixel_format == kPixelFormatI420 ||
         destination_pixel_format == kPixelFormatNv12)
      << "Destination format is unsupported when running libyuv";
  if (transformer_ == transformer &&
      destination_pixel_format_ == destination_pixel_format &&
      destination_size_ == destination_size) {
    // Already configured as desired, abort.
    return;
  }
  transformer_ = transformer;
  destination_pixel_format_ = destination_pixel_format;
  destination_size_ = destination_size;
  destination_pixel_buffer_pool_ = PixelBufferPool::Create(
      destination_pixel_format_, destination_size_.width(),
      destination_size_.height(), buffer_pool_size);
  if (transformer == Transformer::kPixelBufferTransfer) {
    pixel_buffer_transferer_ = std::make_unique<PixelBufferTransferer>();
    rotation_angle_ = rotation_angle;
#if BUILDFLAG(IS_IOS)
    int width, height;
    switch (rotation_angle_) {
      case 0:
      case 180:
        width = destination_size_.width();
        height = destination_size_.height();
        break;
      case 90:
      case 270:
        width = destination_size_.height();
        height = destination_size_.width();
        break;
    }

    rotated_destination_pixel_buffer_pool_ = PixelBufferPool::Create(
        destination_pixel_format_, width, height, buffer_pool_size);
    pixel_buffer_rotator_ = std::make_unique<PixelBufferRotator>();
#endif
  } else {
#if BUILDFLAG(IS_IOS)
    pixel_buffer_rotator_.reset();
#endif
    pixel_buffer_transferer_.reset();
  }
  intermediate_i420_buffer_.resize(0);
  intermediate_nv12_buffer_.resize(0);
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef>
SampleBufferTransformer::Transform(CVPixelBufferRef pixel_buffer) {
  DCHECK(transformer_ != Transformer::kNotConfigured);
  DCHECK(pixel_buffer);
  // Fast path: If source and destination formats are identical, return the
  // source pixel buffer.
  if (pixel_buffer &&
      static_cast<size_t>(destination_size_.width()) ==
          CVPixelBufferGetWidth(pixel_buffer) &&
      static_cast<size_t>(destination_size_.height()) ==
          CVPixelBufferGetHeight(pixel_buffer) &&
      destination_pixel_format_ ==
          CVPixelBufferGetPixelFormatType(pixel_buffer) &&
      CVPixelBufferGetIOSurface(pixel_buffer)) {
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>(
        pixel_buffer, base::scoped_policy::RETAIN);
  }
  // Create destination buffer from pool.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination_pixel_buffer =
      destination_pixel_buffer_pool_->CreateBuffer();
  if (!destination_pixel_buffer) {
    // Most likely the buffer count was exceeded, but other errors are possible.
    LOG(ERROR) << "Failed to create a destination buffer";
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  // Do pixel transfer or libyuv conversion + rescale.
  TransformPixelBuffer(pixel_buffer, destination_pixel_buffer.get());
  return destination_pixel_buffer;
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef>
SampleBufferTransformer::Transform(CMSampleBufferRef sample_buffer) {
  DCHECK(transformer_ != Transformer::kNotConfigured);
  DCHECK(sample_buffer);
  // If the sample buffer has a pixel buffer, run the pixel buffer path instead.
  if (CVPixelBufferRef pixel_buffer =
          CMSampleBufferGetImageBuffer(sample_buffer)) {
    return Transform(pixel_buffer);
  }
  // Create destination buffer from pool.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination_pixel_buffer =
      destination_pixel_buffer_pool_->CreateBuffer();
  if (!destination_pixel_buffer) {
    // Most likely the buffer count was exceeded, but other errors are possible.
    LOG(ERROR) << "Failed to create a destination buffer";
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  // Sample buffer path - it's MJPEG. Do libyuv conversion + rescale.
  if (!TransformSampleBuffer(sample_buffer, destination_pixel_buffer.get())) {
    LOG(ERROR) << "Failed to transform sample buffer.";
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  return destination_pixel_buffer;
}

#if BUILDFLAG(IS_IOS)
base::apple::ScopedCFTypeRef<CVPixelBufferRef> SampleBufferTransformer::Rotate(
    CVPixelBufferRef source_pixel_buffer) {
  DCHECK(source_pixel_buffer);
  DCHECK(pixel_buffer_rotator_);

  // Create destination buffer from pool.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> rotated_pixel_buffer =
      rotated_destination_pixel_buffer_pool_->CreateBuffer();
  if (!rotated_pixel_buffer) {
    // Most likely the buffer count was exceeded, but other errors are possible.
    LOG(ERROR) << "Failed to create a destination buffer";
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }

  // The rotated_pixel_buffer might not be the same size as source_pixel_buffer
  // since source_pixel_buffer gets rotated by rotation_angle_.
  if (pixel_buffer_rotator_->Rotate(
          source_pixel_buffer, rotated_pixel_buffer.get(), rotation_angle_)) {
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>(rotated_pixel_buffer);
  } else {
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
}
#endif

void SampleBufferTransformer::TransformPixelBuffer(
    CVPixelBufferRef source_pixel_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  switch (transformer_) {
    case Transformer::kPixelBufferTransfer:
      return TransformPixelBufferWithPixelTransfer(source_pixel_buffer,
                                                   destination_pixel_buffer);
    case Transformer::kLibyuv:
      return TransformPixelBufferWithLibyuv(source_pixel_buffer,
                                            destination_pixel_buffer);
    case Transformer::kNotConfigured:
      NOTREACHED_IN_MIGRATION();
  }
}

void SampleBufferTransformer::TransformPixelBufferWithPixelTransfer(
    CVPixelBufferRef source_pixel_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  DCHECK(transformer_ == Transformer::kPixelBufferTransfer);
  DCHECK(pixel_buffer_transferer_);
  bool success = pixel_buffer_transferer_->TransferImage(
      source_pixel_buffer, destination_pixel_buffer);
  DCHECK(success);
}

void SampleBufferTransformer::TransformPixelBufferWithLibyuv(
    CVPixelBufferRef source_pixel_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  DCHECK(transformer_ == Transformer::kLibyuv);
  // Lock source and destination pixel buffers.
  CVReturn lock_status = CVPixelBufferLockBaseAddress(
      source_pixel_buffer, kCVPixelBufferLock_ReadOnly);
  DCHECK_EQ(lock_status, kCVReturnSuccess);
  lock_status = CVPixelBufferLockBaseAddress(destination_pixel_buffer, 0);
  DCHECK_EQ(lock_status, kCVReturnSuccess);

  // Perform transform with libyuv.
  switch (destination_pixel_format_) {
    case kPixelFormatI420:
      TransformPixelBufferWithLibyuvFromAnyToI420(source_pixel_buffer,
                                                  destination_pixel_buffer);
      break;
    case kPixelFormatNv12:
      TransformPixelBufferWithLibyuvFromAnyToNV12(source_pixel_buffer,
                                                  destination_pixel_buffer);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Unlock source and destination pixel buffers.
  lock_status = CVPixelBufferUnlockBaseAddress(destination_pixel_buffer, 0);
  DCHECK_EQ(lock_status, kCVReturnSuccess);
  lock_status = CVPixelBufferUnlockBaseAddress(source_pixel_buffer,
                                               kCVPixelBufferLock_ReadOnly);
  DCHECK_EQ(lock_status, kCVReturnSuccess);
}

void SampleBufferTransformer::TransformPixelBufferWithLibyuvFromAnyToI420(
    CVPixelBufferRef source_pixel_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  // Get source pixel format and bytes.
  size_t source_width = CVPixelBufferGetWidth(source_pixel_buffer);
  size_t source_height = CVPixelBufferGetHeight(source_pixel_buffer);
  OSType source_pixel_format =
      CVPixelBufferGetPixelFormatType(source_pixel_buffer);

  // Rescaling has to be done in a separate step.
  const bool rescale_needed =
      static_cast<size_t>(destination_size_.width()) != source_width ||
      static_cast<size_t>(destination_size_.height()) != source_height;

  // Step 1: Convert to I420.
  I420Planes i420_fullscale_buffer;
  if (source_pixel_format == kPixelFormatI420) {
    // We are already at I420.
    i420_fullscale_buffer = GetI420PlanesFromPixelBuffer(source_pixel_buffer);
    // Fast path should have been taken if no resize needed and the buffer is on
    // an IOSurface already.
    DCHECK(rescale_needed || !CVPixelBufferGetIOSurface(source_pixel_buffer));
    if (!rescale_needed) {
      I420Planes i420_destination_buffer =
          GetI420PlanesFromPixelBuffer(destination_pixel_buffer);
      CopyI420(i420_fullscale_buffer, i420_destination_buffer);
      return;
    }
  } else {
    // Convert X -> I420.
    if (!rescale_needed) {
      i420_fullscale_buffer =
          GetI420PlanesFromPixelBuffer(destination_pixel_buffer);
    } else {
      i420_fullscale_buffer = EnsureI420BufferSizeAndGetPlanes(
          source_width, source_height, &intermediate_i420_buffer_);
    }
    ConvertFromAnyToI420(source_pixel_buffer, i420_fullscale_buffer);
  }

  // Step 2: Rescale I420.
  if (rescale_needed) {
    I420Planes i420_destination_buffer =
        GetI420PlanesFromPixelBuffer(destination_pixel_buffer);
    ScaleI420(i420_fullscale_buffer, i420_destination_buffer);
  }
}

void SampleBufferTransformer::TransformPixelBufferWithLibyuvFromAnyToNV12(
    CVPixelBufferRef source_pixel_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  // Get source pixel format and bytes.
  size_t source_width = CVPixelBufferGetWidth(source_pixel_buffer);
  size_t source_height = CVPixelBufferGetHeight(source_pixel_buffer);
  OSType source_pixel_format =
      CVPixelBufferGetPixelFormatType(source_pixel_buffer);

  // Rescaling has to be done in a separate step.
  const bool rescale_needed =
      static_cast<size_t>(destination_size_.width()) != source_width ||
      static_cast<size_t>(destination_size_.height()) != source_height;

  // Step 1: Convert to NV12.
  NV12Planes nv12_fullscale_buffer;
  if (source_pixel_format == kPixelFormatNv12) {
    // We are already at NV12.
    nv12_fullscale_buffer = GetNV12PlanesFromPixelBuffer(source_pixel_buffer);
    // Fast path should have been taken if no resize needed and the buffer is on
    // an IOSurface already.
    DCHECK(rescale_needed || !CVPixelBufferGetIOSurface(source_pixel_buffer));
    if (!rescale_needed) {
      NV12Planes nv12_destination_buffer =
          GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
      CopyNV12(nv12_fullscale_buffer, nv12_destination_buffer);
      return;
    }
  } else {
    if (!rescale_needed) {
      nv12_fullscale_buffer =
          GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
    } else {
      nv12_fullscale_buffer = EnsureNV12BufferSizeAndGetPlanes(
          source_width, source_height, &intermediate_nv12_buffer_);
    }
    ConvertFromAnyToNV12(source_pixel_buffer, nv12_fullscale_buffer);
  }

  // Step 2: Rescale NV12.
  if (rescale_needed) {
    NV12Planes nv12_destination_buffer =
        GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
    ScaleNV12(nv12_fullscale_buffer, nv12_destination_buffer);
  }
}

bool SampleBufferTransformer::TransformSampleBuffer(
    CMSampleBufferRef source_sample_buffer,
    CVPixelBufferRef destination_pixel_buffer) {
  DCHECK(transformer_ == Transformer::kLibyuv);
  // Ensure source pixel format is MJPEG and get width and height.
  CMFormatDescriptionRef source_format_description =
      CMSampleBufferGetFormatDescription(source_sample_buffer);
  FourCharCode source_pixel_format =
      CMFormatDescriptionGetMediaSubType(source_format_description);
  CHECK_EQ(source_pixel_format, kPixelFormatMjpeg);
  CMVideoDimensions source_dimensions =
      CMVideoFormatDescriptionGetDimensions(source_format_description);

  // Access source pixel buffer bytes.
  uint8_t* source_buffer_data_base_address;
  size_t source_buffer_data_size;
  std::tie(source_buffer_data_base_address, source_buffer_data_size) =
      GetSampleBufferBaseAddressAndSize(source_sample_buffer);

  // Lock destination pixel buffer.
  CVReturn lock_status =
      CVPixelBufferLockBaseAddress(destination_pixel_buffer, 0);
  DCHECK_EQ(lock_status, kCVReturnSuccess);
  // Convert to I420 or NV12.
  bool success = false;
  switch (destination_pixel_format_) {
    case kPixelFormatI420:
      success = TransformSampleBufferFromMjpegToI420(
          source_buffer_data_base_address, source_buffer_data_size,
          source_dimensions.width, source_dimensions.height,
          destination_pixel_buffer);
      break;
    case kPixelFormatNv12:
      success = TransformSampleBufferFromMjpegToNV12(
          source_buffer_data_base_address, source_buffer_data_size,
          source_dimensions.width, source_dimensions.height,
          destination_pixel_buffer);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  // Unlock destination pixel buffer.
  lock_status = CVPixelBufferUnlockBaseAddress(destination_pixel_buffer, 0);
  DCHECK_EQ(lock_status, kCVReturnSuccess);
  return success;
}

bool SampleBufferTransformer::TransformSampleBufferFromMjpegToI420(
    uint8_t* source_buffer_data_base_address,
    size_t source_buffer_data_size,
    size_t source_width,
    size_t source_height,
    CVPixelBufferRef destination_pixel_buffer) {
  DCHECK(destination_pixel_format_ == kPixelFormatI420);
  // Rescaling has to be done in a separate step.
  const bool rescale_needed =
      static_cast<size_t>(destination_size_.width()) != source_width ||
      static_cast<size_t>(destination_size_.height()) != source_height;

  // Step 1: Convert MJPEG -> I420.
  I420Planes i420_fullscale_buffer;
  if (!rescale_needed) {
    i420_fullscale_buffer =
        GetI420PlanesFromPixelBuffer(destination_pixel_buffer);
  } else {
    i420_fullscale_buffer = EnsureI420BufferSizeAndGetPlanes(
        source_width, source_height, &intermediate_i420_buffer_);
  }
  if (!ConvertFromMjpegToI420(source_buffer_data_base_address,
                              source_buffer_data_size, i420_fullscale_buffer)) {
    return false;
  }

  // Step 2: Rescale I420.
  if (rescale_needed) {
    I420Planes i420_destination_buffer =
        GetI420PlanesFromPixelBuffer(destination_pixel_buffer);
    ScaleI420(i420_fullscale_buffer, i420_destination_buffer);
  }
  return true;
}

bool SampleBufferTransformer::TransformSampleBufferFromMjpegToNV12(
    uint8_t* source_buffer_data_base_address,
    size_t source_buffer_data_size,
    size_t source_width,
    size_t source_height,
    CVPixelBufferRef destination_pixel_buffer) {
  DCHECK(destination_pixel_format_ == kPixelFormatNv12);
  // Rescaling has to be done in a separate step.
  const bool rescale_needed =
      static_cast<size_t>(destination_size_.width()) != source_width ||
      static_cast<size_t>(destination_size_.height()) != source_height;

  // Step 1: Convert MJPEG -> NV12.
  NV12Planes nv12_fullscale_buffer;
  if (!rescale_needed) {
    nv12_fullscale_buffer =
        GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
  } else {
    nv12_fullscale_buffer = EnsureNV12BufferSizeAndGetPlanes(
        source_width, source_height, &intermediate_nv12_buffer_);
  }
  if (!ConvertFromMjpegToNV12(source_buffer_data_base_address,
                              source_buffer_data_size, nv12_fullscale_buffer)) {
    return false;
  }

  // Step 2: Rescale NV12.
  if (rescale_needed) {
    NV12Planes nv12_destination_buffer =
        GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
    ScaleNV12(nv12_fullscale_buffer, nv12_destination_buffer);
  }
  return true;
}

}  // namespace media
