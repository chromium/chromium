// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/apple/sample_buffer_transformer.h"

#include <tuple>

#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "media/capture/video/apple/test/pixel_buffer_test_utils.h"
#include "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace media {

namespace {

// Example single colored .jpg file (created with MSPaint). It is of RGB color
// (255, 127, 63).
const uint8_t kExampleJpegData[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x03, 0x05, 0x03, 0x03, 0x03, 0x03, 0x03, 0x06, 0x04,
    0x04, 0x03, 0x05, 0x07, 0x06, 0x07, 0x07, 0x07, 0x06, 0x07, 0x07, 0x08,
    0x09, 0x0b, 0x09, 0x08, 0x08, 0x0a, 0x08, 0x07, 0x07, 0x0a, 0x0d, 0x0a,
    0x0a, 0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x07, 0x09, 0x0e, 0x0f, 0x0d, 0x0c,
    0x0e, 0x0b, 0x0c, 0x0c, 0x0c, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x06, 0x03, 0x03, 0x06, 0x0c, 0x08, 0x07, 0x08,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x10, 0x00,
    0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
    0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
    0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81,
    0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24,
    0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
    0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86,
    0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
    0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
    0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc4, 0x00,
    0x1f, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x11, 0x00,
    0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00,
    0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31,
    0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08,
    0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15,
    0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
    0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84,
    0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
    0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4,
    0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xda, 0x00,
    0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xf7,
    0x8a, 0x28, 0xa2, 0xbf, 0x89, 0xcf, 0xf4, 0x50, 0x28, 0xa2, 0x8a, 0x00,
    0xff, 0xd9};
constexpr size_t kExampleJpegDataSize = 638;
constexpr uint32_t kExampleJpegWidth = 32;
constexpr uint32_t kExampleJpegHeight = 16;
constexpr uint32_t kExampleJpegScaledDownWidth = 16;
constexpr uint32_t kExampleJpegScaledDownHeight = 8;

const uint8_t kInvalidJpegData[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
constexpr size_t kInvalidJpegDataSize = 24;

constexpr uint8_t kColorR = 255u;
constexpr uint8_t kColorG = 127u;
constexpr uint8_t kColorB = 63u;

constexpr unsigned int kFullResolutionWidth = 128;
constexpr unsigned int kFullResolutionHeight = 96;
constexpr unsigned int kScaledDownResolutionWidth = 64;
constexpr unsigned int kScaledDownResolutionHeight = 48;

// NV12 a.k.a. 420v
constexpr OSType kPixelFormatNv12 =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
// UYVY a.k.a. 2vuy
constexpr OSType kPixelFormatUyvy = kCVPixelFormatType_422YpCbCr8;
// YUY2 a.k.a. yuvs
constexpr OSType kPixelFormatYuy2 = kCVPixelFormatType_422YpCbCr8_yuvs;
// I420 a.k.a. y420
constexpr OSType kPixelFormatI420 = kCVPixelFormatType_420YpCbCr8Planar;

auto SupportedCaptureFormats() {
  return ::testing::Values(kPixelFormatNv12, kPixelFormatUyvy,
                           kPixelFormatYuy2);
}

auto SupportedOutputFormats() {
  return ::testing::Values(kPixelFormatNv12, kPixelFormatI420);
}

// Gives parameterized tests a readable suffix.
// E.g. ".../yuvsTo420v" instead of ".../4"
std::string TestParametersOSTypeTupleToString(
    testing::TestParamInfo<std::tuple<OSType, OSType>> info) {
  auto [input_pixel_format, output_pixel_format] = info.param;
  return MacFourCCToString(input_pixel_format) + std::string("To") +
         MacFourCCToString(output_pixel_format);
}
std::string TestParametersOSTypeToString(testing::TestParamInfo<OSType> info) {
  return MacFourCCToString(info.param);
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef> CreatePixelBuffer(
    OSType pixel_format,
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b) {
  // Create a YUVS buffer in main memory.
  std::unique_ptr<ByteArrayPixelBuffer> yuvs_buffer =
      CreateYuvsPixelBufferFromSingleRgbColor(width, height, r, g, b);
  // Convert and/or transfer to a pixel buffer that has an IOSurface.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer =
      PixelBufferPool::Create(pixel_format, width, height, 1)->CreateBuffer();
  PixelBufferTransferer transferer;
  bool success = transferer.TransferImage(yuvs_buffer->pixel_buffer.get(),
                                          pixel_buffer.get());
  DCHECK(success);
  return pixel_buffer;
}

enum class PixelBufferType {
  kIoSurfaceBacked,
  kIoSurfaceMissing,
};

void NonPlanarCvPixelBufferReleaseCallback(void* releaseRef, const void* data) {
  free(const_cast<void*>(data));
}

void PlanarCvPixelBufferReleaseCallback(void* releaseRef,
                                        const void* data,
                                        size_t size,
                                        size_t num_planes,
                                        const void* planes[]) {
  free(const_cast<void*>(data));
  for (size_t plane = 0; plane < num_planes; ++plane) {
    free(const_cast<void*>(planes[plane]));
  }
}

std::pair<uint8_t*, size_t> GetDataAndStride(CVPixelBufferRef pixel_buffer,
                                             size_t plane) {
  if (CVPixelBufferIsPlanar(pixel_buffer)) {
    return {static_cast<uint8_t*>(
                CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, plane)),
            CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, plane)};
  } else {
    DCHECK_EQ(plane, 0u) << "Non-planar pixel buffers only have 1 plane.";
    return {static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixel_buffer)),
            CVPixelBufferGetBytesPerRow(pixel_buffer)};
  }
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef> AddPadding(
    CVPixelBufferRef pixel_buffer,
    OSType pixel_format,
    int width,
    int height,
    int padding) {
  size_t num_planes = CVPixelBufferGetPlaneCount(pixel_buffer);
  std::vector<size_t> plane_widths;
  std::vector<size_t> plane_heights;
  std::vector<size_t> plane_strides;
  if (CVPixelBufferIsPlanar(pixel_buffer)) {
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t plane_stride =
          CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, plane);
      size_t padded_stride = plane_stride + padding;
      size_t h = CVPixelBufferGetHeightOfPlane(pixel_buffer, plane);
      size_t w = CVPixelBufferGetWidthOfPlane(pixel_buffer, plane);
      plane_heights.push_back(h);
      plane_widths.push_back(w);
      plane_strides.push_back(padded_stride);
    }
  } else {
    // CVPixelBufferGetPlaneCount returns 0 for non-planar buffers.
    num_planes = 1;
    size_t plane_stride = CVPixelBufferGetBytesPerRow(pixel_buffer);
    size_t padded_stride = plane_stride + padding;
    size_t h = CVPixelBufferGetHeight(pixel_buffer);
    plane_heights.push_back(h);
    plane_strides.push_back(padded_stride);
  }
  std::vector<void*> plane_address;
  CHECK_EQ(
      CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly),
      kCVReturnSuccess);
  // Allocate and copy each plane.
  for (size_t plane = 0; plane < num_planes; ++plane) {
    plane_address.push_back(
        calloc(1, plane_strides[plane] * plane_heights[plane]));
    uint8_t* dst_ptr = static_cast<uint8_t*>(plane_address[plane]);
    auto [src_ptr, plane_stride] = GetDataAndStride(pixel_buffer, plane);
    CHECK(dst_ptr);
    CHECK(src_ptr);
    for (size_t r = 0; r < plane_heights[plane]; ++r) {
      memcpy(dst_ptr, src_ptr, plane_stride);
      src_ptr += plane_stride;
      dst_ptr += plane_strides[plane];
    }
  }
  CHECK_EQ(
      CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly),
      kCVReturnSuccess);

  base::apple::ScopedCFTypeRef<CVPixelBufferRef> padded_pixel_buffer;
  CVReturn create_buffer_result;
  if (CVPixelBufferIsPlanar(pixel_buffer)) {
    // Without some memory block the callback won't be called and we leak the
    // planar data.
    void* descriptor = calloc(1, sizeof(CVPlanarPixelBufferInfo_YCbCrPlanar));
    create_buffer_result = CVPixelBufferCreateWithPlanarBytes(
        nullptr, width, height, pixel_format, descriptor, 0, num_planes,
        plane_address.data(), plane_widths.data(), plane_heights.data(),
        plane_strides.data(), &PlanarCvPixelBufferReleaseCallback,
        plane_strides.data(), nullptr, padded_pixel_buffer.InitializeInto());
  } else {
    create_buffer_result = CVPixelBufferCreateWithBytes(
        nullptr, width, height, pixel_format, plane_address[0],
        plane_strides[0], &NonPlanarCvPixelBufferReleaseCallback, nullptr,
        nullptr, padded_pixel_buffer.InitializeInto());
  }
  DCHECK_EQ(create_buffer_result, kCVReturnSuccess);
  return padded_pixel_buffer;
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef> CreateSampleBuffer(
    OSType pixel_format,
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    PixelBufferType pixel_buffer_type,
    size_t padding = 0) {
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer =
      CreatePixelBuffer(pixel_format, width, height, r, g, b);
  if (padding != 0) {
    CHECK_EQ(pixel_buffer_type, PixelBufferType::kIoSurfaceMissing)
        << "Padding does not work with IOSurfaces.";
  }
  if (pixel_buffer_type == PixelBufferType::kIoSurfaceMissing) {
    // Our pixel buffer currently has an IOSurface. To get rid of it, we perform
    // a pixel buffer transfer to a destination pixel buffer that is not backed
    // by an IOSurface. The resulting pixel buffer will have the desired color.
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> iosurfaceless_pixel_buffer;
    CVReturn create_buffer_result =
        CVPixelBufferCreate(nullptr, width, height, pixel_format, nullptr,
                            iosurfaceless_pixel_buffer.InitializeInto());
    DCHECK_EQ(create_buffer_result, kCVReturnSuccess);
    PixelBufferTransferer transferer;
    bool success = transferer.TransferImage(pixel_buffer.get(),
                                            iosurfaceless_pixel_buffer.get());
    DCHECK(success);
    DCHECK(!CVPixelBufferGetIOSurface(iosurfaceless_pixel_buffer.get()));
    pixel_buffer = iosurfaceless_pixel_buffer;

    if (padding > 0) {
      pixel_buffer =
          AddPadding(pixel_buffer.get(), pixel_format, width, height, padding);
    }
  }

  // Wrap the pixel buffer in a sample buffer.
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format_description;
  OSStatus status = CMVideoFormatDescriptionCreateForImageBuffer(
      nil, pixel_buffer.get(), format_description.InitializeInto());
  DCHECK(status == noErr);

  // Dummy information to make CMSampleBufferCreateForImageBuffer() happy.
  CMSampleTimingInfo timing_info;
  timing_info.decodeTimeStamp = kCMTimeInvalid;
  timing_info.presentationTimeStamp = CMTimeMake(0, CMTimeScale(NSEC_PER_SEC));
  timing_info.duration =
      CMTimeMake(33 * NSEC_PER_MSEC, CMTimeScale(NSEC_PER_SEC));  // 30 fps

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  status = CMSampleBufferCreateForImageBuffer(
      nil, pixel_buffer.get(), YES, nil, nullptr, format_description.get(),
      &timing_info, sample_buffer.InitializeInto());
  DCHECK(status == noErr);
  return sample_buffer;
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef> CreateMjpegSampleBuffer(
    const uint8_t* mjpeg_data,
    size_t mjpeg_data_size,
    size_t width,
    size_t height) {
  CMBlockBufferCustomBlockSource source = {0};
  source.FreeBlock = [](void* refcon, void* doomedMemoryBlock,
                        size_t sizeInBytes) {
    // Do nothing. The data to be released is not dynamically allocated in this
    // test code.
  };

  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data_buffer;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      nil, const_cast<void*>(static_cast<const void*>(mjpeg_data)),
      mjpeg_data_size, nil, &source, 0, mjpeg_data_size, 0,
      data_buffer.InitializeInto());
  DCHECK(status == noErr);

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format_description;
  status = CMVideoFormatDescriptionCreate(nil, kCMVideoCodecType_JPEG_OpenDML,
                                          width, height, nil,
                                          format_description.InitializeInto());
  DCHECK(status == noErr);

  // Dummy information to make CMSampleBufferCreateReady() happy.
  CMSampleTimingInfo timing_info;
  timing_info.decodeTimeStamp = kCMTimeInvalid;
  timing_info.presentationTimeStamp = CMTimeMake(0, CMTimeScale(NSEC_PER_SEC));
  timing_info.duration =
      CMTimeMake(33 * NSEC_PER_MSEC, CMTimeScale(NSEC_PER_SEC));  // 30 fps

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  status = CMSampleBufferCreateReady(
      nil, data_buffer.get(), format_description.get(), 1, 1, &timing_info, 1,
      &kExampleJpegDataSize, sample_buffer.InitializeInto());
  DCHECK(status == noErr);
  return sample_buffer;
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef>
CreateExampleMjpegSampleBuffer() {
  // Sanity-check the example data.
  int width;
  int height;
  int result =
      libyuv::MJPGSize(kExampleJpegData, kExampleJpegDataSize, &width, &height);
  DCHECK(result == 0);
  DCHECK_EQ(width, static_cast<int>(kExampleJpegWidth));
  DCHECK_EQ(height, static_cast<int>(kExampleJpegHeight));
  return CreateMjpegSampleBuffer(kExampleJpegData, kExampleJpegDataSize,
                                 kExampleJpegWidth, kExampleJpegHeight);
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef>
CreateInvalidMjpegSampleBuffer() {
  return CreateMjpegSampleBuffer(kInvalidJpegData, kInvalidJpegDataSize,
                                 kExampleJpegWidth, kExampleJpegHeight);
}

}  // namespace

class SampleBufferTransformerPixelTransferTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<OSType, OSType>> {};

#if BUILDFLAG(IS_IOS)
TEST_P(SampleBufferTransformerPixelTransferTest,
       CanRotateBy90DegreesClockwise) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);

  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kFullResolutionWidth, kFullResolutionHeight),
      /*rotation_angle*/ 90, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> roatated_pixel_buffer =
      transformer->Rotate(output_pixel_buffer.get());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(roatated_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionWidth,
            CVPixelBufferGetHeight(roatated_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight,
            CVPixelBufferGetWidth(roatated_pixel_buffer.get()));
}
#endif

