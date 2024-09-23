// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/video_types.h"
#include "media/gpu/test/local_gpu_memory_buffer_manager.h"
#include "media/gpu/vaapi/test_utils.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_image_decoder_test_common.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/jpeg_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {
namespace {

using DecodedImagePtr = std::unique_ptr<vaapi_test_utils::DecodedImage>;

constexpr const char* kYuv422Filename = "pixel-1280x720.jpg";
constexpr const char* kYuv420Filename = "pixel-1280x720-yuv420.jpg";
constexpr const char* kYuv444Filename = "pixel-1280x720-yuv444.jpg";
constexpr const char* kOddHeightImageFilename = "pixel-40x23-yuv420.jpg";
constexpr const char* kOddWidthImageFilename = "pixel-41x22-yuv420.jpg";
constexpr const char* kOddDimensionsImageFilename = "pixel-41x23-yuv420.jpg";

const vaapi_test_utils::TestParam kVAImageTestCases[] = {
    {"YUV422", kYuv422Filename},
    {"YUV420", kYuv420Filename},
    {"YUV444", kYuv444Filename},
};

const vaapi_test_utils::TestParam kDmaBufTestCases[] = {
    {"YUV420", kYuv420Filename},
    {"OddHeightImage40x23", kOddHeightImageFilename},
    {"OddWidthImage41x22", kOddWidthImageFilename},
    {"OddDimensionsImage41x23", kOddDimensionsImageFilename},
};

constexpr double kMinSsim = 0.995;

// This file is not supported by the VAAPI, so we don't define expectations on
// the decode result.
constexpr const char* kUnsupportedFilename = "pixel-1280x720-grayscale.jpg";

// The size of the minimum coded unit for a YUV 4:2:0 image (both the width and
// the height of the MCU are the same for 4:2:0).
constexpr int k420MCUSize = 16;

// The largest maximum supported surface size we expect a driver to report for
// JPEG decoding.
constexpr gfx::Size kLargestSupportedSize(16 * 1024, 16 * 1024);

// Decodes the given |encoded_image| using libyuv and returns the result as a
// DecodedImage object. The decoded planes will be stored in |dest_*|.
// Note however, that this function does not takes ownership of |dest_*| or
// manage their memory.
DecodedImagePtr GetSwDecode(base::span<const uint8_t> encoded_image,
                            std::vector<uint8_t>* dest_y,
                            std::vector<uint8_t>* dest_u,
                            std::vector<uint8_t>* dest_v) {
  DCHECK(dest_y && dest_u && dest_v);
  JpegParseResult parse_result;
  const bool result = ParseJpegPicture(encoded_image, &parse_result);
  if (!result)
    return nullptr;

  const gfx::Size jpeg_size(
      base::strict_cast<int>(parse_result.frame_header.visible_width),
      base::strict_cast<int>(parse_result.frame_header.visible_height));
  if (jpeg_size.IsEmpty())
    return nullptr;

  const gfx::Size half_jpeg_size((jpeg_size.width() + 1) / 2,
                                 (jpeg_size.height() + 1) / 2);

  dest_y->resize(jpeg_size.GetArea());
  dest_u->resize(half_jpeg_size.GetArea());
  dest_v->resize(half_jpeg_size.GetArea());

  if (libyuv::ConvertToI420(
          encoded_image.data(), encoded_image.size(), dest_y->data(),
          jpeg_size.width(), dest_u->data(), half_jpeg_size.width(),
          dest_v->data(), half_jpeg_size.width(), 0, 0, jpeg_size.width(),
          jpeg_size.height(), jpeg_size.width(), jpeg_size.height(),
          libyuv::kRotate0, libyuv::FOURCC_MJPG) != 0) {
    return nullptr;
  }

  auto sw_decoded_jpeg = std::make_unique<vaapi_test_utils::DecodedImage>();
  sw_decoded_jpeg->fourcc = VA_FOURCC_I420;
  sw_decoded_jpeg->number_of_planes = 3u;
  sw_decoded_jpeg->size = jpeg_size;
  sw_decoded_jpeg->planes[0].data = dest_y->data();
  sw_decoded_jpeg->planes[0].stride = jpeg_size.width();
  sw_decoded_jpeg->planes[1].data = dest_u->data();
  sw_decoded_jpeg->planes[1].stride = half_jpeg_size.width();
  sw_decoded_jpeg->planes[2].data = dest_v->data();
  sw_decoded_jpeg->planes[2].stride = half_jpeg_size.width();

  return sw_decoded_jpeg;
}

// Generates a checkerboard pattern as a JPEG image of a specified |size| and
// |subsampling| format. Returns an empty vector on failure.
std::vector<unsigned char> GenerateJpegImage(
    const gfx::Size& size,
    SkJpegEncoder::Downsample subsampling = SkJpegEncoder::Downsample::k420) {
  DCHECK(!size.IsEmpty());

  // First build a raw RGBA image of the given size with a checkerboard pattern.
  const SkImageInfo image_info = SkImageInfo::Make(
      size.width(), size.height(), SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kOpaque_SkAlphaType);
  const size_t byte_size = image_info.computeMinByteSize();
  if (byte_size == SIZE_MAX)
    return {};
  const size_t stride = image_info.minRowBytes();
  DCHECK_EQ(4, SkColorTypeBytesPerPixel(image_info.colorType()));
  DCHECK_EQ(4 * size.width(), base::checked_cast<int>(stride));
  constexpr gfx::Size kCheckerRectSize(3, 5);
  std::vector<uint8_t> rgba_data(byte_size);
  uint8_t* data = rgba_data.data();
  for (int y = 0; y < size.height(); y++) {
    const bool y_bit = (((y / kCheckerRectSize.height()) & 0x1) == 0);
    for (int x = 0; x < base::checked_cast<int>(stride); x += 4) {
      const bool x_bit = (((x / kCheckerRectSize.width()) & 0x1) == 0);
      const SkColor color = (x_bit != y_bit) ? SK_ColorBLUE : SK_ColorMAGENTA;
      data[x + 0] = SkColorGetR(color);
      data[x + 1] = SkColorGetG(color);
      data[x + 2] = SkColorGetB(color);
      data[x + 3] = SkColorGetA(color);
    }
    data += stride;
  }

  // Now, encode it as a JPEG.
  //
  // TODO(andrescj): if this generates a large enough image (in terms of byte
  // size), it will be decoded incorrectly in AMD Stoney Ridge (see
  // b/127874877). When that's resolved, change the quality here to 100 so that
  // the generated JPEG is large.
  std::vector<unsigned char> jpeg_data;
  if (gfx::JPEGCodec::Encode(
          SkPixmap(image_info, rgba_data.data(), stride) /* input */,
          95 /* quality */, subsampling /* downsample */,
          &jpeg_data /* output */)) {
    return jpeg_data;
  }
  return {};
}

// Rounds |n| to the greatest multiple of |m| that is less than or equal to |n|.
int RoundDownToMultiple(int n, int m) {
  DCHECK_GE(n, 0);
  DCHECK_GT(m, 0);
  return (n / m) * m;
}

// Rounds |n| to the smallest multiple of |m| that is greater than or equal to
// |n|.
int RoundUpToMultiple(int n, int m) {
  DCHECK_GE(n, 0);
  DCHECK_GT(m, 0);
  if (n % m == 0)
    return n;
  base::CheckedNumeric<int> safe_n(n);
  safe_n += m;
  return RoundDownToMultiple(safe_n.ValueOrDie(), m);
}

// Given a minimum supported surface dimension (width or height) value
// |min_surface_supported|, this function returns a non-zero coded dimension of
// a 4:2:0 JPEG image that would not be supported because the dimension is right
// below the supported value. For example, if |min_surface_supported| is 19,
// this function should return 16 because for a 4:2:0 image, both coded
// dimensions should be multiples of 16. If an unsupported dimension was found
// (i.e., |min_surface_supported| > 16), this function returns true, false
// otherwise.
bool GetMinUnsupportedDimension(int min_surface_supported,
                                int* min_unsupported) {
  if (min_surface_supported <= k420MCUSize)
    return false;
  *min_unsupported =
      RoundDownToMultiple(min_surface_supported - 1, k420MCUSize);
  return true;
}

// Given a minimum supported surface dimension (width or height) value
// |min_surface_supported|, this function returns a non-zero coded dimension of
// a 4:2:0 JPEG image that would be supported because the dimension is at least
// the minimum. For example, if |min_surface_supported| is 35, this function
// should return 48 because for a 4:2:0 image, both coded dimensions should be
// multiples of 16.
int GetMinSupportedDimension(int min_surface_supported) {
  LOG_ASSERT(min_surface_supported > 0);
  return RoundUpToMultiple(min_surface_supported, k420MCUSize);
}

// Given a maximum supported surface dimension (width or height) value
// |max_surface_supported|, this function returns the coded dimension of a 4:2:0
// JPEG image that would be supported because the dimension is at most the
// maximum. For example, if |max_surface_supported| is 65, this function
// should return 64 because for a 4:2:0 image, both coded dimensions should be
// multiples of 16.
int GetMaxSupportedDimension(int max_surface_supported) {
  return RoundDownToMultiple(max_surface_supported, k420MCUSize);
}

}  // namespace

