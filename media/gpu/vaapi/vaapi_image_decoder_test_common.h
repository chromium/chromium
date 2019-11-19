// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_TEST_COMMON_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_TEST_COMMON_H_

#include <memory>
#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "media/gpu/vaapi/test_utils.h"

namespace base {
class FilePath;
}

namespace media {

class VaapiImageDecoder;

class VaapiImageDecoderTestCommon
    : public testing::TestWithParam<vaapi_test_utils::TestParam> {
 public:
  VaapiImageDecoderTestCommon(std::unique_ptr<VaapiImageDecoder> decoder);
  ~VaapiImageDecoderTestCommon();

  void SetUp() override;

  VaapiImageDecoder* Decoder() const { return decoder_.get(); }

  // Find the location of the specified test file. If a file with specified path
  // is not found, treat the file as being relative to the test file directory.
  // This is either a custom test data path provided by --test_data_path, or the
  // default test data path (//media/test/data).
  base::FilePath FindTestDataFilePath(const std::string& file_name) const;

 private:
  std::string test_data_path_;
  std::unique_ptr<VaapiImageDecoder> decoder_;
};

}  // namespace media

#endif  //  MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_TEST_COMMON_H_
