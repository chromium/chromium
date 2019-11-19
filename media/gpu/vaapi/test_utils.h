// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_UTILS_H_
#define MEDIA_GPU_VAAPI_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/stl_util.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class ScopedVAImage;

namespace vaapi_test_utils {

struct TestParam {
  const char* test_name;
  const char* filename;
};

std::string TestParamToString(
    const testing::TestParamInfo<TestParam>& param_info);

constexpr size_t kMaxNumberPlanes = base::size(VAImage().pitches);
static_assert(kMaxNumberPlanes <= 3u, "The number of planes should be <= 3");
static_assert(
    base::size(VAImage().pitches) == base::size(VAImage().offsets),
    "The number of VAImage pitches is not equal to the number of offsets");

// A structure to hold generic image decodes in planar format.
struct DecodedImage {
  uint32_t fourcc;
  uint32_t number_of_planes;  // Can not be greater than kMaxNumberPlanes.
  gfx::Size size;
  struct {
    uint8_t* data;
    int stride;
  } planes[kMaxNumberPlanes];
};

// Takes a ScopedVAImage and returns a DecodedImage object that represents
// the same decoded result.
DecodedImage ScopedVAImageToDecodedImage(const ScopedVAImage* scoped_va_image);

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