class VaapiJpegDecoderTest : public VaapiImageDecoderTestCommon {
 protected:
  VaapiJpegDecoderTest()
      : VaapiImageDecoderTestCommon(std::make_unique<VaapiJpegDecoder>()) {}

  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image,
      uint32_t preferred_fourcc,
      VaapiImageDecodeStatus* status = nullptr);

  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status = nullptr);
};

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image,
    uint32_t preferred_fourcc,
    VaapiImageDecodeStatus* status) {
  const VaapiImageDecodeStatus decode_status = Decoder()->Decode(encoded_image);
  EXPECT_EQ(!!Decoder()->GetScopedVASurface(),
            decode_status == VaapiImageDecodeStatus::kSuccess);

  // Still try to get image when decode fails.
  VaapiImageDecodeStatus image_status;
  std::unique_ptr<ScopedVAImage> scoped_image;
  scoped_image = static_cast<VaapiJpegDecoder*>(Decoder())->GetImage(
      preferred_fourcc, &image_status);
  EXPECT_EQ(!!scoped_image, image_status == VaapiImageDecodeStatus::kSuccess);

  // Return the first fail status.
  if (status) {
    *status = decode_status != VaapiImageDecodeStatus::kSuccess ? decode_status
                                                                : image_status;
  }
  return scoped_image;
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image,
    VaapiImageDecodeStatus* status) {
  return Decode(encoded_image, VA_FOURCC_I420, status);
}

