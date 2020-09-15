// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/image_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

namespace blink {
namespace {

class ImageDataTest : public testing::Test {};

// This test passes if it does not crash. If the required memory is not
// allocated to the ImageData, then an exception must raise.
TEST_F(ImageDataTest, CreateImageDataTooBig) {
  DummyExceptionStateForTesting exception_state;
  ImageData* too_big_image_data =
      ImageData::Create(32767, 32767, exception_state);
  if (!too_big_image_data) {
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kRangeError);
  }
}

// This test verifies the correct behavior of ImageData member function used
// to convert pixels data from canvas pixel format to image data storage
// format. This function is used in BaseRenderingContext2D::getImageData.
TEST_F(ImageDataTest,
       TestConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat) {
  // Source pixels in RGBA32
  unsigned char rgba32_pixels[] = {255, 0,   0,   255,  // Red
                                   0,   0,   0,   0,    // Transparent
                                   255, 192, 128, 64,   // Decreasing values
                                   93,  117, 205, 11};  // Random values
  const unsigned kNumColorComponents = 16;
  float f32_pixels[kNumColorComponents];
  for (unsigned i = 0; i < kNumColorComponents; i++)
    f32_pixels[i] = rgba32_pixels[i] / 255.0;

  const unsigned kNumPixels = kNumColorComponents / 4;

  // Source pixels in F16
  unsigned char f16_pixels[kNumColorComponents * 2];
  EXPECT_TRUE(skcms_Transform(f32_pixels, skcms_PixelFormat_RGBA_ffff,
                              skcms_AlphaFormat_Unpremul, nullptr, f16_pixels,
                              skcms_PixelFormat_RGBA_hhhh,
                              skcms_AlphaFormat_Unpremul, nullptr, 4));

  // Source pixels in U16
  uint16_t u16_pixels[kNumColorComponents];
  for (unsigned i = 0; i < kNumColorComponents; i++)
    u16_pixels[i] = f32_pixels[i] * 65535.0;

  // Creating ArrayBufferContents objects. We need two buffers for RGBA32 data
  // because
  // CanvasPixelFormat::kRGBA8->kUint8ClampedArrayStorageFormat
  // consumes the input data parameter.
  ArrayBufferContents contents_rgba32(kNumColorComponents, 1,
                                      ArrayBufferContents::kNotShared,
                                      ArrayBufferContents::kDontInitialize);
  std::memcpy(contents_rgba32.Data(), rgba32_pixels, kNumColorComponents);

  ArrayBufferContents contents_rgba32_2;
  contents_rgba32.CopyTo(contents_rgba32_2);

  ArrayBufferContents contents_f16(kNumColorComponents * 2, 1,
                                   ArrayBufferContents::kNotShared,
                                   ArrayBufferContents::kDontInitialize);
  std::memcpy(contents_f16.Data(), f16_pixels, kNumColorComponents * 2);

  // Testing CanvasPixelFormat::kRGBA8->
  // kUint8ClampedArrayStorageFormat
  NotShared<DOMArrayBufferView> data(
      ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
          contents_rgba32, CanvasPixelFormat::kRGBA8,
          kUint8ClampedArrayStorageFormat));
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint8Clamped);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), rgba32_pixels, kNumPixels, kPixelFormat_8888,
      kAlphaUnmultiplied, kNoUnpremulRoundTripTolerance);

  // Testing CanvasPixelFormat::kRGBA8->
  // kUint16ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_rgba32_2, CanvasPixelFormat::kRGBA8, kUint16ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint16);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), u16_pixels, kNumPixels, kPixelFormat_16161616,
      kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
  // Testing CanvasPixelFormat::kRGBA8 ->
  // kFloat32ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_rgba32_2, CanvasPixelFormat::kRGBA8, kFloat32ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), f32_pixels, kNumPixels, kPixelFormat_ffff,
      kAlphaUnmultiplied, kUnpremulRoundTripTolerance);

  // Testing CanvasPixelFormat::kF16 -> kUint8ClampedArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_f16, CanvasPixelFormat::kF16, kUint8ClampedArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint8Clamped);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), rgba32_pixels, kNumPixels, kPixelFormat_8888,
      kAlphaUnmultiplied, kNoUnpremulRoundTripTolerance);

  // Testing CanvasPixelFormat::kF16 -> kUint16ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_f16, CanvasPixelFormat::kF16, kUint16ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint16);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), u16_pixels, kNumPixels, kPixelFormat_16161616,
      kAlphaUnmultiplied, kUnpremulRoundTripTolerance);

  // Testing CanvasPixelFormat::kF16 -> kFloat32ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_f16, CanvasPixelFormat::kF16, kFloat32ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      data->BaseAddress(), f32_pixels, kNumPixels, kPixelFormat_ffff,
      kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
}