TEST_P(SampleBufferTransformerPixelTransferTest, CanConvertFullScale) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kFullResolutionWidth, kFullResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerPixelTransferTest, CanConvertAndScaleDown) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerPixelTransferTest,
       CanConvertAndScaleDownWhenIoSurfaceIsMissing) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerPixelTransferTest,
       CanConvertWithPaddingFullScale) {
  auto [input_pixel_format, output_pixel_format] = GetParam();
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing, /*padding*/ 100);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kFullResolutionWidth, kFullResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerPixelTransferTest,
       CanConvertAndScaleWithPadding) {
  auto [input_pixel_format, output_pixel_format] = GetParam();
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing, /*padding*/ 100);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kPixelBufferTransfer,
      output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

INSTANTIATE_TEST_SUITE_P(SampleBufferTransformerTest,
                         SampleBufferTransformerPixelTransferTest,
                         ::testing::Combine(SupportedCaptureFormats(),
                                            SupportedOutputFormats()),
                         TestParametersOSTypeTupleToString);

class SampleBufferTransformerLibyuvTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<OSType, OSType>> {};

TEST_P(SampleBufferTransformerLibyuvTest, CanConvertFullScale) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kFullResolutionWidth, kFullResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerLibyuvTest, CanConvertAndScaleDown) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerLibyuvTest, CanConvertWithPaddingFullScale) {
  auto [input_pixel_format, output_pixel_format] = GetParam();
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing, /*padding*/ 100);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kFullResolutionWidth, kFullResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerLibyuvTest, CanConvertAndScaleWithPadding) {
  auto [input_pixel_format, output_pixel_format] = GetParam();
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing, /*padding*/ 100);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerLibyuvTest,
       CanConvertAndScaleDownWhenIoSurfaceIsMissing) {
  auto [input_pixel_format, output_pixel_format] = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateSampleBuffer(input_pixel_format, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceMissing);
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

INSTANTIATE_TEST_SUITE_P(SampleBufferTransformerTest,
                         SampleBufferTransformerLibyuvTest,
                         ::testing::Combine(SupportedCaptureFormats(),
                                            SupportedOutputFormats()),
                         TestParametersOSTypeTupleToString);

class SampleBufferTransformerMjpegTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<OSType> {};

TEST_P(SampleBufferTransformerMjpegTest, CanConvertFullScale) {
  OSType output_pixel_format = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateExampleMjpegSampleBuffer();
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kExampleJpegWidth, kExampleJpegHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_EQ(kExampleJpegWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kExampleJpegHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

#if defined(ARCH_CPU_ARM64)
// Disabled, see https://crbug.com/1354691
#define MAYBE_CanConvertAndScaleDown DISABLED_CanConvertAndScaleDown
#else
#define MAYBE_CanConvertAndScaleDown CanConvertAndScaleDown
#endif
TEST_P(SampleBufferTransformerMjpegTest, MAYBE_CanConvertAndScaleDown) {
  OSType output_pixel_format = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateExampleMjpegSampleBuffer();
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kExampleJpegScaledDownWidth, kExampleJpegScaledDownHeight), 0,
      1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());

  EXPECT_EQ(kExampleJpegScaledDownWidth,
            CVPixelBufferGetWidth(output_pixel_buffer.get()));
  EXPECT_EQ(kExampleJpegScaledDownHeight,
            CVPixelBufferGetHeight(output_pixel_buffer.get()));
  EXPECT_TRUE(PixelBufferIsSingleColor(output_pixel_buffer.get(), kColorR,
                                       kColorG, kColorB));
}

TEST_P(SampleBufferTransformerMjpegTest,
       AttemptingToTransformInvalidMjpegFailsGracefully) {
  OSType output_pixel_format = GetParam();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> input_sample_buffer =
      CreateInvalidMjpegSampleBuffer();
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();
  transformer->Reconfigure(
      SampleBufferTransformer::Transformer::kLibyuv, output_pixel_format,
      gfx::Size(kExampleJpegWidth, kExampleJpegHeight), 0, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_pixel_buffer =
      transformer->Transform(input_sample_buffer.get());
  EXPECT_FALSE(output_pixel_buffer);
}

INSTANTIATE_TEST_SUITE_P(SampleBufferTransformerTest,
                         SampleBufferTransformerMjpegTest,
                         SupportedOutputFormats(),
                         TestParametersOSTypeToString);

TEST(SampleBufferTransformerBestTransformerForNv12OutputTest,
     SourceAndDestinationResolutionMatches_InputSampleBuffer) {
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0 = CreateSampleBuffer(
      kPixelFormatNv12, kFullResolutionWidth, kFullResolutionHeight, kColorR,
      kColorG, kColorB, PixelBufferType::kIoSurfaceMissing);

  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample0.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample0.get()), 0);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_buffer =
      transformer->Transform(sample0.get());

  EXPECT_EQ(gfx::Size(kFullResolutionWidth, kFullResolutionHeight),
            transformer->destination_size());
  EXPECT_EQ(kFullResolutionWidth, CVPixelBufferGetWidth(output_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight, CVPixelBufferGetHeight(output_buffer.get()));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
  // Because sample0 has no underlying IOSurface, it should not be returned from
  // the transformer.
  EXPECT_NE(output_buffer.get(), CMSampleBufferGetImageBuffer(sample0.get()));

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1 = CreateSampleBuffer(
      kPixelFormatNv12, kScaledDownResolutionWidth, kScaledDownResolutionHeight,
      kColorR, kColorG, kColorB, PixelBufferType::kIoSurfaceBacked);

  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample1.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample1.get()), 0);
  output_buffer = transformer->Transform(sample1.get());

  EXPECT_EQ(gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight),
            transformer->destination_size());
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_buffer.get()));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
  // Because sample1 does have an IOSurface, it can be returned directly.
  EXPECT_EQ(output_buffer.get(), CMSampleBufferGetImageBuffer(sample1.get()));
}