// The intention of this test is to ensure that the workarounds added in
// VaapiWrapper::GetJpegDecodeSuitableImageFourCC() don't result in an
// unsupported image format.
TEST_F(VaapiJpegDecoderTest, MinimalImageFormatSupport) {
  // All drivers should support at least I420.
  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));

  // Additionally, the mesa VAAPI driver should support YV12, NV12 and YUYV.
  ASSERT_NE(VAImplementation::kInvalid, VaapiWrapper::GetImplementationType());
  if (VaapiWrapper::GetImplementationType() == VAImplementation::kMesaGallium) {
    VAImageFormat yv12_format{};
    yv12_format.fourcc = VA_FOURCC_YV12;
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(yv12_format));

    VAImageFormat nv12_format{};
    nv12_format.fourcc = VA_FOURCC_NV12;
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(nv12_format));

    VAImageFormat yuyv_format{};
    yuyv_format.fourcc = VA_FOURCC('Y', 'U', 'Y', 'V');
    ASSERT_TRUE(VaapiWrapper::IsImageFormatSupported(yuyv_format));
  }
}

TEST_P(VaapiJpegDecoderTest, DecodeSucceeds) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::as_byte_span(jpeg_data);

  // Skip the image if the VAAPI driver doesn't claim to support its chroma
  // subsampling format. However, we expect at least 4:2:0 and 4:2:2 support.
  const VaapiWrapper::InternalFormats supported_internal_formats =
      VaapiWrapper::GetDecodeSupportedInternalFormats(VAProfileJPEGBaseline);
  ASSERT_TRUE(supported_internal_formats.yuv420);
  ASSERT_TRUE(supported_internal_formats.yuv422);
  JpegParseResult parse_result;
  ASSERT_TRUE(ParseJpegPicture(encoded_image, &parse_result));
  const unsigned int rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  ASSERT_NE(kInvalidVaRtFormat, rt_format);
  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(VAProfileJPEGBaseline,
                                                          rt_format)) {
    GTEST_SKIP();
  }

  // Note that this test together with
  // VaapiJpegDecoderTest.MinimalImageFormatSupport gives us two guarantees:
  //
  // 1) Every combination of supported internal format (returned by
  //    GetJpegDecodeSupportedInternalFormats()) and supported image format
  //    works with vaGetImage() (for JPEG decoding).
  //
  // 2) The FOURCC returned by VaapiWrapper::GetJpegDecodeSuitableImageFourCC()
  //    corresponds to a supported image format.
  //
  // Note that we expect VA_FOURCC_I420 and VA_FOURCC_NV12 support in all
  // drivers.
  const std::vector<VAImageFormat>& supported_image_formats =
      VaapiWrapper::GetSupportedImageFormatsForTesting();
  EXPECT_GE(supported_image_formats.size(), 2u);

  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));

  VAImageFormat nv12_format{};
  nv12_format.fourcc = VA_FOURCC_NV12;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(nv12_format));

  // Decode the image using libyuv. Using |temp_*| for resource management.
  std::vector<uint8_t> temp_y;
  std::vector<uint8_t> temp_u;
  std::vector<uint8_t> temp_v;
  DecodedImagePtr sw_decoded_jpeg =
      GetSwDecode(encoded_image, &temp_y, &temp_u, &temp_v);
  ASSERT_TRUE(sw_decoded_jpeg);

  // Now run the comparison between the sw and hw image decodes for the
  // supported formats.
  for (const auto& image_format : supported_image_formats) {
    std::unique_ptr<ScopedVAImage> scoped_image =
        Decode(encoded_image, image_format.fourcc);
    ASSERT_TRUE(scoped_image);
    ASSERT_TRUE(Decoder()->GetScopedVASurface());
    EXPECT_TRUE(Decoder()->GetScopedVASurface()->IsValid());
    EXPECT_EQ(Decoder()->GetScopedVASurface()->size().width(),
              base::strict_cast<int>(parse_result.frame_header.visible_width));
    EXPECT_EQ(Decoder()->GetScopedVASurface()->size().height(),
              base::strict_cast<int>(parse_result.frame_header.visible_height));
    EXPECT_EQ(rt_format, Decoder()->GetScopedVASurface()->format());
    const uint32_t actual_fourcc = scoped_image->image()->format.fourcc;
    // TODO(andrescj): CompareImages() only supports I420, NV12, YUY2, and YUYV.
    // Make it support all the image formats we expect and call it
    // unconditionally.
    if (actual_fourcc == VA_FOURCC_I420 || actual_fourcc == VA_FOURCC_NV12 ||
        actual_fourcc == VA_FOURCC_YUY2 ||
        actual_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
      ASSERT_TRUE(vaapi_test_utils::CompareImages(
          *sw_decoded_jpeg,
          vaapi_test_utils::ScopedVAImageToDecodedImage(scoped_image.get()),
          kMinSsim));
    }
    DVLOG(1) << "Got a " << FourccToString(scoped_image->image()->format.fourcc)
             << " VAImage (preferred " << FourccToString(image_format.fourcc)
             << ")";
  }
}

