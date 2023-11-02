// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_VIDEO_ENCODER_H_
#define PPAPI_TESTS_TEST_VIDEO_ENCODER_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/tests/test_case.h"

class TestVideoEncoder : public TestCase {
 public:
  explicit TestVideoEncoder(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestAvailableCodecs();
  std::string TestIncorrectSizeFails();
  std::string TestInitializeVP8();
  std::string TestInitializeVP9();

  std::string TestInitializeCodec(PP_VideoProfile profile);

  // Used by the tests that access the C API directly.
  const PPB_VideoEncoder_0_1* video_encoder_interface_;
};

#endif  // PPAPI_TESTS_TEST_VIDEO_ENCODER_H_