// This test verifies the correct behavior of ImageData member function used
// to convert image data from image data storage format to canvas pixel format.
// This function is used in BaseRenderingContext2D::putImageData.
TEST_F(ImageDataTest, TestGetImageDataInCanvasColorSettings) {
  unsigned num_image_data_color_spaces = 3;
  CanvasColorSpace image_data_color_spaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  unsigned num_canvas_color_settings = 3;
  CanvasColorSpace canvas_color_spaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };

  CanvasPixelFormat canvas_pixel_formats[] = {
      CanvasPixelFormat::kRGBA8, CanvasPixelFormat::kF16,
      CanvasPixelFormat::kF16,   CanvasPixelFormat::kF16,
  };

  // As cross checking the output of Skia color space covnersion API is not in
  // the scope of this unit test, we do turn-around tests here. To do so, we
  // create an ImageData with the selected color space and storage format,
  // convert it to the target canvas color space and pixel format by calling
  // ImageData::imageDataInCanvasColorSettings(), and then convert it back
  // to the source image data color space and Float32 storage format by calling
  // ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat().
  // We expect to get the same image data as we started with.

  // Source pixels in RGBA32, unpremul
  uint8_t u8_pixels[] = {255, 0,   0,   255,  // Red
                         0,   0,   0,   0,    // Transparent
                         255, 192, 128, 64,   // Decreasing values
                         93,  117, 205, 11};  // Random values
  size_t data_length = 16;

  uint16_t* u16_pixels = new uint16_t[data_length];
  for (size_t i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  float* f32_pixels = new float[data_length];
  for (size_t i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  NotShared<DOMUint8ClampedArray> data_u8(
      DOMUint8ClampedArray::Create(u8_pixels, data_length));
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->lengthAsSizeT());
  NotShared<DOMUint16Array> data_u16(
      DOMUint16Array::Create(u16_pixels, data_length));
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->lengthAsSizeT());
  NotShared<DOMFloat32Array> data_f32(
      DOMFloat32Array::Create(f32_pixels, data_length));
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->lengthAsSizeT());

  ImageData* image_data = nullptr;
  ImageDataColorSettings* color_settings = ImageDataColorSettings::Create();

  // At most two bytes are needed for output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 2]());
  std::unique_ptr<uint8_t[]> pixels_converted_in_image_data(
      new uint8_t[data_length * 2]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    color_settings->setColorSpace(
        ImageData::CanvasColorSpaceName(image_data_color_spaces[i]));

    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      NotShared<DOMArrayBufferView> data_array;
      switch (image_data_storage_formats[j]) {
        case kUint8ClampedArrayStorageFormat:
          data_array = data_u8;
          color_settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          data_array = data_u16;
          color_settings->setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          data_array = data_f32;
          color_settings->setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      image_data =
          ImageData::CreateForTest(IntSize(2, 2), data_array, color_settings);

      for (unsigned k = 0; k < num_canvas_color_settings; k++) {
        // Convert the original data used to create ImageData to the
        // canvas color space and canvas pixel format.
        EXPECT_TRUE(
            ColorCorrectionTestUtils::
                ConvertPixelsToColorSpaceAndPixelFormatForTest(
                    data_array->BaseAddress(), data_length,
                    image_data_color_spaces[i], image_data_storage_formats[j],
                    canvas_color_spaces[k], canvas_pixel_formats[k],
                    pixels_converted_manually, kPixelFormat_hhhh));

        // Convert the image data to the color settings of the canvas.
        EXPECT_TRUE(image_data->ImageDataInCanvasColorSettings(
            canvas_color_spaces[k], canvas_pixel_formats[k],
            pixels_converted_in_image_data.get(), kRGBAColorType));

        // Compare the converted pixels
        ColorCorrectionTestUtils::CompareColorCorrectedPixels(
            pixels_converted_manually.get(),
            pixels_converted_in_image_data.get(),
            static_cast<int>(image_data->Size().Area()),
            (canvas_pixel_formats[k] == CanvasPixelFormat::kRGBA8)
                ? kPixelFormat_8888
                : kPixelFormat_hhhh,
            kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
      }
    }
  }
  delete[] u16_pixels;
  delete[] f32_pixels;
}

