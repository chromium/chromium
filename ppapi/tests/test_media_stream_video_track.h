// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MEDIA_STREAM_VIDEO_TRACK_H_
#define PPAPI_TESTS_TEST_MEDIA_STREAM_VIDEO_TRACK_H_

#include <string>

#include "ppapi/cpp/media_stream_video_track.h"
#include "ppapi/tests/test_case.h"

class TestMediaStreamVideoTrack : public TestCase {
 public:
  explicit TestMediaStreamVideoTrack(TestingInstance* instance);
  virtual ~TestMediaStreamVideoTrack();

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  // Overrides.
  virtual void HandleMessage(const pp::Var& message_data);

  std::string TestCreate();
  std::string TestGetFrame();
  std::string TestConfigure();

  pp::MediaStreamVideoTrack video_track_;

  NestedEvent event_;
};

#endif  // PPAPI_TESTS_TEST_MEDIA_STREAM_VIDEO_TRACK_H_