// Same test as above, verifying that Transform() methods work on pixel buffers
// directly (so that there's no need to have a sample buffer).
TEST(SampleBufferTransformerBestTransformerForNv12OutputTest,
     SourceAndDestinationResolutionMatches_InputPixelBuffer) {
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0 = CreateSampleBuffer(
      kPixelFormatNv12, kFullResolutionWidth, kFullResolutionHeight, kColorR,
      kColorG, kColorB, PixelBufferType::kIoSurfaceMissing);
  CVPixelBufferRef pixel0 = CMSampleBufferGetImageBuffer(sample0.get());
  ASSERT_TRUE(pixel0);

  transformer->Reconfigure(
      SampleBufferTransformer::kBestTransformerForPixelBufferToNv12Output,
      kPixelFormatNv12, media::GetPixelBufferSize(pixel0), 0);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_buffer =
      transformer->Transform(pixel0);

  EXPECT_EQ(gfx::Size(kFullResolutionWidth, kFullResolutionHeight),
            transformer->destination_size());
  EXPECT_EQ(kFullResolutionWidth, CVPixelBufferGetWidth(output_buffer.get()));
  EXPECT_EQ(kFullResolutionHeight, CVPixelBufferGetHeight(output_buffer.get()));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
  // Because pixel0 has no underlying IOSurface, it should not be returned from
  // the transformer.
  EXPECT_NE(output_buffer.get(), pixel0);

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1 = CreateSampleBuffer(
      kPixelFormatNv12, kScaledDownResolutionWidth, kScaledDownResolutionHeight,
      kColorR, kColorG, kColorB, PixelBufferType::kIoSurfaceBacked);
  CVPixelBufferRef pixel1 = CMSampleBufferGetImageBuffer(sample1.get());
  ASSERT_TRUE(pixel1);

  transformer->Reconfigure(
      SampleBufferTransformer::kBestTransformerForPixelBufferToNv12Output,
      kPixelFormatNv12, media::GetPixelBufferSize(pixel1), 0);
  output_buffer = transformer->Transform(pixel1);

  EXPECT_EQ(gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight),
            transformer->destination_size());
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_buffer.get()));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
  // Because pixel1 does have an IOSurface, it can be returned directly.
  EXPECT_EQ(output_buffer.get(), pixel1);
}

