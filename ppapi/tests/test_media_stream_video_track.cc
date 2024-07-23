// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests PPB_MediaStreamVideoTrack interface.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_media_stream_video_track.h"

#include <stddef.h>
#include <stdint.h>

#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(MediaStreamVideoTrack);

namespace {

const int32_t kTimes = 3;
const int32_t kDefaultWidth = 640;
const int32_t kDefaultHeight = 480;
const char kJSCode[] =
    "function gotStream(stream) {"
    "  var track = stream.getVideoTracks()[0];"
    "  var plugin = document.getElementById('plugin');"
    "  plugin.postMessage(track);"
    "}"
    "var constraints = {"
    "  audio: false,"
    "  video: { mandatory: { minWidth: 640, minHeight: 480 } }"
    "};"
    "navigator.getUserMedia ="
    "    navigator.getUserMedia || navigator.webkitGetUserMedia;"
    "navigator.getUserMedia(constraints,"
    "    gotStream, function() {});";
}

TestMediaStreamVideoTrack::TestMediaStreamVideoTrack(TestingInstance* instance)
    : TestCase(instance),
      event_(instance_->pp_instance()) {
}

bool TestMediaStreamVideoTrack::Init() {
  return true;
}

TestMediaStreamVideoTrack::~TestMediaStreamVideoTrack() {
}

void TestMediaStreamVideoTrack::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST(GetFrame, filter);
  RUN_TEST(Configure, filter);
}

void TestMediaStreamVideoTrack::HandleMessage(const pp::Var& message) {
  if (message.is_resource())
    video_track_ = pp::MediaStreamVideoTrack(message.AsResource());
  event_.Signal();
}

std::string TestMediaStreamVideoTrack::TestCreate() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(video_track_.is_null());
  ASSERT_FALSE(video_track_.HasEnded());
  ASSERT_FALSE(video_track_.GetId().empty());

  // Close the track.
  video_track_.Close();
  ASSERT_TRUE(video_track_.HasEnded());
  video_track_ = pp::MediaStreamVideoTrack();
  PASS();
}

std::string TestMediaStreamVideoTrack::TestGetFrame() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(video_track_.is_null());
  ASSERT_FALSE(video_track_.HasEnded());
  ASSERT_FALSE(video_track_.GetId().empty());

  PP_TimeDelta timestamp = 0.0;

  // Get |kTimes| frames.
  for (int i = 0; i < kTimes; ++i) {
    TestCompletionCallbackWithOutput<pp::VideoFrame> cc(
        instance_->pp_instance(), false);
    cc.WaitForResult(video_track_.GetFrame(cc.GetCallback()));
    ASSERT_EQ(PP_OK, cc.result());
    pp::VideoFrame frame = cc.output();
    ASSERT_FALSE(frame.is_null());
    ASSERT_TRUE(frame.GetFormat() == PP_VIDEOFRAME_FORMAT_YV12 ||
                frame.GetFormat() == PP_VIDEOFRAME_FORMAT_I420);

    pp::Size size;
    ASSERT_TRUE(frame.GetSize(&size));
    ASSERT_EQ(size.width(), kDefaultWidth);
    ASSERT_EQ(size.height(), kDefaultHeight);

    ASSERT_GE(frame.GetTimestamp(), timestamp);
    timestamp = frame.GetTimestamp();

    ASSERT_GT(frame.GetDataBufferSize(), 0U);
    ASSERT_TRUE(frame.GetDataBuffer() != NULL);

    video_track_.RecycleFrame(frame);

    // A recycled frame should be invalidated.
    ASSERT_EQ(frame.GetFormat(), PP_VIDEOFRAME_FORMAT_UNKNOWN);
    ASSERT_FALSE(frame.GetSize(&size));
    ASSERT_EQ(frame.GetDataBufferSize(), 0U);
    ASSERT_TRUE(frame.GetDataBuffer() == NULL);
  }

  // Close the track.
  video_track_.Close();
  ASSERT_TRUE(video_track_.HasEnded());
  video_track_ = pp::MediaStreamVideoTrack();
  PASS();
}

