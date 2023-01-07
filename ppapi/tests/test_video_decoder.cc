// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_decoder.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/video_decoder.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/tests/testing_instance.h"

namespace {

// The maximum number of pictures that the client can pass in for
// min_picture_count, just as a sanity check on the argument.
// This should match the value of kMaximumPictureCount in
// video_decoder_resource.cc.
const uint32_t kMaximumPictureCount = 100;

}  // namespace

REGISTER_TEST_CASE(VideoDecoder);

bool TestVideoDecoder::Init() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {PP_GRAPHICS3DATTRIB_WIDTH, width,
                             PP_GRAPHICS3DATTRIB_HEIGHT, height,
                             PP_GRAPHICS3DATTRIB_NONE};
  graphics_3d_ = pp::Graphics3D(instance_, attribs);
  return (pp::Module::Get()->GetBrowserInterface(PPB_VIDEODECODER_INTERFACE) &&
          CheckTestingInterface());
}

void TestVideoDecoder::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestVideoDecoder, Create, filter);
}

std::string TestVideoDecoder::TestCreate() {
  // Test that Initialize fails with a bad Graphics3D resource.
  {
    pp::VideoDecoder video_decoder(instance_);
    ASSERT_FALSE(video_decoder.is_null());

    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    pp::Graphics3D null_graphics_3d;
    callback.WaitForResult(
        video_decoder.Initialize(null_graphics_3d,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 0,
                                 callback.GetCallback()));
    ASSERT_EQ(PP_ERROR_BADRESOURCE, callback.result());
  }
  // Test that Initialize fails with a bad profile enum value.
  {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    const PP_VideoProfile kInvalidProfile = static_cast<PP_VideoProfile>(-1);
    callback.WaitForResult(
        video_decoder.Initialize(graphics_3d_,
                                 kInvalidProfile,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 0,
                                 callback.GetCallback()));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  }
  // Test that Initialize succeeds if we can create a Graphics3D resources and
  // if we allow software fallback to VP8, which should always be supported.
  if (!graphics_3d_.is_null()) {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        video_decoder.Initialize(graphics_3d_,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 0,
                                 callback.GetCallback()));
    ASSERT_EQ(PP_OK, callback.result());
  }
  // Test that Initialize succeeds with a larger than normal number of requested
  // picture buffers, if we can create a Graphics3D resource and if we allow
  // software fallback to VP8, which should always be supported.
  if (!graphics_3d_.is_null()) {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        video_decoder.Initialize(graphics_3d_,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 kMaximumPictureCount,
                                 callback.GetCallback()));
    ASSERT_EQ(PP_OK, callback.result());
  }
  // Test that Initialize fails if we request an unreasonable number of picture
  // buffers.
  if (!graphics_3d_.is_null()) {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        video_decoder.Initialize(graphics_3d_,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 kMaximumPictureCount + 1,
                                 callback.GetCallback()));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  }

  PASS();
}