// Make sure that JPEGs whose size is in the supported size range are decoded
// successfully.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
// TODO(andrescj): consider recreating the decoder for every size so that no
// state is retained.
TEST_F(VaapiJpegDecoderTest, DecodeSucceedsForSupportedSizes) {
  gfx::Size min_supported_size;
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetSupportedResolutions(
      VAProfileJPEGBaseline, VaapiWrapper::CodecMode::kDecode,
      min_supported_size, max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // The actual image min/max coded size depends on the subsampling format. For
  // example, for 4:2:0, the coded dimensions must be multiples of 16. So, if
  // the minimum surface size is, e.g., 18x18, the minimum image coded size is
  // 32x32. Get those actual min/max coded sizes now.
  const int min_width = GetMinSupportedDimension(min_supported_size.width());
  const int min_height = GetMinSupportedDimension(min_supported_size.height());
  const int max_width = GetMaxSupportedDimension(max_supported_size.width());
  const int max_height = GetMaxSupportedDimension(max_supported_size.height());
  ASSERT_GT(max_width, 0);
  ASSERT_GT(max_height, 0);
  const std::vector<gfx::Size> test_sizes = {{min_width, min_height},
                                             {min_width, max_height},
                                             {max_width, min_height},
                                             {max_width, max_height}};
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    auto jpeg_data_span = base::as_bytes(base::make_span(jpeg_data));
    ASSERT_FALSE(jpeg_data.empty());
    std::unique_ptr<ScopedVAImage> scoped_image = Decode(jpeg_data_span);
    ASSERT_TRUE(scoped_image)
        << "Decode unexpectedly failed for size = " << test_size.ToString();
    ASSERT_TRUE(Decoder()->GetScopedVASurface());
    EXPECT_TRUE(Decoder()->GetScopedVASurface()->IsValid());

    // Decode the image using libyuv. Using |temp_*| for resource management.
    std::vector<uint8_t> temp_y;
    std::vector<uint8_t> temp_u;
    std::vector<uint8_t> temp_v;
    DecodedImagePtr sw_decoded_jpeg =
        GetSwDecode(jpeg_data_span, &temp_y, &temp_u, &temp_v);
    ASSERT_TRUE(sw_decoded_jpeg);

    EXPECT_TRUE(vaapi_test_utils::CompareImages(
        *sw_decoded_jpeg,
        vaapi_test_utils::ScopedVAImageToDecodedImage(scoped_image.get()),
        kMinSsim))
        << "The SSIM check unexpectedly failed for size = "
        << test_size.ToString();
  }
}