// This test examines ImageData::Create(StaticBitmapImage)
TEST_F(ImageDataTest, TestCreateImageDataFromStaticBitmapImage) {
  const unsigned kNumColorComponents = 16;
  const unsigned kNumPixels = kNumColorComponents / 4;
  const unsigned kWidth = 2;
  const unsigned kHeight = 2;

  // Preparing source pixels
  uint8_t expected_u8_pixels_unpremul[] = {
      255, 0,   0,   255,  // Red
      0,   0,   0,   0,    // Transparent
      255, 192, 128, 64,   // Decreasing values
      93,  117, 205, 41};  // Random values

  uint8_t expected_u8_pixels_premul[kNumColorComponents];
  uint16_t expected_f16_pixels_unpremul[kNumColorComponents];
  uint16_t expected_f16_pixels_premul[kNumColorComponents];
  float expected_f32_pixels_unpremul[kNumColorComponents];
  float expected_f32_pixels_premul[kNumColorComponents];

  auto prepareSourcePixels = [&expected_u8_pixels_unpremul, &kNumPixels](
                                 auto buffer, bool is_premul,
                                 auto pixel_format) {
    EXPECT_TRUE(skcms_Transform(
        expected_u8_pixels_unpremul, skcms_PixelFormat_RGBA_8888,
        skcms_AlphaFormat_Unpremul, nullptr, buffer, pixel_format,
        is_premul ? skcms_AlphaFormat_PremulAsEncoded
                  : skcms_AlphaFormat_Unpremul,
        nullptr, kNumPixels));
  };

  prepareSourcePixels(expected_u8_pixels_premul, true,
                      skcms_PixelFormat_RGBA_8888);
  prepareSourcePixels(expected_f16_pixels_unpremul, false,
                      skcms_PixelFormat_RGBA_hhhh);
  prepareSourcePixels(expected_f16_pixels_premul, true,
                      skcms_PixelFormat_RGBA_hhhh);
  prepareSourcePixels(expected_f32_pixels_unpremul, false,
                      skcms_PixelFormat_RGBA_ffff);
  prepareSourcePixels(expected_f32_pixels_premul, true,
                      skcms_PixelFormat_RGBA_ffff);

  auto contents_u8_premul =
      SkData::MakeWithoutCopy(expected_u8_pixels_premul, kNumColorComponents);
  auto contents_u8_unpremul =
      SkData::MakeWithoutCopy(expected_u8_pixels_unpremul, kNumColorComponents);
  auto contents_f16_premul = SkData::MakeWithoutCopy(expected_f16_pixels_premul,
                                                     kNumColorComponents * 2);
  auto contents_f16_unpremul = SkData::MakeWithoutCopy(
      expected_f16_pixels_unpremul, kNumColorComponents * 2);

  // Preparing StaticBitmapImage objects
  auto info_u8_premul = SkImageInfo::Make(
      kWidth, kHeight, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  auto info_u8_unpremul = info_u8_premul.makeAlphaType(kUnpremul_SkAlphaType);
  auto info_f16_premul = info_u8_premul.makeColorType(kRGBA_F16_SkColorType)
                             .makeColorSpace(SkColorSpace::MakeSRGBLinear());
  auto info_f16_unpremul = info_f16_premul.makeAlphaType(kUnpremul_SkAlphaType);

  auto image_u8_premul =
      StaticBitmapImage::Create(contents_u8_premul, info_u8_premul);
  auto image_u8_unpremul =
      StaticBitmapImage::Create(contents_u8_unpremul, info_u8_unpremul);
  auto image_f16_premul =
      StaticBitmapImage::Create(contents_f16_premul, info_f16_premul);
  auto image_f16_unpremul =
      StaticBitmapImage::Create(contents_f16_unpremul, info_f16_unpremul);

  // Creating ImageData objects
  ImageData* actual_image_data_u8[4];
  ImageData* actual_image_data_f32[4];
  // u8 premul
  actual_image_data_u8[0] = ImageData::Create(image_u8_premul);
  actual_image_data_u8[1] =
      ImageData::Create(image_u8_unpremul, kPremultiplyAlpha);
  // u8 unpremul
  actual_image_data_u8[2] = ImageData::Create(image_u8_unpremul);
  actual_image_data_u8[3] =
      ImageData::Create(image_u8_premul, kUnpremultiplyAlpha);

  // ImageData does not support half float storage. Therefore, ImageData
  // objects that are created from half-float backed StaticBitmapImage objects
  // will contain float 32 data items.

  // f32 premul
  actual_image_data_f32[0] = ImageData::Create(image_f16_premul);
  actual_image_data_f32[1] =
      ImageData::Create(image_f16_unpremul, kPremultiplyAlpha);
  // f32 unpremul
  actual_image_data_f32[2] = ImageData::Create(image_f16_unpremul);
  actual_image_data_f32[3] =
      ImageData::Create(image_f16_premul, kUnpremultiplyAlpha);

  // Associating expected color component arrays
  uint8_t* expected_pixel_arrays_u8[4] = {
      expected_u8_pixels_premul, expected_u8_pixels_premul,
      expected_u8_pixels_unpremul, expected_u8_pixels_unpremul};
  float* expected_pixel_arrays_f32[4] = {
      expected_f32_pixels_premul, expected_f32_pixels_premul,
      expected_f32_pixels_unpremul, expected_f32_pixels_unpremul};

  // Comparing ImageData with the source StaticBitmapImage
  for (unsigned i = 0; i < 4; i++) {
    ColorCorrectionTestUtils::CompareColorCorrectedPixels(
        expected_pixel_arrays_u8[i],
        actual_image_data_u8[i]->BufferBase()->Data(), kNumPixels,
        kPixelFormat_8888, kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
  }

  for (unsigned i = 0; i < 4; i++) {
    ColorCorrectionTestUtils::CompareColorCorrectedPixels(
        expected_pixel_arrays_f32[i],
        actual_image_data_f32[i]->BufferBase()->Data(), kNumPixels,
        kPixelFormat_ffff, kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
  }
}

// This test examines ImageData::CropRect()
TEST_F(ImageDataTest, TestCropRect) {
  const int num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };
  String image_data_storage_format_names[] = {
      kUint8ClampedArrayStorageFormatName, kUint16ArrayStorageFormatName,
      kFloat32ArrayStorageFormatName,
  };

  // Source pixels
  unsigned width = 20;
  unsigned height = 20;
  size_t data_length = width * height * 4;
  uint8_t* u8_pixels = new uint8_t[data_length];
  uint16_t* u16_pixels = new uint16_t[data_length];
  float* f32_pixels = new float[data_length];

  // Test scenarios
  const int num_test_cases = 14;
  const IntRect crop_rects[14] = {
      IntRect(3, 4, 5, 6),     IntRect(3, 4, 5, 6),    IntRect(10, 10, 20, 20),
      IntRect(10, 10, 20, 20), IntRect(0, 0, 20, 20),  IntRect(0, 0, 20, 20),
      IntRect(0, 0, 10, 10),   IntRect(0, 0, 10, 10),  IntRect(0, 0, 10, 0),
      IntRect(0, 0, 0, 10),    IntRect(10, 0, 10, 10), IntRect(0, 10, 10, 10),
      IntRect(0, 5, 20, 15),   IntRect(0, 5, 20, 15)};
  const bool crop_flips[14] = {true,  false, true,  false, true,  false, true,
                               false, false, false, false, false, true,  false};

  // Fill the pixels with numbers related to their positions
  unsigned set_value = 0;
  unsigned expected_value = 0;
  float fexpected_value = 0;
  unsigned index = 0, row_index = 0;
  for (unsigned i = 0; i < height; i++)
    for (unsigned j = 0; j < width; j++)
      for (unsigned k = 0; k < 4; k++) {
        index = i * width * 4 + j * 4 + k;
        set_value = (i + 1) * (j + 1) * (k + 1);
        u8_pixels[index] = set_value % 255;
        u16_pixels[index] = (set_value * 257) % 65535;
        f32_pixels[index] = (set_value % 255) / 255.0f;
      }

  // Create ImageData objects

  NotShared<DOMUint8ClampedArray> data_u8(
      DOMUint8ClampedArray::Create(u8_pixels, data_length));
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->lengthAsSizeT());
  NotShared<DOMUint16Array> data_u16(
      DOMUint16Array::Create(u16_pixels, data_length));
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->lengthAsSizeT());
  NotShared<DOMFloat32Array> data_f32(
      DOMFloat32Array::Create(f32_pixels, data_length));
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->lengthAsSizeT());

  ImageData* image_data = nullptr;
  ImageData* cropped_image_data = nullptr;

  bool test_passed = true;
  for (int i = 0; i < num_image_data_storage_formats; i++) {
    NotShared<DOMArrayBufferView> data_array;
    if (image_data_storage_formats[i] == kUint8ClampedArrayStorageFormat)
      data_array = data_u8;
    else if (image_data_storage_formats[i] == kUint16ArrayStorageFormat)
      data_array = data_u16;
    else
      data_array = data_f32;

    ImageDataColorSettings* color_settings = ImageDataColorSettings::Create();
    color_settings->setStorageFormat(image_data_storage_format_names[i]);
    image_data = ImageData::CreateForTest(IntSize(width, height), data_array,
                                          color_settings);
    for (int j = 0; j < num_test_cases; j++) {
      // Test the size of the cropped image data
      IntRect src_rect(IntPoint(), image_data->Size());
      IntRect crop_rect = Intersection(src_rect, crop_rects[j]);

      cropped_image_data = image_data->CropRect(crop_rects[j], crop_flips[j]);
      if (crop_rect.IsEmpty()) {
        EXPECT_FALSE(cropped_image_data);
        continue;
      }
      EXPECT_TRUE(cropped_image_data->Size() == crop_rect.Size());

      // Test the content
      for (int k = 0; k < crop_rect.Height(); k++)
        for (int m = 0; m < crop_rect.Width(); m++)
          for (int n = 0; n < 4; n++) {
            row_index = crop_flips[j] ? (crop_rect.Height() - k - 1) : k;
            index =
                row_index * cropped_image_data->Size().Width() * 4 + m * 4 + n;
            expected_value =
                (k + crop_rect.Y() + 1) * (m + crop_rect.X() + 1) * (n + 1);
            if (image_data_storage_formats[i] ==
                kUint8ClampedArrayStorageFormat)
              expected_value %= 255;
            else if (image_data_storage_formats[i] == kUint16ArrayStorageFormat)
              expected_value = (expected_value * 257) % 65535;
            else
              fexpected_value = (expected_value % 255) / 255.0f;

            if (image_data_storage_formats[i] ==
                kUint8ClampedArrayStorageFormat) {
              if (cropped_image_data->data()
                      .GetAsUint8ClampedArray()
                      ->Data()[index] != expected_value) {
                test_passed = false;
                break;
              }
            } else if (image_data_storage_formats[i] ==
                       kUint16ArrayStorageFormat) {
              if (cropped_image_data->data()
                      .GetAsUint16Array()
                      .View()
                      ->Data()[index] != expected_value) {
                test_passed = false;
                break;
              }
            } else {
              if (cropped_image_data->data()
                      .GetAsFloat32Array()
                      .View()
                      ->Data()[index] != fexpected_value) {
                test_passed = false;
                break;
              }
            }
          }
      EXPECT_TRUE(test_passed);
    }
  }

  delete[] u8_pixels;
  delete[] u16_pixels;
  delete[] f32_pixels;
}

TEST_F(ImageDataTest, ImageDataTooBigToAllocateDoesNotCrash) {
  ImageData* image_data = ImageData::CreateForTest(
      IntSize(1, (v8::TypedArray::kMaxLength / 4) + 1));
  EXPECT_EQ(image_data, nullptr);
}

}  // namespace
}  // namespace blink
