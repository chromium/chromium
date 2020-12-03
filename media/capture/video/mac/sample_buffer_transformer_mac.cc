// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/sample_buffer_transformer_mac.h"

#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace media {

const base::Feature kInCaptureConvertToNv12{"InCaptureConvertToNv12",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kInCaptureConvertToNv12WithPixelTransfer{
    "InCaptureConvertToNv12WithPixelTransfer",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInCaptureConvertToNv12WithLibyuv{
    "InCaptureConvertToNv12WithLibyuv", base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

constexpr size_t kDefaultBufferPoolSize = 10;

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
  uint8_t* y_plane_data;
  uint8_t* u_plane_data;
  uint8_t* v_plane_data;
  size_t y_plane_stride;
  size_t u_plane_stride;
  size_t v_plane_stride;
};

size_t GetContiguousI420BufferSize(size_t width, size_t height) {
  gfx::Size dimensions(width, height);
  return VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kYPlane,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kVPlane,
                               dimensions)
             .GetArea();
}

I420Planes GetI420PlanesFromContiguousBuffer(uint8_t* data_base_address,
                                             size_t width,
                                             size_t height) {
  gfx::Size dimensions(width, height);
  gfx::Size y_plane_size =
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kYPlane, dimensions);
  gfx::Size u_plane_size =
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane, dimensions);
  gfx::Size v_plane_size =
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane, dimensions);
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
  if (i420_buffer->size() < required_size)
    i420_buffer->resize(required_size);
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
  uint8_t* y_plane_data;
  uint8_t* uv_plane_data;
  size_t y_plane_stride;
  size_t uv_plane_stride;
};

size_t GetContiguousNV12BufferSize(size_t width, size_t height) {
  gfx::Size dimensions(width, height);
  return VideoFrame::PlaneSize(PIXEL_FORMAT_NV12, VideoFrame::kYPlane,
                               dimensions)
             .GetArea() +
         VideoFrame::PlaneSize(PIXEL_FORMAT_NV12, VideoFrame::kUVPlane,
                               dimensions)
             .GetArea();
}

NV12Planes GetNV12PlanesFromContiguousBuffer(uint8_t* data_base_address,
                                             size_t width,
                                             size_t height) {
  gfx::Size dimensions(width, height);
  gfx::Size y_plane_size =
      VideoFrame::PlaneSize(PIXEL_FORMAT_NV12, VideoFrame::kYPlane, dimensions);
  gfx::Size uv_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_NV12, VideoFrame::kUVPlane, dimensions);
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
  if (nv12_buffer->size() < required_size)
    nv12_buffer->resize(required_size);
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

