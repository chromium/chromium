// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MEDIA_STREAM_AUDIO_TRACK_H_
#define PPAPI_TESTS_TEST_MEDIA_STREAM_AUDIO_TRACK_H_

#include <stdint.h>

#include <string>

#include "ppapi/cpp/media_stream_audio_track.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/tests/test_case.h"

class TestMediaStreamAudioTrack : public TestCase {
 public:
  explicit TestMediaStreamAudioTrack(TestingInstance* instance);
  virtual ~TestMediaStreamAudioTrack();

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  // Overrides.
  virtual void HandleMessage(const pp::Var& message_data);

  // Test helpers.
  std::string CheckConfigure(int32_t attrib_list[], int32_t expected_result);
  std::string CheckGetBuffer(int times, int expected_duration);

  std::string TestCreate();
  std::string TestGetBuffer();
  std::string TestConfigure();
  std::string TestConfigureClose();
  std::string TestVerifyWaveform();

  pp::MediaStreamAudioTrack audio_track_;

  NestedEvent event_;
};

#endif  // PPAPI_TESTS_TEST_MEDIA_STREAM_AUDIO_TRACK_H_
