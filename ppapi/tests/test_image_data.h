// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_IMAGE_DATA_H_
#define PPAPI_TESTS_TEST_IMAGE_DATA_H_

#include <string>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/tests/test_case.h"

class TestImageData : public TestCase {
 public:
  TestImageData(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestInvalidFormat();
  std::string TestGetNativeFormat();
  std::string TestFormatSupported();
  std::string TestInvalidSize();
  std::string TestHugeSize();
  std::string TestInitToZero();
  std::string TestIsImageData();

  // Subtests used by tests above; pass in a pixel format.
  std::string SubTestFormatSupported(PP_ImageDataFormat format);
  std::string SubTestInvalidSize(PP_ImageDataFormat format);
  std::string SubTestHugeSize(PP_ImageDataFormat format);
  std::string SubTestInitToZero(PP_ImageDataFormat format);
  std::string SubTestIsImageData(PP_ImageDataFormat format);

  // Used by the tests that access the C API directly.
  const PPB_ImageData* image_data_interface_;
};

#endif  // PPAPI_TESTS_TEST_IMAGE_DATA_H_
