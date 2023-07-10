// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_UTILS_H_
#define MEDIA_GPU_VAAPI_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

class ScopedVAImage;

namespace vaapi_test_utils {

struct TestParam {
  const char* test_name;
  const char* filename;
};

std::string TestParamToString(
    const testing::TestParamInfo<TestParam>& param_info);

constexpr size_t kMaxNumberPlanes = std::size(VAImage().pitches);
static_assert(kMaxNumberPlanes <= 3u, "The number of planes should be <= 3");
static_assert(
    std::size(VAImage().pitches) == std::size(VAImage().offsets),
    "The number of VAImage pitches is not equal to the number of offsets");

// A structure to hold generic image decodes in planar format.
struct DecodedImage {
  virtual ~DecodedImage();

  uint32_t fourcc;
  uint32_t number_of_planes;  // Can not be greater than kMaxNumberPlanes.
  gfx::Size size;
  struct {
    raw_ptr<uint8_t> data;
    int stride;
  } planes[kMaxNumberPlanes];
};

// Takes a ScopedVAImage and returns a DecodedImage object that represents
// the same decoded result.
DecodedImage ScopedVAImageToDecodedImage(const ScopedVAImage* scoped_va_image);

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<DecodedImage> NativePixmapToDecodedImage(
    gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    const gfx::BufferFormat& format);
#endif

// Compares the result of sw decoding |reference_image| with |hw_decoded_image|
// using SSIM. Returns true if all conversions work and SSIM is at least
// |min_ssim|, or false otherwise. Note that |reference_image| must be given in
// I420 format.
bool CompareImages(const DecodedImage& reference_image,
                   const DecodedImage& hw_decoded_image,
                   double min_ssim = 0.995);

}  // namespace vaapi_test_utils
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_UTILS_H_