// Returns true on success. Converting uncompressed pixel formats should never
// fail, however MJPEG frames produces by some webcams have been observed to be
// invalid in special circumstances (see https://crbug.com/1147867). To support
// a graceful failure path in this case, this function may return false.
bool ConvertFromAnyToI420(CVPixelBufferRef source_pixel_buffer,
                          const I420Planes& destination) {
  auto pixel_format = CVPixelBufferGetPixelFormatType(source_pixel_buffer);
  switch (pixel_format) {
    // UYVY a.k.a. 2vuy
    case kCVPixelFormatType_422YpCbCr8: {
      const uint8_t* src_uyvy = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_uyvy = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      return libyuv::UYVYToI420(
                 src_uyvy, src_stride_uyvy, destination.y_plane_data,
                 destination.y_plane_stride, destination.u_plane_data,
                 destination.u_plane_stride, destination.v_plane_data,
                 destination.v_plane_stride, destination.width,
                 destination.height) == 0;
    }
    // YUY2 a.k.a. yuvs
    case kCMPixelFormat_422YpCbCr8_yuvs: {
      const uint8_t* src_yuy2 = static_cast<const uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_stride_yuy2 = CVPixelBufferGetBytesPerRow(source_pixel_buffer);
      return libyuv::YUY2ToI420(
                 src_yuy2, src_stride_yuy2, destination.y_plane_data,
                 destination.y_plane_stride, destination.u_plane_data,
                 destination.u_plane_stride, destination.v_plane_data,
                 destination.v_plane_stride, destination.width,
                 destination.height) == 0;
    }
    // MJPEG a.k.a. dmb1
    case kCMVideoCodecType_JPEG_OpenDML: {
      uint8_t* src_jpg = static_cast<uint8_t*>(
          CVPixelBufferGetBaseAddress(source_pixel_buffer));
      size_t src_jpg_size = CVPixelBufferGetDataSize(source_pixel_buffer);
      return ConvertFromMjpegToI420(src_jpg, src_jpg_size, destination);
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
      return libyuv::NV12ToI420(
                 src_y, src_stride_y, src_uv, src_stride_uv,
                 destination.y_plane_data, destination.y_plane_stride,
                 destination.u_plane_data, destination.u_plane_stride,
                 destination.v_plane_data, destination.v_plane_stride,
                 destination.width, destination.height) == 0;
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
      return libyuv::I420Copy(
                 src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
                 destination.y_plane_data, destination.y_plane_stride,
                 destination.u_plane_data, destination.u_plane_stride,
                 destination.v_plane_data, destination.v_plane_stride,
                 destination.width, destination.height) == 0;
    }
    default:
      NOTREACHED() << "Pixel format " << pixel_format << " not supported.";
  }
  return false;
}

void ConvertFromI420ToNV12(const I420Planes& source,
                           const NV12Planes& destination) {
  DCHECK_EQ(source.width, destination.width);
  DCHECK_EQ(source.height, destination.height);
  int result = libyuv::I420ToNV12(
      source.y_plane_data, source.y_plane_stride, source.u_plane_data,
      source.u_plane_stride, source.v_plane_data, source.v_plane_stride,
      destination.y_plane_data, destination.y_plane_stride,
      destination.uv_plane_data, destination.uv_plane_stride, source.width,
      source.height);
  // A webcam has never been observed to produce invalid uncompressed pixel
  // buffer, so we do not support a graceful failure path in this case.
  DCHECK_EQ(result, 0);
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
  libyuv::CopyPlane(source.y_plane_data, source.y_plane_stride,
                    destination.y_plane_data, destination.y_plane_stride,
                    destination.width, destination.height);
  size_t half_width = (destination.width + 1) >> 1;
  size_t half_height = (destination.height + 1) >> 1;
  libyuv::CopyPlane(source.uv_plane_data, source.uv_plane_stride,
                    destination.uv_plane_data, destination.uv_plane_stride,
                    half_width * 2, half_height);
}

}  // namespace

// static
std::unique_ptr<SampleBufferTransformer>
SampleBufferTransformer::CreateIfAutoReconfigureEnabled() {
  return IsAutoReconfigureEnabled()
             ? std::make_unique<SampleBufferTransformer>()
             : nullptr;
}

// static
std::unique_ptr<SampleBufferTransformer> SampleBufferTransformer::Create() {
  return std::make_unique<SampleBufferTransformer>();
}

// static
bool SampleBufferTransformer::IsAutoReconfigureEnabled() {
  return base::FeatureList::IsEnabled(kInCaptureConvertToNv12) ||
         base::FeatureList::IsEnabled(
             kInCaptureConvertToNv12WithPixelTransfer) ||
         base::FeatureList::IsEnabled(kInCaptureConvertToNv12WithLibyuv);
}

SampleBufferTransformer::SampleBufferTransformer()
    : transformer_(Transformer::kNotConfigured),
      destination_pixel_format_(0x0),
      destination_width_(0),
      destination_height_(0) {}

SampleBufferTransformer::~SampleBufferTransformer() {}

SampleBufferTransformer::Transformer SampleBufferTransformer::transformer()
    const {
  return transformer_;
}