std::string TestMediaStreamVideoTrack::TestConfigure() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(video_track_.is_null());
  ASSERT_FALSE(video_track_.HasEnded());
  ASSERT_FALSE(video_track_.GetId().empty());

  // Configure format.
  struct {
    int32_t format;
    int32_t expected_format;
  } formats[] = {
    { PP_VIDEOFRAME_FORMAT_BGRA, PP_VIDEOFRAME_FORMAT_BGRA }, // To RGBA.
    { PP_VIDEOFRAME_FORMAT_I420, PP_VIDEOFRAME_FORMAT_I420 }, // To I420.
    { PP_VIDEOFRAME_FORMAT_YV12, PP_VIDEOFRAME_FORMAT_YV12 }, // To YV12.
    { PP_VIDEOFRAME_FORMAT_BGRA, PP_VIDEOFRAME_FORMAT_BGRA }, // To RGBA.
    { PP_VIDEOFRAME_FORMAT_UNKNOWN, PP_VIDEOFRAME_FORMAT_YV12 }, // To default.
  };
  for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
    TestCompletionCallback cc1(instance_->pp_instance(), false);
    int32_t attrib_list[] = {
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT, formats[i].format,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE,
    };
    cc1.WaitForResult(video_track_.Configure(attrib_list, cc1.GetCallback()));
    ASSERT_EQ(PP_OK, cc1.result());

    for (int j = 0; j < kTimes; ++j) {
      TestCompletionCallbackWithOutput<pp::VideoFrame> cc2(
          instance_->pp_instance(), false);
      cc2.WaitForResult(video_track_.GetFrame(cc2.GetCallback()));
      ASSERT_EQ(PP_OK, cc2.result());
      pp::VideoFrame frame = cc2.output();
      ASSERT_FALSE(frame.is_null());
      if (formats[i].format != PP_VIDEOFRAME_FORMAT_UNKNOWN) {
        ASSERT_EQ(frame.GetFormat(), formats[i].expected_format);
      } else {
        // Both YV12 and I420 are acceptable as default YUV formats.
        ASSERT_TRUE(frame.GetFormat() == PP_VIDEOFRAME_FORMAT_YV12 ||
                    frame.GetFormat() == PP_VIDEOFRAME_FORMAT_I420);
      }

      pp::Size size;
      ASSERT_TRUE(frame.GetSize(&size));
      ASSERT_EQ(size.width(), kDefaultWidth);
      ASSERT_EQ(size.height(), kDefaultHeight);

      ASSERT_GT(frame.GetDataBufferSize(), 0U);
      ASSERT_TRUE(frame.GetDataBuffer() != NULL);

      video_track_.RecycleFrame(frame);
    }
  }

  // Configure size.
  struct {
    int32_t width;
    int32_t height;
    int32_t expect_width;
    int32_t expect_height;
  } sizes[] = {
    { 72, 72, 72, 72 }, // To 72x27.
    { 1024, 768, 1024, 768 }, // To 1024x768.
    { 0, 0, kDefaultWidth, kDefaultHeight }, // To default.
  };
  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
    TestCompletionCallback cc1(instance_->pp_instance(), false);
    int32_t attrib_list[] = {
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH, sizes[i].width,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT, sizes[i].height,
      PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE,
    };
    cc1.WaitForResult(video_track_.Configure(attrib_list, cc1.GetCallback()));
    ASSERT_EQ(PP_OK, cc1.result());

    for (int j = 0; j < kTimes; ++j) {
      TestCompletionCallbackWithOutput<pp::VideoFrame> cc2(
          instance_->pp_instance(), false);
      cc2.WaitForResult(video_track_.GetFrame(cc2.GetCallback()));
      ASSERT_EQ(PP_OK, cc2.result());
      pp::VideoFrame frame = cc2.output();
      ASSERT_FALSE(frame.is_null());
      ASSERT_TRUE(frame.GetFormat() == PP_VIDEOFRAME_FORMAT_YV12 ||
                  frame.GetFormat() == PP_VIDEOFRAME_FORMAT_I420);

      pp::Size size;
      ASSERT_TRUE(frame.GetSize(&size));
      ASSERT_EQ(size.width(), sizes[i].expect_width);
      ASSERT_EQ(size.height(), sizes[i].expect_height);

      ASSERT_GT(frame.GetDataBufferSize(), 0U);
      ASSERT_TRUE(frame.GetDataBuffer() != NULL);

      video_track_.RecycleFrame(frame);
    }
  }

  // Close the track.
  video_track_.Close();
  ASSERT_TRUE(video_track_.HasEnded());
  video_track_ = pp::MediaStreamVideoTrack();
  PASS();
}
