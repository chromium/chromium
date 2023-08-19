// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <memory>
#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/test/local_gpu_memory_buffer_manager.h"
#include "media/gpu/vaapi/test_utils.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_image_decoder_test_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_webp_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/webp_parser.h"
#include "third_party/libwebp/src/src/webp/decode.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {
namespace {

constexpr const char* kSmallImageFilename =
    "RGB_noise_large_pixels_115x115.webp";
constexpr const char* kMediumImageFilename =
    "RGB_noise_large_pixels_2015x2015.webp";
constexpr const char* kLargeImageFilename =
    "RGB_noise_large_pixels_4000x4000.webp";

constexpr const char* kLowFreqImageFilename = "solid_green_2015x2015.webp";
constexpr const char* kMedFreqImageFilename =
    "BlackAndWhite_criss-cross_pattern_2015x2015.webp";
constexpr const char* kHighFreqImageFilename = "RGB_noise_2015x2015.webp";

const vaapi_test_utils::TestParam kTestCases[] = {
    {"SmallImage_115x115", kSmallImageFilename},
    {"MediumImage_2015x2015", kMediumImageFilename},
    {"LargeImage_4000x4000", kLargeImageFilename},
    {"LowFreqImage", kLowFreqImageFilename},
    {"MedFreqImage", kMedFreqImageFilename},
    {"HighFreqImage", kHighFreqImageFilename},
};

// Any number above 99.5% should do. We are being very aggressive here.
constexpr double kMinSsim = 0.999;

// WebpDecode*() returns memory that has to be released in a specific way.
struct WebpDecodeDeleter {
  void operator()(void* ptr) { WebPFree(ptr); }
};

}  // namespace

class VaapiWebPDecoderTest : public VaapiImageDecoderTestCommon {
 protected:
  VaapiWebPDecoderTest()
      : VaapiImageDecoderTestCommon(std::make_unique<VaapiWebPDecoder>()) {}

  void SetUp() override {
    if (!VaapiWrapper::IsDecodeSupported(VAProfileVP8Version0_3)) {
      DLOG(INFO) << "VP8 decoding is not supported by the VA-API.";
      GTEST_SKIP();
    }

    VaapiImageDecoderTestCommon::SetUp();
  }

  std::unique_ptr<NativePixmapAndSizeInfo> Decode(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status = nullptr) {
    const VaapiImageDecodeStatus decode_status =
        Decoder()->Decode(encoded_image);
    EXPECT_EQ(!!Decoder()->GetScopedVASurface(),
              decode_status == VaapiImageDecodeStatus::kSuccess);

    // Still try to export the surface when decode fails.
    VaapiImageDecodeStatus export_status;
    std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
        Decoder()->ExportAsNativePixmapDmaBuf(&export_status);
    EXPECT_EQ(!!exported_pixmap,
              export_status == VaapiImageDecodeStatus::kSuccess);

    // Return the first fail status.
    if (status) {
      *status = decode_status != VaapiImageDecodeStatus::kSuccess
                    ? decode_status
                    : export_status;
    }
    return exported_pixmap;
  }
};

TEST_P(VaapiWebPDecoderTest, DecodeAndExportAsNativePixmapDmaBuf) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string webp_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &webp_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::as_bytes(base::make_span(webp_data));

  // Decode the image using the VA-API and wrap the decoded image in a
  // DecodedImage object.
  ASSERT_TRUE(VaapiWrapper::IsDecodingSupportedForInternalFormat(
      VAProfileVP8Version0_3, VA_RT_FORMAT_YUV420));

  VaapiImageDecodeStatus status;
  std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
      Decode(encoded_image, &status);
  ASSERT_EQ(VaapiImageDecodeStatus::kSuccess, status);
  EXPECT_FALSE(Decoder()->GetScopedVASurface());
  ASSERT_TRUE(exported_pixmap);
  ASSERT_TRUE(exported_pixmap->pixmap);
  ASSERT_EQ(gfx::BufferFormat::YUV_420_BIPLANAR,
            exported_pixmap->pixmap->GetBufferFormat());

  // Make sure the visible area is contained by the surface.
  EXPECT_FALSE(exported_pixmap->va_surface_resolution.IsEmpty());
  EXPECT_FALSE(exported_pixmap->pixmap->GetBufferSize().IsEmpty());
  ASSERT_TRUE(
      gfx::Rect(exported_pixmap->va_surface_resolution)
          .Contains(gfx::Rect(exported_pixmap->pixmap->GetBufferSize())));

  // TODO(andrescj): we could get a better lower bound based on the dimensions
  // and the format.
  ASSERT_GT(exported_pixmap->byte_size, 0u);

  gfx::NativePixmapHandle handle = exported_pixmap->pixmap->ExportHandle();
  ASSERT_EQ(gfx::NumberOfPlanesForLinearBufferFormat(
                exported_pixmap->pixmap->GetBufferFormat()),
            handle.planes.size());

  std::unique_ptr<vaapi_test_utils::DecodedImage> hw_decoded_webp =
      vaapi_test_utils::NativePixmapToDecodedImage(
          handle, exported_pixmap->pixmap->GetBufferSize(),
          exported_pixmap->pixmap->GetBufferFormat());
  ASSERT_TRUE(hw_decoded_webp);

  // Decode the image using libwebp and wrap the decoded image in a
  // DecodedImage object.
  std::unique_ptr<Vp8FrameHeader> parse_result = ParseWebPImage(encoded_image);
  ASSERT_TRUE(parse_result);

  int reference_width;
  int reference_height;
  int y_stride;
  int uv_stride;
  uint8_t* libwebp_u_plane = nullptr;
  uint8_t* libwebp_v_plane = nullptr;
  std::unique_ptr<uint8_t, WebpDecodeDeleter> libwebp_y_plane(
      WebPDecodeYUV(encoded_image.data(), encoded_image.size(),
                    &reference_width, &reference_height, &libwebp_u_plane,
                    &libwebp_v_plane, &y_stride, &uv_stride));
  ASSERT_TRUE(libwebp_y_plane && libwebp_u_plane && libwebp_v_plane);
  ASSERT_EQ(reference_width, base::strict_cast<int>(parse_result->width));
  ASSERT_EQ(reference_height, base::strict_cast<int>(parse_result->height));

  // Wrap the software decoded image in a DecodedImage object.
  vaapi_test_utils::DecodedImage sw_decoded_webp{};
  sw_decoded_webp.fourcc = VA_FOURCC_I420;
  sw_decoded_webp.number_of_planes = 3u;
  sw_decoded_webp.size = gfx::Size(reference_width, reference_height);
  sw_decoded_webp.planes[0].data = libwebp_y_plane.get();
  sw_decoded_webp.planes[0].stride = y_stride;
  sw_decoded_webp.planes[1].data = libwebp_u_plane;
  sw_decoded_webp.planes[1].stride = uv_stride;
  sw_decoded_webp.planes[2].data = libwebp_v_plane;
  sw_decoded_webp.planes[2].stride = uv_stride;

  EXPECT_TRUE(vaapi_test_utils::CompareImages(sw_decoded_webp, *hw_decoded_webp,
                                              kMinSsim));
}

// TODO(crbug.com/986073): expand test coverage. See
// vaapi_jpeg_decoder_unittest.cc as reference:
// cs.chromium.org/chromium/src/media/gpu/vaapi/vaapi_jpeg_decoder_unittest.cc
INSTANTIATE_TEST_SUITE_P(All,
                         VaapiWebPDecoderTest,
                         testing::ValuesIn(kTestCases),
                         vaapi_test_utils::TestParamToString);

}  // namespace media
