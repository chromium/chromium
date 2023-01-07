// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_decoder_dev.h"

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VideoDecoderDev);

bool TestVideoDecoderDev::Init() {
  video_decoder_interface_ = static_cast<const PPB_VideoDecoder_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VIDEODECODER_DEV_INTERFACE));
  return video_decoder_interface_ && CheckTestingInterface();
}

void TestVideoDecoderDev::RunTests(const std::string& filter) {
  RUN_TEST(CreateFailure, filter);
}

void TestVideoDecoderDev::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestVideoDecoderDev::TestCreateFailure() {
  PP_Resource decoder = video_decoder_interface_->Create(
      instance_->pp_instance(), 0, static_cast<PP_VideoDecoder_Profile>(-1));
  if (decoder != 0)
    return "Create: error detecting invalid context & configs";

  PASS();
}
