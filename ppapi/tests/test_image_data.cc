// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_image_data.h"

#include <stdint.h>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(ImageData);

bool TestImageData::Init() {
  image_data_interface_ = static_cast<const PPB_ImageData*>(
      pp::Module::Get()->GetBrowserInterface(PPB_IMAGEDATA_INTERFACE));
  return !!image_data_interface_;
}

void TestImageData::RunTests(const std::string& filter) {
  RUN_TEST(InvalidFormat, filter);
  RUN_TEST(GetNativeFormat, filter);
  RUN_TEST(FormatSupported, filter);
  RUN_TEST(InvalidSize, filter);
  RUN_TEST(HugeSize, filter);
  RUN_TEST(InitToZero, filter);
  RUN_TEST(IsImageData, filter);
}

std::string TestImageData::TestInvalidFormat() {
  pp::ImageData a(instance_, static_cast<PP_ImageDataFormat>(1337),
                  pp::Size(16, 16), true);
  if (!a.is_null())
    return "Crazy image data format accepted";

  pp::ImageData b(instance_, static_cast<PP_ImageDataFormat>(-1),
                  pp::Size(16, 16), true);
  if (!b.is_null())
    return "Negative image data format accepted";
  PASS();
}

std::string TestImageData::SubTestFormatSupported(PP_ImageDataFormat format) {
  if (!pp::ImageData::IsImageDataFormatSupported(format))
    return "ImageData::IsImageDataFormatSupported(format) returned false";
  PASS();
}

std::string TestImageData::TestFormatSupported() {
  ASSERT_SUBTEST_SUCCESS(SubTestFormatSupported(
      PP_IMAGEDATAFORMAT_BGRA_PREMUL));
  ASSERT_SUBTEST_SUCCESS(SubTestFormatSupported(
      PP_IMAGEDATAFORMAT_RGBA_PREMUL));
  PASS();
}

std::string TestImageData::TestGetNativeFormat() {
  PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
  if (!pp::ImageData::IsImageDataFormatSupported(format))
    return "ImageData::GetNativeImageDataFormat() returned unsupported format";
  PASS();
}

std::string TestImageData::SubTestInvalidSize(PP_ImageDataFormat format) {
  pp::ImageData zero_size(instance_, format, pp::Size(0, 0), true);
  if (!zero_size.is_null())
    return "Zero width and height accepted";

  pp::ImageData zero_height(instance_, format, pp::Size(16, 0), true);
  if (!zero_height.is_null())
    return "Zero height accepted";

  pp::ImageData zero_width(instance_, format, pp::Size(0, 16), true);
  if (!zero_width.is_null())
    return "Zero width accepted";

  PP_Size negative_height;
  negative_height.width = 16;
  negative_height.height = -2;
  PP_Resource rsrc = image_data_interface_->Create(
      instance_->pp_instance(),
      format,
      &negative_height, PP_TRUE);
  if (rsrc)
    return "Negative height accepted";

  PP_Size negative_width;
  negative_width.width = -2;
  negative_width.height = 16;
  rsrc = image_data_interface_->Create(
      instance_->pp_instance(),
      format,
      &negative_width, PP_TRUE);
  if (rsrc)
    return "Negative width accepted";

  PASS();
}

std::string TestImageData::TestInvalidSize() {
  ASSERT_SUBTEST_SUCCESS(SubTestInvalidSize(PP_IMAGEDATAFORMAT_BGRA_PREMUL));
  ASSERT_SUBTEST_SUCCESS(SubTestInvalidSize(PP_IMAGEDATAFORMAT_RGBA_PREMUL));
  PASS();
}

std::string TestImageData::SubTestHugeSize(PP_ImageDataFormat format) {
  pp::ImageData huge_size(instance_, format,
                          pp::Size(100000000, 100000000), true);
  if (!huge_size.is_null())
    return "31-bit overflow size accepted";
  PASS();
}

std::string TestImageData::TestHugeSize() {
  ASSERT_SUBTEST_SUCCESS(SubTestHugeSize(PP_IMAGEDATAFORMAT_BGRA_PREMUL));
  ASSERT_SUBTEST_SUCCESS(SubTestHugeSize(PP_IMAGEDATAFORMAT_RGBA_PREMUL));
  PASS();
}

std::string TestImageData::SubTestInitToZero(PP_ImageDataFormat format) {
  const int w = 5;
  const int h = 6;
  pp::ImageData img(instance_, format, pp::Size(w, h), true);
  if (img.is_null())
    return "Could not create bitmap";

  // Basic validity checking of the bitmap. This also tests "describe" since
  // that's where the image data object got its info from.
  if (img.size().width() != w || img.size().height() != h)
    return "Wrong size";
  if (img.format() != format)
    return "Wrong format";
  if (img.stride() < w * 4)
    return "Stride too small";

  // Now check that everything is 0.
  for (int y = 0; y < h; y++) {
    uint32_t* row = img.GetAddr32(pp::Point(0, y));
    for (int x = 0; x < w; x++) {
      if (row[x] != 0)
        return "Image data isn't entirely zero";
    }
  }

  PASS();
}

std::string TestImageData::TestInitToZero() {
  ASSERT_SUBTEST_SUCCESS(SubTestInitToZero(PP_IMAGEDATAFORMAT_BGRA_PREMUL));
  ASSERT_SUBTEST_SUCCESS(SubTestInitToZero(PP_IMAGEDATAFORMAT_RGBA_PREMUL));
  PASS();
}

std::string TestImageData::SubTestIsImageData(PP_ImageDataFormat format) {
  // Make a valid image resource.
  const int w = 16, h = 16;
  pp::ImageData img(instance_, format, pp::Size(w, h), true);
  if (img.is_null())
    return "Couldn't create image data";
  if (!image_data_interface_->IsImageData(img.pp_resource()))
    return "Image data should be identified as an image";
  PASS();
}

std::string TestImageData::TestIsImageData() {
  // Test that a NULL resource isn't an image data.
  pp::Resource null_resource;
  if (image_data_interface_->IsImageData(null_resource.pp_resource()))
    return "Null resource was reported as a valid image";

  // Make another resource type and test it.
  const int w = 16, h = 16;
  pp::Graphics2D device(instance_, pp::Size(w, h), true);
  if (device.is_null())
    return "Couldn't create device context";
  if (image_data_interface_->IsImageData(device.pp_resource()))
    return "Device context was reported as an image";

  ASSERT_SUBTEST_SUCCESS(SubTestIsImageData(PP_IMAGEDATAFORMAT_BGRA_PREMUL));
  ASSERT_SUBTEST_SUCCESS(SubTestIsImageData(PP_IMAGEDATAFORMAT_RGBA_PREMUL));
  PASS();
}
