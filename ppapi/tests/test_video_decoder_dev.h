// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_VIDEO_DECODER_DEV_H_
#define PPAPI_TESTS_TEST_VIDEO_DECODER_DEV_H_

#include <string>

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

class TestVideoDecoderDev : public TestCase {
 public:
  explicit TestVideoDecoderDev(TestingInstance* instance)
      : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void QuitMessageLoop();

 private:
  std::string TestCreateFailure();

  // Used by the tests that access the C API directly.
  const PPB_VideoDecoder_Dev* video_decoder_interface_;
};

#endif  // PPAPI_TESTS_TEST_VIDEO_DECODER_DEV_H_