OSType SampleBufferTransformer::destination_pixel_format() const {
  return destination_pixel_format_;
}

size_t SampleBufferTransformer::destination_width() const {
  return destination_width_;
}

size_t SampleBufferTransformer::destination_height() const {
  return destination_height_;
}

base::ScopedCFTypeRef<CVPixelBufferRef>
SampleBufferTransformer::AutoReconfigureAndTransform(
    CMSampleBufferRef sample_buffer) {
  AutoReconfigureBasedOnInputAndFeatureFlags(sample_buffer);
  return Transform(sample_buffer);
}

void SampleBufferTransformer::Reconfigure(
    Transformer transformer,
    OSType destination_pixel_format,
    size_t destination_width,
    size_t destination_height,
    base::Optional<size_t> buffer_pool_size) {
  DCHECK(transformer != Transformer::kLibyuv ||
         destination_pixel_format == kPixelFormatI420 ||
         destination_pixel_format == kPixelFormatNv12)
      << "Destination format is unsupported when running libyuv";
  if (transformer_ == transformer &&
      destination_pixel_format_ == destination_pixel_format &&
      destination_width_ == destination_width &&
      destination_height_ == destination_height) {
    // Already configured as desired, abort.
    return;
  }
  transformer_ = transformer;
  destination_pixel_format_ = destination_pixel_format;
  destination_width_ = destination_width;
  destination_height_ = destination_height;
  destination_pixel_buffer_pool_ =
      PixelBufferPool::Create(destination_pixel_format_, destination_width_,
                              destination_height_, buffer_pool_size);
  if (transformer == Transformer::kPixelBufferTransfer) {
    pixel_buffer_transferer_ = std::make_unique<PixelBufferTransferer>();
  } else {
    pixel_buffer_transferer_.reset();
  }
  intermediate_i420_buffer_.resize(0);
  intermediate_nv12_buffer_.resize(0);
}

void SampleBufferTransformer::AutoReconfigureBasedOnInputAndFeatureFlags(
    CMSampleBufferRef sample_buffer) {
  DCHECK(IsAutoReconfigureEnabled());
  Transformer desired_transformer = Transformer::kNotConfigured;
  size_t desired_width;
  size_t desired_height;
  if (CVPixelBufferRef pixel_buffer =
          CMSampleBufferGetImageBuffer(sample_buffer)) {
    // We have a pixel buffer.
    if (base::FeatureList::IsEnabled(kInCaptureConvertToNv12)) {
      // Pixel transfers are believed to be more efficient for X -> NV12.
      desired_transformer = Transformer::kPixelBufferTransfer;
    }
    desired_width = CVPixelBufferGetWidth(pixel_buffer);
    desired_height = CVPixelBufferGetHeight(pixel_buffer);
  } else {
    // We don't have a pixel buffer. Reconfigure to be prepared for MJPEG.
    if (base::FeatureList::IsEnabled(kInCaptureConvertToNv12)) {
      // Only libyuv supports MJPEG -> NV12.
      desired_transformer = Transformer::kLibyuv;
    }
    CMFormatDescriptionRef format_description =
        CMSampleBufferGetFormatDescription(sample_buffer);
    CMVideoDimensions dimensions =
        CMVideoFormatDescriptionGetDimensions(format_description);
    desired_width = dimensions.width;
    desired_height = dimensions.height;
  }
  if (base::FeatureList::IsEnabled(kInCaptureConvertToNv12WithPixelTransfer)) {
    desired_transformer = Transformer::kPixelBufferTransfer;
  } else if (base::FeatureList::IsEnabled(kInCaptureConvertToNv12WithLibyuv)) {
    desired_transformer = Transformer::kLibyuv;
  }
  Reconfigure(desired_transformer, kPixelFormatNv12, desired_width,
              desired_height, kDefaultBufferPoolSize);
}

