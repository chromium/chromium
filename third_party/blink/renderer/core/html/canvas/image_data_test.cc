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
  EXPECT_EQ(data_length, data_u8->length());
  NotShared<DOMUint16Array> data_u16(
      DOMUint16Array::Create(u16_pixels, data_length));
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  NotShared<DOMFloat32Array> data_f32(
      DOMFloat32Array::Create(f32_pixels, data_length));
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->length());

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