TEST(SampleBufferTransformerBestTransformerForNv12OutputTest,
     CanConvertAndScaleDown_InputPixelBuffer) {
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer =
      CreateSampleBuffer(kPixelFormatNv12, kFullResolutionWidth,
                         kFullResolutionHeight, kColorR, kColorG, kColorB,
                         PixelBufferType::kIoSurfaceBacked);
  CVPixelBufferRef pixel_buffer =
      CMSampleBufferGetImageBuffer(sample_buffer.get());
  ASSERT_TRUE(pixel_buffer);

  transformer->Reconfigure(
      SampleBufferTransformer::kBestTransformerForPixelBufferToNv12Output,
      kPixelFormatNv12,
      gfx::Size(kScaledDownResolutionWidth, kScaledDownResolutionHeight), 0);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_buffer =
      transformer->Transform(pixel_buffer);

  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionWidth,
            CVPixelBufferGetWidth(output_buffer.get()));
  EXPECT_EQ(kScaledDownResolutionHeight,
            CVPixelBufferGetHeight(output_buffer.get()));
  EXPECT_TRUE(
      PixelBufferIsSingleColor(output_buffer.get(), kColorR, kColorG, kColorB));
}

TEST(SampleBufferTransformerBestTransformerForNv12OutputTest,
     DestinationPixelFormatIsAlwaysNv12) {
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample = CreateSampleBuffer(
      kPixelFormatNv12, kScaledDownResolutionWidth, kScaledDownResolutionHeight,
      kColorR, kColorG, kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_buffer =
      transformer->Transform(sample.get());
  EXPECT_EQ(kPixelFormatNv12, transformer->destination_pixel_format());
  EXPECT_EQ(
      kPixelFormatNv12,
      IOSurfaceGetPixelFormat(CVPixelBufferGetIOSurface(output_buffer.get())));

  sample = CreateSampleBuffer(kPixelFormatUyvy, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(kPixelFormatNv12, transformer->destination_pixel_format());
  EXPECT_EQ(
      kPixelFormatNv12,
      IOSurfaceGetPixelFormat(CVPixelBufferGetIOSurface(output_buffer.get())));

  sample = CreateSampleBuffer(kPixelFormatYuy2, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(kPixelFormatNv12, transformer->destination_pixel_format());
  EXPECT_EQ(
      kPixelFormatNv12,
      IOSurfaceGetPixelFormat(CVPixelBufferGetIOSurface(output_buffer.get())));

  sample = CreateSampleBuffer(kPixelFormatI420, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(kPixelFormatNv12, transformer->destination_pixel_format());
  EXPECT_EQ(
      kPixelFormatNv12,
      IOSurfaceGetPixelFormat(CVPixelBufferGetIOSurface(output_buffer.get())));

  sample = CreateExampleMjpegSampleBuffer();
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(kPixelFormatNv12, transformer->destination_pixel_format());
  EXPECT_EQ(
      kPixelFormatNv12,
      IOSurfaceGetPixelFormat(CVPixelBufferGetIOSurface(output_buffer.get())));
}

TEST(SampleBufferTransformerBestTransformerForNv12OutputTest,
     UsesBestTransformerPaths) {
  std::unique_ptr<SampleBufferTransformer> transformer =
      SampleBufferTransformer::Create();

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample = CreateSampleBuffer(
      kPixelFormatNv12, kScaledDownResolutionWidth, kScaledDownResolutionHeight,
      kColorR, kColorG, kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> output_buffer =
      transformer->Transform(sample.get());
  EXPECT_EQ(SampleBufferTransformer::Transformer::kPixelBufferTransfer,
            transformer->transformer());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));

  sample = CreateSampleBuffer(kPixelFormatUyvy, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(SampleBufferTransformer::Transformer::kPixelBufferTransfer,
            transformer->transformer());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));

  sample = CreateSampleBuffer(kPixelFormatYuy2, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(SampleBufferTransformer::Transformer::kPixelBufferTransfer,
            transformer->transformer());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));

  sample = CreateSampleBuffer(kPixelFormatI420, kScaledDownResolutionWidth,
                              kScaledDownResolutionHeight, kColorR, kColorG,
                              kColorB, PixelBufferType::kIoSurfaceBacked);
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(SampleBufferTransformer::Transformer::kPixelBufferTransfer,
            transformer->transformer());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));

  sample = CreateExampleMjpegSampleBuffer();
  transformer->Reconfigure(
      SampleBufferTransformer::GetBestTransformerForNv12Output(sample.get()),
      kPixelFormatNv12, media::GetSampleBufferSize(sample.get()), 0);
  output_buffer = transformer->Transform(sample.get());
  EXPECT_EQ(SampleBufferTransformer::Transformer::kLibyuv,
            transformer->transformer());
  EXPECT_TRUE(CVPixelBufferGetIOSurface(output_buffer.get()));
}

}  // namespace media