base::ScopedCFTypeRef<CVPixelBufferRef> SampleBufferTransformer::Transform(
    CMSampleBufferRef sample_buffer) {
  DCHECK(transformer_ != Transformer::kNotConfigured);
  CVPixelBufferRef source_pixel_buffer =
      CMSampleBufferGetImageBuffer(sample_buffer);
  // Fast path: If source and destination formats are identical, return the
  // source pixel buffer.
  if (source_pixel_buffer &&
      destination_width_ == CVPixelBufferGetWidth(source_pixel_buffer) &&
      destination_height_ == CVPixelBufferGetHeight(source_pixel_buffer) &&
      destination_pixel_format_ ==
          CVPixelBufferGetPixelFormatType(source_pixel_buffer) &&
      CVPixelBufferGetIOSurface(source_pixel_buffer)) {
    return base::ScopedCFTypeRef<CVPixelBufferRef>(source_pixel_buffer,
                                                   base::scoped_policy::RETAIN);
  }
  // Create destination buffer from pool.
  base::ScopedCFTypeRef<CVPixelBufferRef> destination_pixel_buffer =
      destination_pixel_buffer_pool_->CreateBuffer();
  if (!destination_pixel_buffer) {
    // Maximum destination buffers exceeded. Old buffers are not being released
    // (and thus not returned to the pool) in time.
    LOG(ERROR) << "Maximum destination buffers exceeded";
    return base::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  if (source_pixel_buffer) {
    // Pixel buffer path. Do pixel transfer or libyuv conversion + rescale.
    TransformPixelBuffer(source_pixel_buffer, destination_pixel_buffer);
    return destination_pixel_buffer;
  }
  // Sample buffer path - it's MJPEG. Do libyuv conversion + rescale.
  if (!TransformSampleBuffer(sample_buffer, destination_pixel_buffer)) {
    LOG(ERROR) << "Failed to transform sample buffer.";
    return base::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  return destination_pixel_buffer;
}

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
      NOTREACHED();
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
      NOTREACHED();
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
  const bool rescale_needed = destination_width_ != source_width ||
                              destination_height_ != source_height;

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
    if (!ConvertFromAnyToI420(source_pixel_buffer, i420_fullscale_buffer)) {
      // Only MJPEG conversions are known to be able to fail. Because X is an
      // uncompressed pixel format, this conversion should never fail.
      NOTREACHED();
    }
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
  const bool rescale_needed = destination_width_ != source_width ||
                              destination_height_ != source_height;

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
    // Convert X -> I420 -> NV12. (We don't know how to do X -> NV12.)
    // TODO(https://crbug.com/1154273): Convert to NV12 directly.
    I420Planes i420_fullscale_buffer;
    if (source_pixel_format == kPixelFormatI420) {
      // We are already at I420.
      i420_fullscale_buffer = GetI420PlanesFromPixelBuffer(source_pixel_buffer);
    } else {
      // Convert X -> I420.
      i420_fullscale_buffer = EnsureI420BufferSizeAndGetPlanes(
          source_width, source_height, &intermediate_i420_buffer_);
      if (!ConvertFromAnyToI420(source_pixel_buffer, i420_fullscale_buffer)) {
        NOTREACHED();
      }
    }
    // Convert I420 -> NV12.
    if (!rescale_needed) {
      nv12_fullscale_buffer =
          GetNV12PlanesFromPixelBuffer(destination_pixel_buffer);
    } else {
      nv12_fullscale_buffer = EnsureNV12BufferSizeAndGetPlanes(
          source_width, source_height, &intermediate_nv12_buffer_);
    }
    ConvertFromI420ToNV12(i420_fullscale_buffer, nv12_fullscale_buffer);
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
  DCHECK(source_pixel_format == kPixelFormatMjpeg);
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
      NOTREACHED();
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
  const bool rescale_needed = destination_width_ != source_width ||
                              destination_height_ != source_height;

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
  const bool rescale_needed = destination_width_ != source_width ||
                              destination_height_ != source_height;

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