class VaapiJpegDecoderWithDmaBufsTest : public VaapiJpegDecoderTest {
 public:
  VaapiJpegDecoderWithDmaBufsTest() = default;
  ~VaapiJpegDecoderWithDmaBufsTest() override = default;
};

// TODO(andrescj): test other JPEG formats besides YUV 4:2:0.
TEST_P(VaapiJpegDecoderWithDmaBufsTest, DecodeSucceeds) {
  ASSERT_NE(VAImplementation::kInvalid, VaapiWrapper::GetImplementationType());
  if (VaapiWrapper::GetImplementationType() == VAImplementation::kMesaGallium) {
    // TODO(crbug.com/40632250): until we support surfaces with multiple buffer
    // objects, the AMD driver fails this test.
    GTEST_SKIP();
  }

  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::as_byte_span(jpeg_data);

  // Decode into a VAAPI-allocated surface.
  const VaapiImageDecodeStatus decode_status = Decoder()->Decode(encoded_image);
  EXPECT_EQ(VaapiImageDecodeStatus::kSuccess, decode_status);
  ASSERT_TRUE(Decoder()->GetScopedVASurface());
  const gfx::Size va_surface_visible_size =
      Decoder()->GetScopedVASurface()->size();

  // The size stored in the ScopedVASurface should be the visible size of the
  // JPEG.
  JpegParseResult parse_result;
  ASSERT_TRUE(ParseJpegPicture(encoded_image, &parse_result));
  EXPECT_EQ(gfx::Size(parse_result.frame_header.visible_width,
                      parse_result.frame_header.visible_height),
            va_surface_visible_size);

  // Export the surface.
  VaapiImageDecodeStatus export_status = VaapiImageDecodeStatus::kInvalidState;
  std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
      Decoder()->ExportAsNativePixmapDmaBuf(&export_status);
  EXPECT_EQ(VaapiImageDecodeStatus::kSuccess, export_status);
  ASSERT_TRUE(exported_pixmap);
  ASSERT_TRUE(exported_pixmap->pixmap);
  EXPECT_FALSE(Decoder()->GetScopedVASurface());

  // For JPEG decoding, the size of the surface we request is the coded size of
  // the JPEG. Make sure the surface contains that coded area.
  EXPECT_TRUE(gfx::Rect(exported_pixmap->va_surface_resolution)
                  .Contains(gfx::Rect(parse_result.frame_header.coded_width,
                                      parse_result.frame_header.coded_height)));

  // Make sure the visible area is contained by the surface.
  EXPECT_EQ(va_surface_visible_size, exported_pixmap->pixmap->GetBufferSize());
  EXPECT_FALSE(exported_pixmap->va_surface_resolution.IsEmpty());
  EXPECT_FALSE(exported_pixmap->pixmap->GetBufferSize().IsEmpty());
  ASSERT_TRUE(
      gfx::Rect(exported_pixmap->va_surface_resolution)
          .Contains(gfx::Rect(exported_pixmap->pixmap->GetBufferSize())));

  // TODO(andrescj): we could get a better lower bound based on the dimensions
  // and the format.
  ASSERT_GT(exported_pixmap->byte_size, 0u);

  // After exporting the surface, we should not be able to obtain a VAImage with
  // the decoded data.
  VAImageFormat i420_format{};
  i420_format.fourcc = VA_FOURCC_I420;
  EXPECT_TRUE(VaapiWrapper::IsImageFormatSupported(i420_format));
  VaapiImageDecodeStatus image_status = VaapiImageDecodeStatus::kSuccess;
  EXPECT_FALSE(static_cast<VaapiJpegDecoder*>(Decoder())->GetImage(
      i420_format.fourcc, &image_status));
  EXPECT_EQ(VaapiImageDecodeStatus::kInvalidState, image_status);

  // Workaround: in order to import and map the pixmap using minigbm when the
  // format is gfx::BufferFormat::YVU_420, we need to reorder the planes so that
  // the offsets are in increasing order as assumed in https://bit.ly/2NLubNN.
  // Otherwise, we get a validation error. In essence, we're making minigbm
  // think that it is mapping a YVU_420, but it's actually mapping a YUV_420.
  //
  // TODO(andrescj): revisit this once crrev.com/c/1573718 lands.
  gfx::NativePixmapHandle handle = exported_pixmap->pixmap->ExportHandle();
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(
                exported_pixmap->pixmap->GetBufferFormat()),
            handle.planes.size());
  if (exported_pixmap->pixmap->GetBufferFormat() == gfx::BufferFormat::YVU_420)
    std::swap(handle.planes[1], handle.planes[2]);

  std::unique_ptr<vaapi_test_utils::DecodedImage> decoded_image =
      vaapi_test_utils::NativePixmapToDecodedImage(
          handle, exported_pixmap->pixmap->GetBufferSize(),
          exported_pixmap->pixmap->GetBufferFormat());
  ASSERT_TRUE(decoded_image);

  // Decode the image using libyuv. Using |temp_*| for resource management.
  std::vector<uint8_t> temp_y;
  std::vector<uint8_t> temp_u;
  std::vector<uint8_t> temp_v;
  DecodedImagePtr sw_decoded_jpeg =
      GetSwDecode(encoded_image, &temp_y, &temp_u, &temp_v);
  ASSERT_TRUE(sw_decoded_jpeg);

  EXPECT_TRUE(vaapi_test_utils::CompareImages(*sw_decoded_jpeg, *decoded_image,
                                              kMinSsim));
}

// Make sure that JPEGs whose size is below the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForBelowMinSize) {
  gfx::Size min_supported_size;
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetSupportedResolutions(
      VAProfileJPEGBaseline, VaapiWrapper::CodecMode::kDecode,
      min_supported_size, max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // Get good (supported) minimum dimensions.
  const int good_width = GetMinSupportedDimension(min_supported_size.width());
  ASSERT_LE(good_width, max_supported_size.width());
  const int good_height = GetMinSupportedDimension(min_supported_size.height());
  ASSERT_LE(good_height, max_supported_size.height());

  // Get bad (unsupported) dimensions.
  int bad_width;
  const bool got_bad_width =
      GetMinUnsupportedDimension(min_supported_size.width(), &bad_width);
  int bad_height;
  const bool got_bad_height =
      GetMinUnsupportedDimension(min_supported_size.height(), &bad_height);

  // Now build and test the good/bad combinations that we expect will fail.
  std::vector<gfx::Size> test_sizes;
  if (got_bad_width)
    test_sizes.push_back({bad_width, good_height});
  if (got_bad_height)
    test_sizes.push_back({good_width, bad_height});
  if (got_bad_width && got_bad_height)
    test_sizes.push_back({bad_width, bad_height});
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    ASSERT_FALSE(jpeg_data.empty());
    VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::as_bytes(base::make_span(jpeg_data)), &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedImage, status);
    EXPECT_FALSE(Decoder()->GetScopedVASurface());
  }
}

// Make sure that JPEGs whose size is above the supported size range are
// rejected.
//
// TODO(andrescj): for now, this assumes 4:2:0. Handle other formats.
TEST_F(VaapiJpegDecoderTest, DecodeFailsForAboveMaxSize) {
  gfx::Size min_supported_size;
  gfx::Size max_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetSupportedResolutions(
      VAProfileJPEGBaseline, VaapiWrapper::CodecMode::kDecode,
      min_supported_size, max_supported_size));

  // Ensure the maximum supported size is reasonable.
  ASSERT_GE(max_supported_size.width(), min_supported_size.width());
  ASSERT_GE(max_supported_size.height(), min_supported_size.height());
  ASSERT_LE(max_supported_size.width(), kLargestSupportedSize.width());
  ASSERT_LE(max_supported_size.height(), kLargestSupportedSize.height());

  // Get good (supported) maximum dimensions.
  const int good_width = GetMaxSupportedDimension(max_supported_size.width());
  ASSERT_GE(good_width, min_supported_size.width());
  ASSERT_GT(good_width, 0);
  const int good_height = GetMaxSupportedDimension(max_supported_size.height());
  ASSERT_GE(good_height, min_supported_size.height());
  ASSERT_GT(good_height, 0);

  // Get bad (unsupported) dimensions.
  const int bad_width =
      RoundUpToMultiple(max_supported_size.width() + 1, k420MCUSize);
  const int bad_height =
      RoundUpToMultiple(max_supported_size.height() + 1, k420MCUSize);

  // Now build and test the good/bad combinations that we expect will fail.
  const std::vector<gfx::Size> test_sizes = {{bad_width, good_height},
                                             {good_width, bad_height},
                                             {bad_width, bad_height}};
  for (const auto& test_size : test_sizes) {
    const std::vector<unsigned char> jpeg_data = GenerateJpegImage(test_size);
    ASSERT_FALSE(jpeg_data.empty());
    VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
    ASSERT_FALSE(Decode(base::as_bytes(base::make_span(jpeg_data)), &status))
        << "Decode unexpectedly succeeded for size = " << test_size.ToString();
    EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedImage, status);
    EXPECT_FALSE(Decoder()->GetScopedVASurface());
  }
}

TEST_F(VaapiJpegDecoderTest, DecodeFails) {
  // A grayscale image (4:0:0) should be rejected.
  base::FilePath input_file = FindTestDataFilePath(kUnsupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  VaapiImageDecodeStatus status = VaapiImageDecodeStatus::kSuccess;
  ASSERT_FALSE(Decode(base::as_bytes(base::make_span(jpeg_data)), &status));
  EXPECT_EQ(VaapiImageDecodeStatus::kUnsupportedSubsampling, status);
  EXPECT_FALSE(Decoder()->GetScopedVASurface());
}

INSTANTIATE_TEST_SUITE_P(All,
                         VaapiJpegDecoderTest,
                         testing::ValuesIn(kVAImageTestCases),
                         vaapi_test_utils::TestParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         VaapiJpegDecoderWithDmaBufsTest,
                         testing::ValuesIn(kDmaBufTestCases),
                         vaapi_test_utils::TestParamToString);

}  // namespace media
